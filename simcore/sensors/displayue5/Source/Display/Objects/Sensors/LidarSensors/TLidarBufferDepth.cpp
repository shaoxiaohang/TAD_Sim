#include "TLidarBufferDepth.h"

#include "Components/SceneCaptureComponent2D.h"
#include "DepthCamera.h"
#include "DepthLidarSVE.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Framework/DisplayGameInstance.h"
#include "Framework/SaveDataThread.h"
#include "Kismet/KismetMathLibrary.h"
#include "RHIGPUReadback.h"
#include "RenderGraphEvent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Runtime/ImageWrapper/Public/IImageWrapperModule.h"
#include "Utils/FRDGBuilderHelper.h"
#include "Utils/ProjectionUtil.h"
#include "WorldXShaders/Private/DepthBasedLidarCSInstance.h"
#include "lidar/LidarModel.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

bool ALidarBufferDepth::Init(const FLidarConfig& _config, std::shared_ptr<lidar::TraditionalLidar> _LidarSensor,
    AActor* _actor, class LidarModel* lmodel)
{
    LidarBufferFun::Init(_config, _LidarSensor, _actor, lmodel);

    FString DepthImageN;
    if (GConfig->GetString(TEXT("Sensor"), TEXT("LidarDN"), DepthImageN, GGameIni))
    {
        UE_LOG(LogTemp, Log, TEXT("SensorManger: DepthImageN is: %s. "), *DepthImageN);
        CameraCount = FMath::Min(100, FCString::Atoi(*DepthImageN));
    }

    FString single_capture_str;
    GConfig->GetString(TEXT("Sensor"), TEXT("LidarSingleCapture"), single_capture_str, GGameIni);
    if (single_capture_str == TEXT("false"))
    {
        bUseSingleCapture = false;
    }

    FString extra_info_str;
    GConfig->GetString(TEXT("Sensor"), TEXT("LidarExtraInfo"), extra_info_str, GGameIni);
    if (extra_info_str == TEXT("true"))
    {
        bOutputExtraInfo = true;
    }

    FString degree_h_pixel;
    if (GConfig->GetString(TEXT("Sensor"), TEXT("DegreeHorizonPerPixel"), degree_h_pixel, GGameIni))
    {
        DegreeHorizonPerPixel = FMath::Min(0.1, FCString::Atof(*degree_h_pixel));
        UE_LOG(LogTemp, Log, TEXT("ALidarBufferDepth: DegreeHorizonPerPixel is: %f. "), DegreeHorizonPerPixel);
    }

    FString degree_v_pixel;
    if (GConfig->GetString(TEXT("Sensor"), TEXT("DegreeVerticalPerPixel"), degree_v_pixel, GGameIni))
    {
        DegreeVerticalPerPixel = FMath::Min(0.1, FCString::Atof(*degree_v_pixel));
        UE_LOG(LogTemp, Log, TEXT("ALidarBufferDepth: DegreeVerticalPerPixel is: %f. "), DegreeVerticalPerPixel);
    }

    SceneCaptureCount = bUseSingleCapture ? 1 : CameraCount;

    AzimuthRange = _LidarSensor->getAzimuthRange();
    ScanCount = _LidarSensor->getHorizontalScanCount();
    MaxPointNum = ScanCount * _LidarSensor->getRaysNum() * _LidarSensor->getReturnNum();
    Channels = _LidarSensor->getRaysNum();
    Range = _LidarSensor->getRange();

    FovRangeVerticalPerCamera.X = MAX_flt;
    FovRangeVerticalPerCamera.Y = -MAX_flt;
    for (uint32 i = 0; i < _LidarSensor->getHorizontalScanCount(); i++)
    {
        for (uint32 j = 0; j < _LidarSensor->getRaysNum(); j++)
        {
            auto yawpitch = _LidarSensor->getYawPitchAngle(i, j);
            FovRangeVerticalPerCamera.X = std::min(FovRangeVerticalPerCamera.X, yawpitch.second);
            FovRangeVerticalPerCamera.Y = std::max(FovRangeVerticalPerCamera.Y, yawpitch.second);
        }
    }
    FovRangeVerticalPerCamera.X -= 1;
    FovRangeVerticalPerCamera.Y += 1;

    MaxAzimuth = AzimuthRange.Y - AzimuthRange.X;

    FovHorizonPerCamera = MaxAzimuth / CameraCount;
    HorizonSampleCount = ScanCount / CameraCount;
    ImageWidthPerCamera = MaxAzimuth / DegreeHorizonPerPixel / CameraCount;
    ImageHeightPerCamera = (FovRangeVerticalPerCamera.Y - FovRangeVerticalPerCamera.X) / DegreeVerticalPerPixel;

    UE_LOG(LogTemp, Log,
        TEXT("CameraCount: %d FovHorizonPerCamera: %f FovRangeVerticalPerCamera.Bottom: %f "
             "FovRangeVerticalPerCamera.Top: %f HorizonSampleCount: %f ImageWidthPerCamera: %d ImageHeightPerCamera: "
             "%d  MaxPointNum %d AzimuthRange %f %f    "),
        SceneCaptureCount, FovHorizonPerCamera, FovRangeVerticalPerCamera.X, FovRangeVerticalPerCamera.Y,
        HorizonSampleCount, ImageWidthPerCamera, ImageHeightPerCamera, MaxPointNum, AzimuthRange.X, AzimuthRange.Y);

    LaserRays.SetNum(MaxPointNum);
    ImageSpaceLaserRays.SetNum(MaxPointNum);
    ScanAzimuth.SetNum(ScanCount);
    ScanSequenceCount = 0;

    PrepareParallelRays(_LidarSensor);

    CreateSenceCaptureComponents();

    CreateTextureRenderTargets();

    SetupComponents();

    SetupBuffers();

    UE_LOG(LogTemp, Log, TEXT("DepthMapBasedLidar Initialize Done. ScanCount: %d Channel: %d MaxPointNum: %d"),
        ScanCount, Channels, MaxPointNum);

    return true;
}

TSharedPtr<LidarBuffer> ALidarBufferDepth::GetTBuffer(const FTLidarMeasurement& measure)
{
    TSharedPtr<DepthLidarBuffer> buffer = MakeShared<DepthLidarBuffer>();

    if (bUseSingleCapture)
    {
        ReadLidarData_RenderThreadSingleCaptureSVE(buffer);
    }
    else
    {
        ReadLidarData_RenderThreadSVE(buffer);
    }

    return buffer;
}

bool ALidarBufferDepth::ReadLidarData_RenderThreadSVE(TSharedPtr<DepthLidarBuffer> buffer)
{
    for (int i = 0; i < SceneCaptures.Num(); i++)
    {
        SceneCaptures[i]->CaptureScene();
    }

    ENQUEUE_RENDER_COMMAND(FComputeLidar_RenderThread)
    ([&](FRHICommandListImmediate& InRHICmdList) mutable {
        TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);
        check(IsInRenderingThread());

        InRHICmdList.BlockUntilGPUIdle();

        FetchReadbackBuffer(buffer);

        ScanSequenceCount += ScanCount;
    });

    FlushRenderingCommands();

    return true;
}

bool ALidarBufferDepth::ReadLidarData_RenderThreadSingleCaptureSVE(TSharedPtr<DepthLidarBuffer> buffer)
{
    SceneViewExtension->ResetParams();
    // push lidar graph
    for (int i = 0; i < CameraCount; i++)
    {
        if (bOutputExtraInfo)
        {
            ExtraSceneCaptures[0]->SetRelativeRotation(FQuat(FRotator(0, AzimuthRange.X + FovHorizonPerCamera * i, 0)));
            ExtraSceneCaptures[0]->CaptureScene();
        }
        SceneCaptures[0]->SetRelativeRotation(FQuat(FRotator(0, AzimuthRange.X + FovHorizonPerCamera * i, 0)));
        SceneCaptures[0]->CaptureScene();
    }

    ENQUEUE_RENDER_COMMAND(Lidar_Post)
    ([&](FRHICommandListImmediate& InRHICmdList) mutable {
        TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);
        check(IsInRenderingThread());

        InRHICmdList.BlockUntilGPUIdle();

        FetchReadbackBuffer(buffer);

        ScanSequenceCount += ScanCount;
    });
    FlushRenderingCommands();

    return true;
}

void ALidarBufferDepth::IgnoreActor(AActor* actor)
{
    if (actor)
    {
        for (auto& captureComponent : SceneCaptures)
        {
            captureComponent->HiddenActors.Add(actor);
        }
        if (bOutputExtraInfo)
        {
            for (auto& captureComponent : ExtraSceneCaptures)
            {
                captureComponent->HiddenActors.Add(actor);
            }
        }
    }
}

void ALidarBufferDepth::FetchReadbackBuffer(TSharedPtr<DepthLidarBuffer> buffer)
{
    check(IsInRenderingThread());

    if (LidarReadbackDetectionCount->IsReady() && LidarReadbackDetection->IsReady() &&
        // #if WITH_EDITORONLY_DATA
        //         (!bDebugLidar || DebugLidarReadbackRawLidar->IsReady()) &&
        //         (!bDebugLidar || DebugLidarReadbackLaserNumPerScan->IsReady()) &&
        //         (!bDebugLidar || DebugLidarReadbackScanOffset->IsReady()) &&
        // #endif
        LidarReadbackScan->IsReady())
    {
        int DetectionCount = -1;
        void* DetectionCountPtr = LidarReadbackDetectionCount->Lock(sizeof(int));
        FMemory::Memcpy(&DetectionCount, DetectionCountPtr, sizeof(int));
        LidarReadbackDetectionCount->Unlock();
        buffer->detection_count = DetectionCount;

        // #if WITH_EDITORONLY_DATA
        //         if (DetectionCount > 0 && bDebugLidar)
        //         {
        //             DebugDetectionCount = DetectionCount;
        //             UE_LOG(LogWorldX, Warning, TEXT("DebugDetectionCount: %d"), DebugDetectionCount);

        //             FMemory::Memcpy(DebugRawLidar.data(),
        //                 DebugLidarReadbackRawLidar->Lock(MaxPointNum * sizeof(LidarDetection)),
        //                 MaxPointNum * sizeof(LidarDetection));
        //             DebugLidarReadbackRawLidar->Unlock();

        //             FMemory::Memcpy(DebugLaserNumPerScan.GetData(),
        //                 DebugLidarReadbackLaserNumPerScan->Lock(ScanCount * sizeof(int)), ScanCount * sizeof(int));
        //             DebugLidarReadbackLaserNumPerScan->Unlock();

        //             FMemory::Memcpy(DebugScanOffset.GetData(), DebugLidarReadbackScanOffset->Lock(ScanCount *
        //             sizeof(int)),
        //                 ScanCount * sizeof(int));
        //             DebugLidarReadbackScanOffset->Unlock();
        //         }
        // #endif
        buffer->detections.resize(MaxPointNum);
        buffer->scans.resize(ScanCount);
        if (DetectionCount > 0)
        {
            void* DetectionBuffer = LidarReadbackDetection->Lock(MaxPointNum * sizeof(LidarDetection));
            FMemory::Memcpy(buffer->detections.data(), DetectionBuffer, MaxPointNum * sizeof(LidarDetection));
            LidarReadbackDetection->Unlock();
        }

        void* ScanBuffer = LidarReadbackScan->Lock(ScanCount * sizeof(LidarScan));
        buffer->scan_count = ScanCount;
        FMemory::Memcpy(buffer->scans.data(), ScanBuffer, ScanCount * sizeof(LidarScan));
        LidarReadbackScan->Unlock();
    }
}

bool ALidarBufferDepth::GetPoints(
    const LidarBuffer* rawbuf, const FTLidarMeasurement& measure, lidar::TraditionalLidar::lidar_ptset& lidarBuffer)
{
    const DepthLidarBuffer* buffer = StaticCast<const DepthLidarBuffer*>(rawbuf);
    uint32_t rn = lidarSensor->getRaysNum();
    uint32_t rtn = lidarSensor->getReturnNum();
    lidarBuffer.channels.resize(measure.HorizontalToScan);
    lidarBuffer.points.resize(measure.HorizontalToScan * rn * rtn);

    const auto& detections = buffer->detections;
    // for (int i = 0; i < 128; ++i)
    // {
    //     const auto& detection = detections[i];
    //     UE_LOG(
    //         LogTemp, Warning, TEXT("Detection %d: %f %f %f"), detection.channel, detection.x, detection.y,
    //         detection.z);
    // }

    double umpsec = 1000.0 * 1000. / (lidarSensor->getRotationFrequency() * lidarSensor->getHorizontalScanCount());
    auto utime = measure.TimeStamp0 * 1000;
    for (uint32 i = 0; i < measure.HorizontalToScan; i++)
    {
        auto& dd = lidarBuffer.channels.at(i);
        dd.hor_pos = (measure.HorizontalPos + i) % lidarSensor->getHorizontalScanCount();
        dd.utime = utime + umpsec * i;
        dd.pn = rn * rtn;
        dd.points = &lidarBuffer.points[i * rn * rtn];
        for (uint32 j = 0; j < rn; ++j)
        {
            const auto& p = buffer->detections[i * rn + j];
            auto& pt = dd.points[j];
            FVector point(p.x, p.y, p.z);
            float distance = point.Size();
            if (distance > 1e-6f && distance < 327.f)
            {
                pt.distance = distance;
                pt.tag_c = p.label;
                pt.norinter = p.norinter;
                pt.instensity = p.intensity;
                pt.x = p.x;
                pt.y = p.y;
                pt.z = p.z;
                // LIDAR 模型
                if (lidarMd)
                    lidarMd->simulator(pt.x, pt.y, pt.z, pt.distance, pt.norinter, pt.instensity);
            }
        }
    }

    return true;
}

void ALidarBufferDepth::PrepareParallelRays(std::shared_ptr<lidar::TraditionalLidar> _LidarSensor)
{
    float v_fov_top_tan = FMath::Tan(FMath::DegreesToRadians(FovRangeVerticalPerCamera.Y));
    float v_fov_bottom_tan = FMath::Abs(FMath::Tan(FMath::DegreesToRadians(FovRangeVerticalPerCamera.X)));
    float v_fov_offset = v_fov_top_tan / (v_fov_top_tan + v_fov_bottom_tan);

    float CenterWidth = ImageWidthPerCamera * 0.5;
    float CenterHeight = ImageHeightPerCamera * v_fov_offset;

    // units in pixel
    FocalLengthX = CenterWidth / FMath::Tan(FMath::DegreesToRadians(FovHorizonPerCamera) * 0.5);
    FocalLengthY = CenterHeight / FMath::Tan(FMath::DegreesToRadians(FovRangeVerticalPerCamera.Y));
    UE_LOG(LogTemp, Log, TEXT("FocalLengthX: %f FocalLengthY: %f"), FocalLengthX, FocalLengthY);

    TArray<std::atomic<int>> RayNumPerCamera;
    RayNumPerCamera.SetNum(CameraCount);
    FMemory::Memset(RayNumPerCamera.GetData(), 0, RayNumPerCamera.GetTypeSize() * RayNumPerCamera.Num());
    FMemory::Memset(ScanAzimuth.GetData(), 0, ScanAzimuth.GetTypeSize() * ScanAzimuth.Num());

    for (int index = 0; index < MaxPointNum; ++index)
    {
        uint32 ScanID = index / Channels;
        uint32 LaserID = index % Channels;
        auto yawpitch = _LidarSensor->getYawPitchAngle(ScanID, LaserID);
        float vertical_angle = yawpitch.second;
        // float azimuth = AzimuthRange.X + ScanID * _LidarSensor->getHorizontalResolution();
        // float horizontal_angle = azimuth + _LidarSensor->getHorizonOffset(LaserID);
        float azimuth = yawpitch.first;
        float horizontal_angle = yawpitch.first;


        // UE_LOG(LogTemp, Log, TEXT("yaw: %f pitch: %f"), yawpitch.first, yawpitch.second);

        // UE_LOG(LogTemp, Log, TEXT("horizontal_angle: %f azimuth: %f"), horizontal_angle, azimuth);

        horizontal_angle = FMath::Fmod(horizontal_angle + 60.0 + 120.0 , MaxAzimuth);
        int CameraID = horizontal_angle / FovHorizonPerCamera;

        // if (LaserID == 0)
        // {
        //     UE_LOG(LogTemp, Log, TEXT("vertical_angle: %f azimuth: %f CameraID: %d"), vertical_angle,
        //     horizontal_angle,
        //         CameraID);
        // }

        float camera_horizontal_angle_offset = -CameraID * FovHorizonPerCamera - FovHorizonPerCamera * 0.5;
        FVector3f LaserRay =
            FVector3f(FMath::Cos(FMath::DegreesToRadians(vertical_angle)) *
                          FMath::Cos(FMath::DegreesToRadians(horizontal_angle + camera_horizontal_angle_offset)),
                FMath::Cos(FMath::DegreesToRadians(vertical_angle)) *
                    FMath::Sin(FMath::DegreesToRadians(horizontal_angle + camera_horizontal_angle_offset)),
                FMath::Sin(FMath::DegreesToRadians(vertical_angle)));
        LaserRays[index] = LaserRay;

        FVector3f CameraToPixelDirection = FVector3f(1, LaserRay.Y / LaserRay.X, LaserRay.Z / LaserRay.X);
        int row = CenterWidth + LaserRay.Y / LaserRay.X * FocalLengthX;
        int column = CenterHeight - LaserRay.Z / LaserRay.X * FocalLengthY;

        if (row < 0 || row >= ImageWidthPerCamera || column < 0 || column >= ImageHeightPerCamera)
        {
            UE_LOG(LogTemp, Error,
                TEXT("index: %d row: %d column: %d ImageWidthPerCamera: %d ImageHeightPerCamera: %d CenterWidth: "
                     "%f CenterHeight: %f vertical_angle: %f LaserRay X %f LaserRay Y %f horizontal_angle: %f "
                     "vertical_angle: %f azimuth %f"),
                index, row, column, ImageWidthPerCamera, ImageHeightPerCamera, CenterWidth, CenterHeight,
                vertical_angle, LaserRay.X, LaserRay.Y, horizontal_angle, vertical_angle, azimuth);
        }

        int ImageSpaceIndex = RayNumPerCamera[CameraID].fetch_add(1);
        int GlobalImageSpaceIndex = ImageSpaceIndex + CameraID * HorizonSampleCount * Channels;
        ImageSpaceLaserRays[GlobalImageSpaceIndex].scan_id = ScanID;
        ImageSpaceLaserRays[GlobalImageSpaceIndex].laser_id = LaserID;
        ImageSpaceLaserRays[GlobalImageSpaceIndex].azimuth = azimuth;
        ImageSpaceLaserRays[GlobalImageSpaceIndex].row = FMath::Clamp(row, 0, ImageWidthPerCamera - 1);
        ImageSpaceLaserRays[GlobalImageSpaceIndex].column = FMath::Clamp(column, 0, ImageHeightPerCamera - 1);
        ImageSpaceLaserRays[GlobalImageSpaceIndex].direction = CameraToPixelDirection;

        if (LaserID == 0)
        {
            ScanAzimuth[ScanID] = azimuth;
        }
        // });
    }

    // for (int i = 0; i < ImageSpaceLaserRays.Num(); i++)
    // {
    //     if (ImageSpaceLaserRays[i].direction.Length() < 0.0001)
    //     {
    //         UE_LOG(LogTemp, Error, TEXT(" Error index: %d"), i);
    //     }
    // }

    // auto actorPos = actor->GetTransform().GetLocation();
    // UE_LOG(LogTemp, Log, TEXT("ActorPos: %f %f %f"), actorPos.X, actorPos.Y, actorPos.Z);

    // visualize
    // for (int i = 0; i < ImageSpaceLaserRays.Num(); i++)
    // {
    //     FVector WorldPos = FVector(
    //         ImageSpaceLaserRays[i].direction.X, ImageSpaceLaserRays[i].direction.Y,
    //         ImageSpaceLaserRays[i].direction.Z);

    //     // UE_LOG(LogTemp, Log, TEXT("RayPos: %f %f %f"), WorldPos.X, WorldPos.Y, WorldPos.Z);

    //     // auto ss = actor->GetTransform().GetLocation() +
    //     //         actor->GetTransform().TransformVector(WorldPos).GetUnsafeNormal() * 500;

    //     // UE_LOG(LogTemp, Log, TEXT("WorldPos: %f %f %f"), ss.X, ss.Y, ss.Z);

    //     DrawDebugPoint(actor->GetWorld(),
    //         actor->GetTransform().GetLocation() +
    //             actor->GetTransform().TransformVector(WorldPos).GetUnsafeNormal() * 500,
    //         2, FColor::Red, true);
    //     // DrawDebugLine(actor->GetWorld(),
    //     // 	actor->GetTransform().GetLocation() + actor->GetTransform().TransformVector(WorldPos).GetUnsafeNormal()
    //     // * 400, 	actor->GetTransform().GetLocation() +
    //     // actor->GetTransform().TransformVector(WorldPos).GetUnsafeNormal() * 500, 	FColor::Red, 	true);
    // }

    // TArray<int> RayStartOffsetPerCamera;
    // RayStartOffsetPerCamera.SetNum(RayNumPerCamera.Num());
    // int TempSum = 0;
    // for (int i = 0; i < RayNumPerCamera.Num(); i++)
    // {
    //     RayStartOffsetPerCamera[i] = TempSum;
    //     TempSum += RayNumPerCamera[i];
    //     UE_LOG(LogTemp, Log, TEXT("CameraIndex: %d StartOffset: %d LaserCount: %d"), i, RayStartOffsetPerCamera[i],
    //         RayNumPerCamera[i].load());
    // }
}

void ALidarBufferDepth::CreateSenceCaptureComponents()
{
    for (int i = 0; i < SceneCaptureCount; i++)
    {
        auto DebugName = FString::Printf(TEXT("SceneCaptureComponent2D_%d"), i);
        auto CaptureComponent2D = CreateSceneCaptureComponent(DebugName, AzimuthRange.X + FovHorizonPerCamera * i);
        SceneCaptures.Add(CaptureComponent2D);
        if (bOutputExtraInfo)
        {
            auto ExtraDebugName = FString::Printf(TEXT("SceneCaptureComponent2D_EXTRA_%d"), i);
            auto ExtraCaptureComponent2D =
                CreateSceneCaptureComponent(ExtraDebugName, AzimuthRange.X + FovHorizonPerCamera * i);
            ExtraSceneCaptures.Add(ExtraCaptureComponent2D);
        }
    }
    // add scene view extension
    SceneViewExtension = MakeShared<FDepthMapBasedLidarSceneViewExtension>(this, bUseSingleCapture);
    SceneCaptures[SceneCaptures.Num() - 1]->SceneViewExtensions.Add(SceneViewExtension);
    // if (bOutputExtraInfo)
    // {
    //     ExtraSceneCaptures[ExtraSceneCaptures.Num() - 1]->SceneViewExtensions.Add(SceneViewExtension);
    // }
}

void ALidarBufferDepth::CreateTextureRenderTargets()
{
    for (int i = 0; i < SceneCaptureCount; i++)
    {
        auto Name = FString(TEXT("SBL_RT_")) + FString::FromInt(i);
        auto renderTargetInfo = CreateRenderTargetInfo(Name);
        RenderTargets.Add(renderTargetInfo);
        if (bOutputExtraInfo)
        {
            auto ExtraName = FString(TEXT("SBL_RT_EXTRA_")) + FString::FromInt(i);
            auto extraRenderTargetInfo = CreateRenderTargetInfo(ExtraName);
            ExtraRenderTargets.Add(extraRenderTargetInfo);
        }
    }
}

ALidarBufferDepth::RenderTargetSRVInfo ALidarBufferDepth::CreateRenderTargetInfo(const FString& DebugName)
{
    RenderTargetSRVInfo renderTargetInfo;
    renderTargetInfo.debug_name = DebugName;

    auto renderTarget = NewObject<UTextureRenderTarget2D>(actor, FName(*renderTargetInfo.debug_name));

    renderTarget->InitCustomFormat(ImageWidthPerCamera, ImageHeightPerCamera, PF_R8G8B8A8, true);

    renderTarget->CompressionSettings = TextureCompressionSettings::TC_Default;
    renderTarget->SRGB = false;
    renderTarget->bAutoGenerateMips = false;
    renderTarget->bGPUSharedFlag = true;
    renderTarget->AddressX = TextureAddress::TA_Clamp;
    renderTarget->AddressY = TextureAddress::TA_Clamp;
    // renderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;

    renderTarget->UpdateResourceImmediate(true);

    if (renderTarget != nullptr)
    {
        // UE_LOG(LogTemp, Warning, TEXT("rendertarget create done"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("rendertarget failed"));
    }

    renderTargetInfo.render_target = renderTarget;
    return renderTargetInfo;
}

USceneCaptureComponent2D* ALidarBufferDepth::CreateSceneCaptureComponent(const FString& DebugName, float Yaw)
{
    FMatrix projectionMatrix =
        util::CalcProjectionMatrix(FovHorizonPerCamera, FovRangeVerticalPerCamera, GNearClippingPlane);

    auto CaptureComponent2D = NewObject<USceneCaptureComponent2D>(actor, FName(*DebugName));

    CaptureComponent2D->SetMobility(EComponentMobility::Movable);
    CaptureComponent2D->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
    // CaptureComponent2D->bEnableClipPlane = false;
    // CaptureComponent2D->ClipPlaneBase = FVector(10000, 0, 0);
    // CaptureComponent2D->ClipPlaneNormal = FVector(1, 0, 0);
    CaptureComponent2D->bCaptureOnMovement = false;
    CaptureComponent2D->bCaptureEveryFrame = false;
    CaptureComponent2D->bAlwaysPersistRenderingState = true;
    // CaptureComponent2D->MaxViewDistanceOverride = 25000;
    // CaptureComponent2D->FOVAngle = FovHorizonPerCamera;

    CaptureComponent2D->bUseCustomProjectionMatrix = true;
    CaptureComponent2D->CustomProjectionMatrix = projectionMatrix;

    CaptureComponent2D->SetRelativeRotation(FQuat(FRotator(0, Yaw, 0)));
    CaptureComponent2D->SetRelativeLocation(FVector(0, 0, 0));
    CaptureComponent2D->AttachToComponent(actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    CaptureComponent2D->CreationMethod = EComponentCreationMethod::Instance;
    CaptureComponent2D->RegisterComponent();
    return CaptureComponent2D;
}

void ALidarBufferDepth::SetupComponents()
{
    auto material = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), nullptr,
        TEXT("/Script/Engine.Material'/WorldXShaders/Sensor/DepthBasedLidar/DepthMapEncode.DepthMapEncode'")));
    for (int i = 0; i < SceneCaptureCount; i++)
    {
        SceneCaptures[i]->Deactivate();
        SceneCaptures[i]->TextureTarget = RenderTargets[i].render_target;
        SceneCaptures[i]->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

        auto materialInstance = UMaterialInstanceDynamic::Create(material, actor);
        MaterialInstanceDynamics.Add(materialInstance);

        materialInstance->SetScalarParameterValue(FName(TEXT("hfov")), FMath::DegreesToRadians(FovHorizonPerCamera));
        materialInstance->SetScalarParameterValue(
            FName(TEXT("vfov")), FMath::DegreesToRadians(FovRangeVerticalPerCamera.Y - FovRangeVerticalPerCamera.X));

        SceneCaptures[i]->PostProcessSettings.AddBlendable(materialInstance, 1);

        if (bUseEnableFlags)
        {
            EnableShowFlags(SceneCaptures[i]->ShowFlags);
        }
        else
        {
            DisableShowFlags(SceneCaptures[i]->ShowFlags);
        }
        SceneCaptures[i]->Activate();
    }

    if (bOutputExtraInfo)
    {
        auto extraMaterial = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), nullptr,
            TEXT("/Script/Engine.Material'/WorldXShaders/Sensor/DepthBasedLidar/"
                 "DepthMapEncodeExtra.DepthMapEncodeExtra'")));
        for (int i = 0; i < SceneCaptureCount; i++)
        {
            ExtraSceneCaptures[i]->Deactivate();
            ExtraSceneCaptures[i]->TextureTarget = ExtraRenderTargets[i].render_target;
            ExtraSceneCaptures[i]->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

            auto extraMaterialInstance = UMaterialInstanceDynamic::Create(extraMaterial, actor);
            MaterialInstanceDynamics.Add(extraMaterialInstance);
            ExtraSceneCaptures[i]->PostProcessSettings.AddBlendable(extraMaterialInstance, 1);

            if (bUseEnableFlags)
            {
                EnableShowFlags(ExtraSceneCaptures[i]->ShowFlags);
            }
            else
            {
                DisableShowFlags(ExtraSceneCaptures[i]->ShowFlags);
            }
            ExtraSceneCaptures[i]->Activate();
        }
    }
}

void ALidarBufferDepth::SetupBuffers()
{
    ENQUEUE_RENDER_COMMAND(FGetUAV_RenderThread)
    ([&](FRHICommandListImmediate& RHICommandList) {
        RawLidarBufferRDGPooledBuffer =
            AllocatePooledBuffer(FRDGBufferDesc::CreateByteAddressDesc(MaxPointNum * sizeof(LidarDetection)),
                TEXT("DepthMapBasedLidar.Output.DetectionBuffer"));
        LaserNumPerScanRDGPooledBuffer =
            AllocatePooledBuffer(FRDGBufferDesc::CreateByteAddressDesc(ScanCount * sizeof(int)),
                TEXT("DepthMapBasedLidar.Output.LaserNumPerScan"));
        DetectionCountRDGPooledBuffer = AllocatePooledBuffer(
            FRDGBufferDesc::CreateByteAddressDesc(sizeof(int)), TEXT("DepthMapBasedLidar.Output.DetectionCount"));

        ScanBufferRDGPooledBuffer =
            AllocatePooledBuffer(FRDGBufferDesc::CreateByteAddressDesc(ScanCount * sizeof(LidarScan)),
                TEXT("DepthMapBasedLidar.Output.ScanBuffer"));
        ScanOffsetRDGPooledBuffer = AllocatePooledBuffer(FRDGBufferDesc::CreateByteAddressDesc(ScanCount * sizeof(int)),
            TEXT("DepthMapBasedLidar.Output.ScanOffset"));

        ReorderedBufferRDGPooledBuffer =
            AllocatePooledBuffer(FRDGBufferDesc::CreateByteAddressDesc(MaxPointNum * sizeof(LidarDetection)),
                TEXT("DepthMapBasedLidar.Output.OutputBuffer"));

        ScanAzimuthRDGPooledBuffer =
            AllocatePooledBuffer(FRDGBufferDesc::CreateByteAddressDesc(ScanCount * sizeof(float)),
                TEXT("DepthMapBasedLidar.Output.ScanAzimuth"));
        void* ScanAzimuthTempBuffer = RHICommandList.LockBuffer(
            ScanAzimuthRDGPooledBuffer->GetRHI(), 0, ScanCount * sizeof(float), RLM_WriteOnly);
        FMemory::Memcpy(ScanAzimuthTempBuffer, ScanAzimuth.GetData(), ScanCount * sizeof(float));
        RHICommandList.UnlockBuffer(ScanAzimuthRDGPooledBuffer->GetRHI());

        const int ImageSpaceLaserRaysByteSize = ImageSpaceLaserRays.Num() * ImageSpaceLaserRays.GetTypeSize();
        ImageSpaceLaserRaysRDGPooledBuffer =
            AllocatePooledBuffer(FRDGBufferDesc::CreateByteAddressDesc(ImageSpaceLaserRaysByteSize),
                TEXT("DepthMapBasedLidar.ImageSpaceLaserRays"));
        void* DataTempBuffer = RHICommandList.LockBuffer(
            ImageSpaceLaserRaysRDGPooledBuffer->GetRHI(), 0, ImageSpaceLaserRaysByteSize, RLM_WriteOnly);
        FMemory::Memcpy(DataTempBuffer, ImageSpaceLaserRays.GetData(), ImageSpaceLaserRaysByteSize);
        RHICommandList.UnlockBuffer(ImageSpaceLaserRaysRDGPooledBuffer->GetRHI());
        for (size_t i = 0; i < SceneCaptureCount; i++)
        {
            auto& renderTargetInfo = RenderTargets[i];
            FRHITexture* RHITexture =
                renderTargetInfo.render_target->GetRenderTargetResource()->GetRenderTargetTexture().GetReference();
            renderTargetInfo.pooled_target = CreateRenderTarget(RHITexture, *renderTargetInfo.debug_name);
            if (bOutputExtraInfo)
            {
                auto& extraRenderTargetInfo = ExtraRenderTargets[i];
                FRHITexture* RHITextureExtra = extraRenderTargetInfo.render_target->GetRenderTargetResource()
                                                   ->GetRenderTargetTexture()
                                                   .GetReference();
                extraRenderTargetInfo.pooled_target =
                    CreateRenderTarget(RHITextureExtra, *extraRenderTargetInfo.debug_name);
            }
        }
        LidarReadbackDetectionCount = MakeShared<FRHIGPUBufferReadback>(TEXT("Lidar.Readback.DetectionCount"));
        LidarReadbackDetection = MakeShared<FRHIGPUBufferReadback>(TEXT("Lidar.Readback.Detection"));
        LidarReadbackScan = MakeShared<FRHIGPUBufferReadback>(TEXT("Lidar.Readback.Scan"));
    });

    FlushRenderingCommands();
}

void ALidarBufferDepth::AddLidarBasePass(FRDGBuilder& GraphBuilder, const int& CameraIndex,
    const FRDGTextureSRVRef& RenderTargetSRV, const FLidarPassParams& PassParams)
{
    double angle_in_rad = -FMath::DegreesToRadians(FovHorizonPerCamera);
    float sin_h_fov = FMath::Sin(angle_in_rad * CameraIndex);
    float cos_h_fov = FMath::Cos(angle_in_rad * CameraIndex);

    FDepthBasedLidarCSInstance::FRawHitParameters Parameters;
    Parameters.ChannelCount = Channels;
    Parameters.HorizonCount = HorizonSampleCount;
    Parameters.CameraIndex = CameraIndex;
    Parameters.CosAzimuth = cos_h_fov;
    Parameters.SinAzimuth = sin_h_fov;
    Parameters.Range = Range;
    Parameters.RawHitBuffer = PassParams.RawLidarBufferUAV;
    Parameters.LaserNumPerScan = PassParams.LaserNumPerScanUAV;
    Parameters.InTexture = RenderTargetSRV;
    Parameters.InTextureExtra = RenderTargetSRV;
    Parameters.ImageSpaceLaserRays = PassParams.ImageSpaceLaserRaysSRV;
    FDepthBasedLidarCSInstance::Get()->GraphBuilderDispatchLidar(GraphBuilder, Parameters);
}

void ALidarBufferDepth::AddLidarBasePassExtra(FRDGBuilder& GraphBuilder, const int& CameraIndex,
    const FRDGTextureSRVRef& RenderTargetSRV, const FRDGTextureSRVRef& RenderTargetSRVExtra,
    const FLidarPassParams& PassParams)
{
    double angle_in_rad = -FMath::DegreesToRadians(FovHorizonPerCamera);
    float sin_h_fov = FMath::Sin(angle_in_rad * CameraIndex);
    float cos_h_fov = FMath::Cos(angle_in_rad * CameraIndex);

    FDepthBasedLidarCSInstance::FRawHitParameters Parameters;
    Parameters.ChannelCount = Channels;
    Parameters.HorizonCount = HorizonSampleCount;
    Parameters.CameraIndex = CameraIndex;
    Parameters.CosAzimuth = cos_h_fov;
    Parameters.SinAzimuth = sin_h_fov;
    Parameters.Range = Range;
    Parameters.RawHitBuffer = PassParams.RawLidarBufferUAV;
    Parameters.LaserNumPerScan = PassParams.LaserNumPerScanUAV;
    Parameters.InTexture = RenderTargetSRV;
    Parameters.InTextureExtra = RenderTargetSRVExtra;
    Parameters.ImageSpaceLaserRays = PassParams.ImageSpaceLaserRaysSRV;
    FDepthBasedLidarCSInstance::Get()->GraphBuilderDispatchLidar(GraphBuilder, Parameters);
}

void ALidarBufferDepth::AddLidarPostPass(FRDGBuilder& GraphBuilder, const FLidarPassParams& PassParams)
{
    // scan
    {
        FDepthBasedLidarCSInstance::FScanParameters Parameters;
        Parameters.ScanCount = ScanCount;
        Parameters.ScanSequenceOffset = ScanSequenceCount;
        Parameters.DetectionCount = PassParams.DetectionCountUAV;
        Parameters.ScanOffsetUAV = PassParams.ScanOffsetUAV;
        Parameters.ScanAzimuth = PassParams.ScanAzimuthSRV;
        Parameters.LaserNumPerScan = PassParams.LaserNumPerScanUAV;
        Parameters.ScanBuffer = PassParams.ScanBufferUAV;
        FDepthBasedLidarCSInstance::Get()->GraphBuilderDispatchLidar(GraphBuilder, Parameters);
    }

    // reorder
    {
        FDepthBasedLidarCSInstance::FReorderParameters Parameters;
        Parameters.ScanCount = ScanCount;
        Parameters.ChannelCount = Channels;
        Parameters.LaserNumPerScan = PassParams.LaserNumPerScanUAV;
        Parameters.ScanOffsetUAV = PassParams.ScanOffsetUAV;
        Parameters.RawHitBuffer = PassParams.RawLidarBufferUAV;
        Parameters.ReorderedLidarBuffer = PassParams.ReorderedBufferUAV;

        FDepthBasedLidarCSInstance::Get()->GraphBuilderDispatchLidar(GraphBuilder, Parameters);
    }

    AddEnqueueCopyPass(
        GraphBuilder, LidarReadbackDetectionCount.Get(), PassParams.DetectionCountUAV->GetParent(), sizeof(int));

    // AddEnqueueCopyPass(GraphBuilder, LidarReadbackDetection.Get(), PassParams.ReorderedBufferUAV->GetParent(),
    //     MaxPointNum * sizeof(LidarDetection));

    AddEnqueueCopyPass(GraphBuilder, LidarReadbackDetection.Get(), PassParams.RawLidarBufferUAV->GetParent(),
        MaxPointNum * sizeof(LidarDetection));

    AddEnqueueCopyPass(
        GraphBuilder, LidarReadbackScan.Get(), PassParams.ScanBufferUAV->GetParent(), ScanCount * sizeof(LidarScan));
}

ALidarBufferDepth::FLidarPassParams ALidarBufferDepth::CreateLidarPassParams(
    FRDGBuilder& GraphBuilder, ALidarBufferDepth::ELidarPassType ParamsType)
{
    FLidarPassParams PassParams;

    if (ParamsType & ELidarPassType::BasePass)
    {
        // laser rays
        PassParams.ImageSpaceLaserRaysSRV =
            FRDGBuilderHelper::RegisterSRVBuffer(GraphBuilder, ImageSpaceLaserRaysRDGPooledBuffer);
    }

    if ((ParamsType & ELidarPassType::BasePass) || (ParamsType & ELidarPassType::PostPass))
    {
        // detection buffer
        PassParams.RawLidarBufferUAV =
            FRDGBuilderHelper::RegisterUAVBuffer(GraphBuilder, RawLidarBufferRDGPooledBuffer);

        // laser counter per scan
        PassParams.LaserNumPerScanUAV =
            FRDGBuilderHelper::RegisterUAVBuffer(GraphBuilder, LaserNumPerScanRDGPooledBuffer);

        // detection count
        PassParams.DetectionCountUAV =
            FRDGBuilderHelper::RegisterUAVBuffer(GraphBuilder, DetectionCountRDGPooledBuffer);
    }

    if (ParamsType & ELidarPassType::PostPass)
    {
        // scan azimuth
        PassParams.ScanAzimuthSRV = FRDGBuilderHelper::RegisterSRVBuffer(GraphBuilder, ScanAzimuthRDGPooledBuffer);

        // scan buffer
        PassParams.ScanBufferUAV = FRDGBuilderHelper::RegisterUAVBuffer(GraphBuilder, ScanBufferRDGPooledBuffer);

        // scan offset
        PassParams.ScanOffsetUAV = FRDGBuilderHelper::RegisterUAVBuffer(GraphBuilder, ScanOffsetRDGPooledBuffer);

        // output buffer
        PassParams.ReorderedBufferUAV =
            FRDGBuilderHelper::RegisterUAVBuffer(GraphBuilder, ReorderedBufferRDGPooledBuffer);
    }

    return MoveTemp(PassParams);
}

void ALidarBufferDepth::DisableShowFlags(FEngineShowFlags& ShowFlags)
{
    ShowFlags.SetAmbientOcclusion(false);
    ShowFlags.SetAntiAliasing(false);
    ShowFlags.SetVolumetricFog(false);
    // ShowFlags.SetAtmosphericFog(false);
    // ShowFlags.SetAudioRadius(false);
    // ShowFlags.SetBillboardSprites(false);
    ShowFlags.SetBloom(false);
    // ShowFlags.SetBounds(false);
    // ShowFlags.SetBrushes(false);
    // ShowFlags.SetBSP(false);
    // ShowFlags.SetBSPSplit(false);
    // ShowFlags.SetBSPTriangles(false);
    // ShowFlags.SetBuilderBrush(false);
    // ShowFlags.SetCameraAspectRatioBars(false);
    // ShowFlags.SetCameraFrustums(false);
    ShowFlags.SetCameraImperfections(false);
    ShowFlags.SetCameraInterpolation(false);
    // ShowFlags.SetCameraSafeFrames(false);
    // ShowFlags.SetCollision(false);
    // ShowFlags.SetCollisionPawn(false);
    // ShowFlags.SetCollisionVisibility(false);
    ShowFlags.SetColorGrading(false);
    // ShowFlags.SetCompositeEditorPrimitives(false);
    // ShowFlags.SetConstraints(false);
    // ShowFlags.SetCover(false);
    // ShowFlags.SetDebugAI(false);
    // ShowFlags.SetDecals(false);
    ShowFlags.SetDeferredLighting(false);
    ShowFlags.SetDepthOfField(false);
    ShowFlags.SetDiffuse(false);
    ShowFlags.SetDirectionalLights(false);
    ShowFlags.SetDirectLighting(false);
    // ShowFlags.SetDistanceCulledPrimitives(false);
    // ShowFlags.SetDistanceFieldAO(false);
    // ShowFlags.SetDistanceFieldGI(false);
    ShowFlags.SetDynamicShadows(false);
    // ShowFlags.SetEditor(false);
    ShowFlags.SetEyeAdaptation(false);
    ShowFlags.SetFog(false);
    // ShowFlags.SetGame(false);
    // ShowFlags.SetGameplayDebug(false);
    // ShowFlags.SetGBufferHints(false);
    ShowFlags.SetGlobalIllumination(false);
    ShowFlags.SetGrain(false);
    // ShowFlags.SetGrid(false);
    // ShowFlags.SetHighResScreenshotMask(false);
    // ShowFlags.SetHitProxies(false);
    ShowFlags.SetHLODColoration(false);
    ShowFlags.SetHMDDistortion(false);
    // ShowFlags.SetIndirectLightingCache(false);
    // ShowFlags.SetInstancedFoliage(false);
    // ShowFlags.SetInstancedGrass(false);
    // ShowFlags.SetInstancedStaticMeshes(false);
    // ShowFlags.SetLandscape(false);
    // ShowFlags.SetLargeVertices(false);
    ShowFlags.SetLensFlares(false);
    ShowFlags.SetLightComplexity(false);
    ShowFlags.SetLightFunctions(false);
    ShowFlags.SetLightInfluences(false);
    ShowFlags.SetLighting(false);
    ShowFlags.SetLightMapDensity(false);
    ShowFlags.SetLightRadius(false);
    ShowFlags.SetLightShafts(false);
    // ShowFlags.SetLOD(false);
    ShowFlags.SetLODColoration(false);
    // ShowFlags.SetMaterials(false);
    // ShowFlags.SetMaterialTextureScaleAccuracy(false);
    // ShowFlags.SetMeshEdges(false);
    // ShowFlags.SetMeshUVDensityAccuracy(false);
    // ShowFlags.SetModeWidgets(false);
    ShowFlags.SetMotionBlur(false);
    // ShowFlags.SetNavigation(false);
    ShowFlags.SetOnScreenDebug(false);
    // ShowFlags.SetOutputMaterialTextureScales(false);
    // ShowFlags.SetOverrideDiffuseAndSpecular(false);
    // ShowFlags.SetPaper2DSprites(false);
    ShowFlags.SetParticles(false);
    // ShowFlags.SetPivot(false);
    ShowFlags.SetPointLights(false);
    // ShowFlags.SetPostProcessing(false);
    // ShowFlags.SetPostProcessMaterial(false);
    // ShowFlags.SetPrecomputedVisibility(false);
    // ShowFlags.SetPrecomputedVisibilityCells(false);
    // ShowFlags.SetPreviewShadowsIndicator(false);
    // ShowFlags.SetPrimitiveDistanceAccuracy(false);
    // ShowFlags.SetQuadOverdraw(false);
    // ShowFlags.SetReflectionEnvironment(false);
    // ShowFlags.SetReflectionOverride(false);
    ShowFlags.SetRefraction(false);
    // ShowFlags.SetRendering(false);
    ShowFlags.SetSceneColorFringe(false);
    // ShowFlags.SetScreenPercentage(false);
    ShowFlags.SetScreenSpaceAO(false);
    ShowFlags.SetScreenSpaceReflections(false);
    // ShowFlags.SetSelection(false);
    // ShowFlags.SetSelectionOutline(false);
    // ShowFlags.SetSeparateTranslucency(false);
    // ShowFlags.SetShaderComplexity(false);
    // ShowFlags.SetShaderComplexityWithQuadOverdraw(false);
    // ShowFlags.SetShadowFrustums(false);
    // ShowFlags.SetSkeletalMeshes(false);
    // ShowFlags.SetSkinCache(false);
    ShowFlags.SetSkyLighting(false);
    // ShowFlags.SetSnap(false);
    // ShowFlags.SetSpecular(false);
    // ShowFlags.SetSplines(false);
    ShowFlags.SetSpotLights(false);
    // ShowFlags.SetStaticMeshes(false);
    ShowFlags.SetStationaryLightOverlap(false);
    // ShowFlags.SetStereoRendering(false);
    // ShowFlags.SetStreamingBounds(false);
    ShowFlags.SetSubsurfaceScattering(false);
    // ShowFlags.SetTemporalAA(false);
    // ShowFlags.SetTessellation(false);
    // ShowFlags.SetTestImage(false);
    // ShowFlags.SetTextRender(false);
    // ShowFlags.SetTexturedLightProfiles(false);
    ShowFlags.SetTonemapper(false);
    // ShowFlags.SetTranslucency(false);
    // ShowFlags.SetVectorFields(false);
    // ShowFlags.SetVertexColors(false);
    // ShowFlags.SetVignette(false);
    // ShowFlags.SetVisLog(false);
    // ShowFlags.SetVisualizeAdaptiveDOF(false);
    // ShowFlags.SetVisualizeBloom(false);
    ShowFlags.SetVisualizeBuffer(false);
    ShowFlags.SetVisualizeDistanceFieldAO(false);
    ShowFlags.SetVisualizeDOF(false);
    ShowFlags.SetVisualizeHDR(false);
    ShowFlags.SetVisualizeLightCulling(false);
    // ShowFlags.SetVisualizeLPV(false);
    ShowFlags.SetVisualizeMeshDistanceFields(false);
    ShowFlags.SetVisualizeMotionBlur(false);
    ShowFlags.SetVisualizeOutOfBoundsPixels(false);
    ShowFlags.SetVisualizeSenses(false);
    ShowFlags.SetVisualizeShadingModels(false);
    ShowFlags.SetVisualizeSSR(false);
    ShowFlags.SetVisualizeSSS(false);
    // ShowFlags.SetVolumeLightingSamples(false);
    // ShowFlags.SetVolumes(false);
    // ShowFlags.SetWidgetComponents(false);
    // ShowFlags.SetWireframe(false);
}

void ALidarBufferDepth::EnableShowFlags(FEngineShowFlags& ShowFlags)
{
    ShowFlags = FEngineShowFlags(ESFIM_All0);
    ShowFlags.SetRendering(true);
    ShowFlags.SetMaterials(true);
    // ShowFlags.SetBones(true);
    ShowFlags.SetSkeletalMeshes(true);
    ShowFlags.SetStaticMeshes(true);
    ShowFlags.SetInstancedStaticMeshes(true);
    ShowFlags.SetInstancedFoliage(true);
    ShowFlags.SetInstancedGrass(true);
    // ShowFlags.SetParticles(true);
    // ShowFlags.SetNiagara(true);
    ShowFlags.SetLandscape(true);
    ShowFlags.SetBrushes(true);
    ShowFlags.SetPostProcessMaterial(true);
    ShowFlags.SetPostProcessing(true);
    ShowFlags.SetNaniteMeshes(true);
    ShowFlags.SetNaniteStreamingGeometry(true);
    ShowFlags.SetTonemapper(false);
    ShowFlags.SetEyeAdaptation(false);
}