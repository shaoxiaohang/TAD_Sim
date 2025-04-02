#include "CameraSensor.h"

#include "CineCameraComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HadMap/Public/HadmapManager.h"
#include "Kismet/GameplayStatics.h"
#include "Runtime/Engine/Classes/Components/PostProcessComponent.h"
#include "Runtime/Engine/Classes/Components/SceneCaptureComponent2D.h"
#include "TexJpeg.h"
#include "Utils/ProjectionUtil.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

ACameraSensor::ACameraSensor()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if
    // you don't need it.
    PrimaryActorTick.bCanEverTick = false;
    // str_PostProcess = TEXT("Material'/Game/SensorSim/Camera/Material/Mat_Camera_Capture.Mat_Camera_Capture'");

    RootComponent = CreateDefaultSubobject<USceneComponent>(FName(TEXT("Root")));

    // auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CamMesh"));
    // Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
    // Mesh->bHiddenInGame = false;
    // Mesh->CastShadow = false;
    // RootComponent = Mesh;

    // previewComponent = CreateDefaultSubobject<UCineCameraComponent>(FName(TEXT("PreviewCamera")));
    // previewComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
    // previewComponent->SetUseFieldOfViewForLOD(true);
    // previewComponent->SetActive(false);

    captureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(FName(TEXT("CaptureCamera")));
    captureComponent->SetActive(false);

    // static ConstructorHelpers::FClassFinder<AActor> BlueprintActorClass(TEXT(
    //     "/Game/Horizon/Main/Blueprints/Env/UltraDynamicSky/Blueprints/Ultra_Dynamic_Sky.uasset"));
    // if (BlueprintActorClass.Succeeded())
    // {
    //     UltraDynamicSkyClass = BlueprintActorClass.Class;
    // }

    // // Load skybox BP.
    // UltraDynamicSkyClass = LoadClass<AActor>(
    //     NULL, TEXT("Blueprint'/Game/Horizon/Main/Blueprints/Env/UltraDynamicSky/Blueprints/"
    //                "Ultra_Dynamic_Sky.Ultra_Dynamic_Sky_C'"));
    // if (!UltraDynamicSkyClass)
    // {
    //     UE_LOG(LogTemp, Warning, TEXT("ACameraSensor: Cant load skyBoxBP class!"));
    // }
    // SkyPostProcessComponent = UltraDynamicSkyClass-
}

ACameraSensor::~ACameraSensor()
{
}

void ACameraSensor::PostActorCreated()
{
    Super::PostActorCreated();

    //   auto *StaticMeshComponent = Cast<UStaticMeshComponent>(RootComponent);
    //   if (StaticMeshComponent && !StaticMeshComponent->GetStaticMesh())
    //   {
    //     UStaticMesh *CamMesh = LoadObject<UStaticMesh>(
    //         NULL,
    //         TEXT("/Engine/EditorMeshes/MatineeCam_SM.MatineeCam_SM"),
    //         NULL,
    //         LOAD_None,
    //         NULL);
    //     StaticMeshComponent->SetStaticMesh(CamMesh);
    //   }
}

void ACameraSensor::IgnoreActor(AActor* actor)
{
    if (actor)
    {
        captureComponent->HiddenActors.Add(actor);
    }
}

ISimActorInterface* ACameraSensor::Install(const FSensorConfig& _Config)
{
    ISimActorInterface* SimActor = Super::Install(_Config);
    /* Install previewCamera */
    if (SimActor)
    {
        AActor* Ego = Cast<AActor>(SimActor);
        UE_LOG(LogTemp, Log, TEXT("ACameraSensor: Ignore Ego %s"), *Ego->GetName());
        IgnoreActor(Ego);
        // TODO: All simActors support install camera
        ATransportPawn* Transport = Cast<ATransportPawn>(SimActor);
        if (Transport)
        {
            // if (Transport->InstallCamera(_Config.typeName + FString::FromInt(_Config.id), previewComponent))
            // {
            //     // previewComponent->SetActive(true);
            //     // auto hilpos = GetDisplayInstance()->nHILpos;
            //     // if (hilpos.X >= 0 && hilpos.Y >= 0)
            //     // {
            //     //     Transport->SetDefaultCamera(_Config.typeName + FString::FromInt(_Config.id));

            //     //     if (GEngine && GEngine->GameViewport)
            //     //     {
            //     //         if (GEngine->GameViewport->Viewport->IsFullscreen())
            //     //         {
            //     //             GEngine->GameViewport->HandleToggleFullscreenCommand();
            //     //         }
            //     //         GEngine->GameViewport->GetWindow()->MoveWindowTo(hilpos);
            //     //         GEngine->GameViewport->HandleToggleFullscreenCommand();
            //     //     }
            //     // }
            // }
            return Transport;
        }
    }
    return nullptr;
}

bool ACameraSensor::Init(const FSensorConfig& _Config)
{
    ASensorActor::Init(_Config);

    // TArray<AActor*> FoundActors;
    // AActor* DynamicSkyActor = nullptr;
    // UGameplayStatics::GetAllActorsOfClass(this, UltraDynamicSkyClass, FoundActors);
    // for (AActor* Actor : FoundActors)
    // {
    //     DynamicSkyActor = Actor;
    //     break;
    // }
    // if (DynamicSkyActor)
    // {
    //     UE_LOG(LogTemp, Display, TEXT("ACameraSensor: Found skyBoxBP!"));
    //     SkyPostProcessComponent = DynamicSkyActor->FindComponentByClass<UPostProcessComponent>();
    //     if(SkyPostProcessComponent)
    //     {
    //         UE_LOG(LogTemp, Display, TEXT("ACameraSensor: Found SkyPostProcessComponent!"));
    //     }else
    //     {
    //         UE_LOG(LogTemp, Warning, TEXT("ACameraSensor: Cant find SkyPostProcessComponent!"));
    //     }

    // }
    // else
    // {
    //     UE_LOG(LogTemp, Warning, TEXT("ACameraSensor: Cant find skyBoxBP!"));
    // }

    const FCameraConfig* NewCameraSensorConfig = Cast_Sim<const FCameraConfig>(_Config);

    sensorConfig = *NewCameraSensorConfig;
    if (!NewCameraSensorConfig)
    {
        UE_LOG(LogTemp, Warning, TEXT("CameraSensorComponent: Cant Cast to FCameraConfig!"));
        return false;
    }

    bool enable_preview = sensorConfig.addPreview;

    // ID
    id = NewCameraSensorConfig->id;
    // frequency
    frequency = NewCameraSensorConfig->frequency;
    // Save path
    savePath = NewCameraSensorConfig->savePath;
    if (!savePath.IsEmpty() && !FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*savePath))
    {
        // UE_LOG(LogTemp, Warning, TEXT("CameraSensorComponent: Generate savePath."));
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*savePath);
    }
    UE_LOG(LogTemp, Warning, TEXT("CameraSensorComponent SavePath %s "), *savePath);
    sensorConfig = *NewCameraSensorConfig;
    imageRes0.X = NewCameraSensorConfig->res_Horizontal;
    imageRes0.Y = NewCameraSensorConfig->res_Vertical;
    imageRes = imageRes0;

    if (!str_PostProcess.IsEmpty())
    {
        // 加载材质
        UMaterial* NewMat = LoadObject<UMaterial>(NULL, *str_PostProcess);
        if (!NewMat)
        {
            UE_LOG(LogTemp, Warning, TEXT("CameraSensorComponent: Cant get Material!"));
            return false;
        }
        // 创建材质实例动态
        mid_CameraPostProcess = UMaterialInstanceDynamic::Create(NewMat, this);
        if (!mid_CameraPostProcess)
        {
            UE_LOG(LogTemp, Warning, TEXT("CameraSensorComponent: Cant get MaterialInstanceDynamic!"));
            return false;
        }
    }

    double NewFov_H = 0;
    double NewFov_V = 0;
    double fx = -1;
    double fy = -1;
    double cx = 0.5;
    double cy = 0.5;
    double skew = 0;
    double k1 = 0, k2 = 0, k3 = 0, p1 = 0, p2 = 0;
    double fov_scale = 1.0f;
    double maxxfov = 0, maxyfov = 0;
    double ofx = 0, ofy = 0;

    // 检查传感器配置是否包含内参矩阵
    if (sensorConfig.paraType == EParamTypeEnum::PT_Int && sensorConfig.intrinsic_Matrix.Num() == 9)
    {
        // 获取内参矩阵的值
        const auto& im = sensorConfig.intrinsic_Matrix;
        fx = im[0];
        fy = im[4];
        cx = im[2];
        cy = im[5];
        skew = im[1];
        if (NewCameraSensorConfig->distortion_Parameters.Num() > 0)
            k1 = NewCameraSensorConfig->distortion_Parameters[0];
        if (NewCameraSensorConfig->distortion_Parameters.Num() > 1)
            k2 = NewCameraSensorConfig->distortion_Parameters[1];
        if (NewCameraSensorConfig->distortion_Parameters.Num() > 2)
            k3 = NewCameraSensorConfig->distortion_Parameters[2];
        if (NewCameraSensorConfig->distortion_Parameters.Num() > 3)
            p1 = NewCameraSensorConfig->distortion_Parameters[3];
        if (NewCameraSensorConfig->distortion_Parameters.Num() > 4)
            p2 = NewCameraSensorConfig->distortion_Parameters[4];
    }
    // 检查传感器配置是否包含有效的水平和垂直FOV值
    else if (sensorConfig.paraType == EParamTypeEnum::PT_Fov && sensorConfig.fov_Horizontal > 0 &&
             sensorConfig.fov_Vertical > 0)
    {
        // 根据fov配置计算内参矩阵
        cx = 0.5 * sensorConfig.res_Horizontal;
        cy = 0.5 * sensorConfig.res_Vertical;
        fx = 0.5 * sensorConfig.res_Horizontal / FMath::Tan(FMath::DegreesToRadians(sensorConfig.fov_Horizontal) * 0.5);
        fy = 0.5 * sensorConfig.res_Vertical / FMath::Tan(FMath::DegreesToRadians(sensorConfig.fov_Vertical) * 0.5);
    }
    // 检查传感器配置是否包含有效的CCD值
    else if (sensorConfig.paraType == EParamTypeEnum::PT_Ccd && sensorConfig.ccd_Height > 0 &&
             sensorConfig.ccd_Width > 0)
    {
        // 根据ccd配置计算内参矩阵
        cx = 0.5 * sensorConfig.res_Horizontal;
        cy = 0.5 * sensorConfig.res_Vertical;
        fx = sensorConfig.res_Horizontal * sensorConfig.focal_Length / sensorConfig.ccd_Width;
        fx = sensorConfig.res_Vertical * sensorConfig.focal_Length / sensorConfig.ccd_Height;
    }
    if (fx <= 0 || fy <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("CameraSensor: fx fy errro!"));
        return false;
    }

    if (mid_CameraPostProcess)
    {
        // 内参
        mid_CameraPostProcess->SetScalarParameterValue(FName(TEXT("cx")), cx);
        mid_CameraPostProcess->SetScalarParameterValue(FName(TEXT("cy")), cy);
        mid_CameraPostProcess->SetScalarParameterValue(FName(TEXT("fx")), fx);
        mid_CameraPostProcess->SetScalarParameterValue(FName(TEXT("fy")), fy);
        mid_CameraPostProcess->SetScalarParameterValue(FName(TEXT("skew")), skew);
        mid_CameraPostProcess->SetScalarParameterValue(FName(TEXT("k1")), k1);
        mid_CameraPostProcess->SetScalarParameterValue(FName(TEXT("k2")), k2);
        mid_CameraPostProcess->SetScalarParameterValue(FName(TEXT("k3")), k3);
        mid_CameraPostProcess->SetScalarParameterValue(FName(TEXT("p1")), p1);
        mid_CameraPostProcess->SetScalarParameterValue(FName(TEXT("p2")), p2);
        mid_CameraPostProcess->SetScalarParameterValue(FName(TEXT("ofx")), ofx);
        mid_CameraPostProcess->SetScalarParameterValue(FName(TEXT("ofy")), ofy);
        mid_CameraPostProcess->SetScalarParameterValue(FName(TEXT("w")), NewCameraSensorConfig->res_Horizontal);
        mid_CameraPostProcess->SetScalarParameterValue(FName(TEXT("h")), NewCameraSensorConfig->res_Vertical);
        // Blur
        mid_CameraPostProcess->SetScalarParameterValue(
            FName(TEXT("BlurIntensity")), NewCameraSensorConfig->blur_Intensity);
        mid_CameraPostProcess->SetScalarParameterValue(FName(TEXT("Res_X")), NewCameraSensorConfig->res_Horizontal);
        mid_CameraPostProcess->SetScalarParameterValue(FName(TEXT("Res_Y")), NewCameraSensorConfig->res_Vertical);
    }

    NewFov_H = 2.0f * FMath::RadiansToDegrees(FMath::Atan2(imageRes.X, 2.0f * fx));
    NewFov_V = 2.0f * FMath::RadiansToDegrees(FMath::Atan2(imageRes.Y, 2.0f * fy));

    UE_LOG(LogTemp, Log, TEXT("CameraSensor: NewFov_H=%f, NewFov_V=%f"), NewFov_H, NewFov_V);

    float flen = 100.f;
    // // Set the current focus length of the camera
    // previewComponent->CurrentFocalLength = flen;
    // // Update the minimum and maximum focal lengths for this component's lens settings
    // previewComponent->LensSettings.MinFocalLength = flen;
    // previewComponent->LensSettings.MaxFocalLength = flen;
    // // Set the minimum and maximum aperture values for this component's lens settings
    // previewComponent->LensSettings.MinFStop = 7.0f;
    // previewComponent->LensSettings.MaxFStop = 7.0f;
    // // Calculate the sensor dimensions based on the new field-of-view (FOV), focallength, and aspect
    // // ratio
    // previewComponent->Filmback.SensorWidth = FMath::Tan(FMath::DegreesToRadians(NewFov_H) / 2) * 2 * flen;
    // previewComponent->Filmback.SensorHeight = FMath::Tan(FMath::DegreesToRadians(NewFov_V) / 2) * 2 * flen;
    // // Set the post process settings to match the newly calculated FOV
    // SetPostProcessSettings(
    //     *NewCameraSensorConfig, previewComponent->PostProcessSettings, FMath::Max(1.0, fov_scale * 0.9));
    // // Add the mid-camera postprocess effect with an opacity value of 1.0 to the list of blendables
    // // in the post process settings
    // // previewComponent->PostProcessSettings.AddBlendable(mid_CameraPostProcess, 1);

    if (GetDisplayInstance() && GetDisplayInstance()->nHILpos.X < 0)
    {
        bool memshared = false, gpushared = false, style = false;
        // 获取配置文件中"Sensor"部分的配置项
        GConfig->GetBool(TEXT("Sensor"), TEXT("CameraMemShare"), memshared, GGameIni);
        GConfig->GetBool(TEXT("Sensor"), TEXT("CameraGPUShare"), gpushared, GGameIni);
        GConfig->GetBool(TEXT("Sensor"), TEXT("CameraStyle"), style, GGameIni);
        GConfig->GetBool(TEXT("Sensor"), TEXT("PublicMsg"), public_msg, GGameIni);
        // 从配置文件获取图像质量设置
        GConfig->GetInt(TEXT("Sensor"), TEXT("JpegQuality"), imageQuality, GGameIni);

        FString gpuid = TEXT("0");
        if (!FParse::Value(FCommandLine::Get(), TEXT("-graphicsadapter="), gpuid))
        {
            gpuid = TEXT("0");
        }
        UE_LOG(LogTemp, Log, TEXT("CameraSensor: use gpu id: %s"), *gpuid);
        texJpg = MakeShared<UTexJpeg>(FCString::Atoi(*gpuid));
        FString stylemode;

        // 创建一个新的纹理目标2D对象
        renderTarget2D = NewObject<UTextureRenderTarget2D>();
        // 设置共享标志为true，以便可以在GPU上共享
        renderTarget2D->bGPUSharedFlag = 1;
        // 设置渲染目标格式为RGBA8
        renderTarget2D->RenderTargetFormat = RTF_RGBA8;
        // 设置目标gamma
        renderTarget2D->TargetGamma = targetGamma;
        // 初始化自定义格式，设置图像大小和像素格式
        renderTarget2D->InitCustomFormat(imageRes.X, imageRes.Y, PF_B8G8R8A8, true);

        texJpg->texRT = renderTarget2D;
        if (!texJpg->InitResources(true, gpushared, imageRes0.X, imageRes0.Y, stylemode))
        {
            texJpg.Reset();
            imageRes = imageRes0;
            renderTarget2D->InitCustomFormat(imageRes.X, imageRes.Y, PF_B8G8R8A8, true);
            UE_LOG(LogTemp, Warning, TEXT("CameraSensor: texJpg init faild."));
        }
        // 将场景捕获组件附加到根组件上
        captureComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
        // 设置场景捕获组件是否每帧进行捕获
        captureComponent->bCaptureEveryFrame = true;
        //captureComponent->bCaptureOnMovement = false;
        // 设置场景捕获组件自动激活
        captureComponent->bAutoActivate = true;
        captureComponent->SetActive(true);
        // 设置场景捕获组件为活动状态
        // 设置场景捕获组件的源类型为最终颜色（线性深度）
        captureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
        // 设置场景捕获组件的投影模式为透视
        captureComponent->ProjectionType = ECameraProjectionMode::Perspective;
        // 设置场景捕获组件的近裁剪平面和远裁剪平面
        captureComponent->OrthoWidth = 20000.f;
        // 使用自定义投影矩阵
        captureComponent->bUseCustomProjectionMatrix = true;
        captureComponent->ShowFlags.SetMotionBlur(true);
        captureComponent->ShowFlags.SetAmbientOcclusion(false);

		// ApplyDefaultPostProcessSetting(Sensor->PostProcessSettings);
		// Sensor->PostProcessSettings = PPSettings;

		// captureComponent->PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
		// captureComponent->PostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
		// captureComponent->PostProcessSettings.bOverride_ReflectionMethod = true;
		// captureComponent->PostProcessSettings.ReflectionMethod = EReflectionMethod::Lumen;

		// captureComponent->PostProcessSettings.bOverride_AutoExposureMethod = true;
		// captureComponent->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
		// captureComponent->PostProcessSettings.bOverride_AutoExposureBias = true;
		// captureComponent->PostProcessSettings.AutoExposureBias = 0;

		// captureComponent->PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
		// captureComponent->PostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
		// captureComponent->PostProcessSettings.bOverride_ReflectionMethod = true;
		// captureComponent->PostProcessSettings.ReflectionMethod = EReflectionMethod::Lumen;

		// captureComponent->PostProcessSettings.bOverride_LumenSurfaceCacheResolution = true;
		// captureComponent->PostProcessSettings.LumenSurfaceCacheResolution = 0.001;

        // if ((int32) ERHIZBuffer::IsInverted)
        // {
        //     captureComponent->CustomProjectionMatrix =
        //         FReversedZPerspectiveMatrix(FMath::DegreesToRadians(NewFov_H * 0.5f),
        //             FMath::DegreesToRadians(NewFov_V * 0.5f), 1.f, 1.f, GNearClippingPlane, GNearClippingPlane);
        // }
        // else
        // {
        //     captureComponent->CustomProjectionMatrix = FPerspectiveMatrix(FMath::DegreesToRadians(NewFov_H * 0.5f),
        //         FMath::DegreesToRadians(NewFov_V * 0.5f), 1.f, 1.f, GNearClippingPlane, GNearClippingPlane);
        // }
        captureComponent->CustomProjectionMatrix =
            util::CalcProjectionMatrix(fx, fy, cx, cy, imageRes.X, imageRes.Y, GNearClippingPlane);
        captureComponent->FOVAngle = NewFov_H;
        captureComponent->bUseRayTracingIfEnabled = true;
        // 设置场景捕获组件的视角
        // captureComponent->FOVAngle = fmaxf(NewFov_H, NewFov_V);
        // 设置场景捕获组件的目标纹理
        captureComponent->TextureTarget = renderTarget2D;
        // 设置后处理设置
        SetPostProcessSettings(
            *NewCameraSensorConfig, captureComponent->PostProcessSettings, FMath::Max(1.0, fov_scale * 0.9));
        if (mid_CameraPostProcess)
        {
            captureComponent->PostProcessSettings.AddBlendable(mid_CameraPostProcess, 1);
        }

        // 内存共享
        if (memshared)
        {
            UE_LOG(LogTemp, Log, TEXT("CameraSensor: memshare is on."));

            UE_LOG(LogTemp, Warning, TEXT("CameraSensor: shared file is: Tadsim_%s_%d"), *imageName, sensorConfig.id);
            sharedWriter = MakeShared<SharedMemoryWriter>();
            if (!sharedWriter->init(
                    std::string("Tadsim_") + TCHAR_TO_ANSI(*imageName) + "_" + std::to_string(sensorConfig.id), 100))
            {
                UE_LOG(LogTemp, Warning, TEXT("CameraSensor: sharedWriter init faild."));
                sharedWriter.Reset();
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("CameraSensor: HIL MODEL."));
    }
    if (sharedWriter)
        UE_LOG(LogTemp, Log, TEXT("CameraSensor: sharedWriter is ok"));
    if (renderTarget2D)
        UE_LOG(LogTemp, Log, TEXT("CameraSensor: renderTarget2D is ok"));
    if (texJpg)
        UE_LOG(LogTemp, Log, TEXT("CameraSensor: texJpg is ok"));

    //UseLitShowFlags(captureComponent->ShowFlags);


    return true;
}

void ACameraSensor::Update(const FSensorInput& _Input, FSensorOutput& _Output)
{
    if (!renderTarget2D)
    {
        return;
    }
    const FCameraInput* CameraInput = Cast_Sim<const FCameraInput>(_Input);
    // 频率限制
    if (frequency > 0 && (CameraInput->timeStamp - timeStamp) < 999.9999999 / frequency)
    {
        // UE_LOG(LogTemp, Warning, TEXT("%s: Camera frequency async, has return, Frequency %f
        // TimeStamp is: %f Camera TimeStamp %f"), *this->GetName(), frequency, timeStamp,
        // CameraInput->timeStamp);
        return;
    }
    timeStamp = CameraInput->timeStamp;
    double timeStamp_ego = CameraInput->timeStamp_ego;
    // CUDA拷贝
    bool cuda = false;
    if (texJpg)
    {
        cuda = texJpg->Copy2Cuda();
    }

    std::vector<uint8>& BitData = dataBuf.buffer;
    BitData.clear();

    // 获取图像数据
    auto getRawBuff = [&]() {
        if (!BitData.empty())
        {
            return;
        }
        if (cuda)
        {
            if (!texJpg->Raw(BitData))
            {
                UE_LOG(LogTemp, Warning, TEXT("CameraSensor: read raw image buf faild in texjpg"));
            }
        }
        else
        {
            // captureComponent->CaptureScene();
            // captureComponent->MarkRenderStateDirty();
            // FlushRenderingCommands();

            FReadSurfaceDataFlags ReadPixelFlags(RCM_UNorm);
            FTextureRenderTarget2DResource* RTResource =
                (FTextureRenderTarget2DResource*) renderTarget2D->GameThread_GetRenderTargetResource();
            if (RTResource)
            {
                TArray<FColor> BitMap;
                RTResource->ReadPixels(BitMap, ReadPixelFlags);
                BitData.resize(BitMap.Num() * 4);
                memcpy(BitData.data(), BitMap.GetData(), BitMap.Num() * 4);
            }
        }
        if (BitData.empty())
        {
            UE_LOG(LogTemp, Warning, TEXT("CameraSensor: read image buf faild."));
        }
    };

    TArray64<uint8_t> imgbuf;

    // 内存共享
    if (sharedWriter)
    {
        getRawBuff();

        if (BitData.empty())
        {
            UE_LOG(LogTemp, Warning, TEXT("CameraSensor: read image buf faild."));
        }
        else
        {
            IImageWrapperModule& ImageWrapperModule =
                FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
            TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
            if (ImageWrapper->SetRaw(BitData.data(), sizeof(FColor) * imageRes0.X * imageRes0.Y, imageRes0.X,
                    imageRes0.Y, ERGBFormat::BGRA, 8))
            {
                imgbuf = ImageWrapper->GetCompressed(imageQuality);
                if (!sharedWriter->write(imgbuf, timeStamp_ego))
                {
                    UE_LOG(LogTemp, Warning, TEXT("CameraSensor: sharedWriter write faild."));
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("CameraSensor: encode jpeg faild."));
            }
        }
    }

    // 落盘或发布
    if (!savePath.IsEmpty() || public_msg)
    {
        // jpeg图像，优先cuda编码，否则用ue自带CPU编码
        if (imageFormat == EImageFormat::JPEG)
        {
            if (cuda)
            {
                if (!texJpg->JpegEncoding(imgbuf))
                {
                    UE_LOG(LogTemp, Warning, TEXT("CameraSensor: read jpeg image buf faild in texjpg"));
                }
                UE_LOG(LogTemp, Display, TEXT("CameraSensor: encode cuda"));
            }

            if (imgbuf.Num() == 0)
            {
                getRawBuff();

                if (BitData.empty())
                {
                    UE_LOG(LogTemp, Warning, TEXT("CameraSensor: read image buf faild."));
                }
                else
                {
                    IImageWrapperModule& ImageWrapperModule =
                        FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
                    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
                    if (ImageWrapper->SetRaw(BitData.data(), sizeof(FColor) * imageRes0.X * imageRes0.Y, imageRes0.X,
                            imageRes0.Y, ERGBFormat::BGRA, 8))
                    {
                        imgbuf = ImageWrapper->GetCompressed(imageQuality);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("CameraSensor: encode jpeg faild."));
                    }
                }
            }
        }
        // png用自带的编码器
        else if (imageFormat == EImageFormat::PNG)
        {
            getRawBuff();
            if (BitData.empty())
            {
                UE_LOG(LogTemp, Warning, TEXT("CameraSensor: read image buf faild."));
            }
            else
            {
                IImageWrapperModule& ImageWrapperModule =
                    FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
                TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(imageFormat);

                if (imageName == TEXT("Normal"))
                {
                    if (ImageWrapper->SetRaw(BitData.data(), sizeof(FColor) * imageRes0.X * imageRes0.Y, imageRes0.X,
                            imageRes0.Y, ERGBFormat::BGRA, 8))
                    {
                        imgbuf = ImageWrapper->GetCompressed();
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("CameraSensor: encode jpeg faild."));
                    }
                }
                else if (imageName == TEXT("Depth") || imageName == TEXT("Semantic"))
                {
                    TArray<uint8_t> BitMap;
                    BitMap.SetNum(dataBuf.buffer.size() / 4);
                    for (int i = 0; i < BitMap.Num(); i++)
                    {
                        BitMap[i] = dataBuf.buffer[i * 4 + 3];
                    }
                    if (ImageWrapper->SetRaw(BitMap.GetData(), sizeof(uint8_t) * imageRes.X * imageRes.Y, imageRes.X,
                            imageRes.Y, ERGBFormat::Gray, 8))
                    {
                        imgbuf = ImageWrapper->GetCompressed();
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("CameraSensor: imageName is not supported: %s"), *imageName);
                }
            }
        }
        // exr用自带的编码器
        else if (imageFormat == EImageFormat::EXR)
        {
            getRawBuff();
            if (BitData.empty())
            {
                UE_LOG(LogTemp, Warning, TEXT("CameraSensor: read image buf faild."));
            }
            else
            {
                IImageWrapperModule& ImageWrapperModule =
                    FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
                TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(imageFormat);
                TArray<float> BitMap;
                BitMap.SetNum(dataBuf.buffer.size() / 4);
                for (int i = 0; i < BitMap.Num(); i++)
                {
                    BitMap[i] = (dataBuf.buffer[i * 4 + 2] * 65536.0 + dataBuf.buffer[i * 4 + 1] * 256.0 +
                                    dataBuf.buffer[i * 4]) *
                                0.0001;
                }
                if (ImageWrapper->SetRaw(BitMap.GetData(), sizeof(float) * imageRes.X * imageRes.Y, imageRes.X,
                        imageRes.Y, ERGBFormat::Gray, 32))
                {
                    imgbuf = ImageWrapper->GetCompressed();
                }
                UE_LOG(LogTemp, Display, TEXT("CameraSensor: encode exr"));
            }
        }
        // 落盘
        if (!savePath.IsEmpty())
        {
            if (imgbuf.Num() > 0)
            {
                FString SavePath =
                    savePath + FString::Printf(TEXT("%s_%d_%010d"), *imageName, id, (int64) timeStamp_ego);
                if (imageFormat == EImageFormat::JPEG)
                {
                    SavePath += TEXT(".jpg");
                }
                else if (imageFormat == EImageFormat::PNG)
                {
                    SavePath += TEXT(".png");
                }
                else if (imageFormat == EImageFormat::EXR)
                {
                    SavePath += TEXT(".exr");
                }
                GetDisplayInstance()->GetSaveDataHandle()->SaveJPG(imgbuf, SavePath);
            }
            // 保存POSE
            {
                FString savePathWithName = savePath +
                                           FString::Printf(TEXT("%s_%d_%010d"), *imageName, id, (int64) timeStamp_ego) +
                                           TEXT(".pose");
                std::stringstream ss;
                ss << std::setprecision(15);
                ss << "pose of sensor(lon lat atl roll pitch yaw), the enu reference coord(wgs84) "
                      "and mat of "
                      "world->image\n";
                double X = 0, Y = 0, Z = 0;
                hadmapue4::HadmapManager::Get()->LocalToLonLat(GetActorLocation(), X, Y, Z);
                ss << X << " " << Y << " " << Z << " ";
                auto Rot = GetActorRotation();
                ss << Rot.Roll * PI / 180. << " " << -Rot.Pitch * PI / 180. << " " << -(Rot.Yaw + 90.f) * PI / 180.
                   << "\n";
                ss << hadmapue4::HadmapManager::Get()->mapOriginLon << " "
                   << hadmapue4::HadmapManager::Get()->mapOriginLat << " "
                   << hadmapue4::HadmapManager::Get()->mapOriginAlt << "\n";

                auto loc = GetActorLocation() * 0.01f;
                std::swap(loc.X, loc.Y);
                loc.X *= -1.f;
                loc.Y *= -1.f;
                Rot.Roll *= -1.f;
                Rot.Yaw = -(Rot.Yaw + 90.f);
                auto rot = Rot.Quaternion() * FRotator(0, -90, 0).Quaternion() * FRotator(0, 0, 90).Quaternion();
                FTransform tf;
                tf.SetLocation(loc);
                tf.SetRotation(rot);
                auto tfmat = tf.Inverse().ToMatrixNoScale();
                for (int i = 0; i < 4; i++)
                {
                    for (int j = 0; j < 4; j++)
                    {
                        ss << tfmat.M[j][i] << " ";
                    }
                }
                GetDisplayInstance()->GetSaveDataHandle()->SaveString(
                    ANSI_TO_TCHAR(ss.str().c_str()), savePathWithName);
            }
        }
        // public msg
        if (public_msg)
        {
            sim_msg::CameraRaw craw;
            craw.set_id(id);
            craw.set_timestamp(timeStamp_ego);
            if (imgbuf.Num() > 0)
            {
                if (imageFormat == EImageFormat::JPEG)
                {
                    craw.set_type("JPEG");
                }
                else if (imageFormat == EImageFormat::PNG)
                {
                    craw.set_type("PNG");
                }
                else if (imageFormat == EImageFormat::EXR)
                {
                    craw.set_type("EXR");
                }
                craw.set_image_data(imgbuf.GetData(), imgbuf.Num());
            }

            double X = 0, Y = 0, Z = 0;
            hadmapue4::HadmapManager::Get()->LocalToLonLat(GetActorLocation(), X, Y, Z);
            craw.mutable_pose()->set_longitude(X);
            craw.mutable_pose()->set_latitude(Y);
            craw.mutable_pose()->set_altitude(Z);
            auto Rot = GetActorRotation();
            craw.mutable_pose()->set_roll(Rot.Roll * PI / 180.f);
            craw.mutable_pose()->set_pitch(-Rot.Pitch * PI / 180.f);
            craw.mutable_pose()->set_yaw(-(Rot.Yaw + 90.f) * PI / 180.f);
            craw.set_width(imageRes0.X);
            craw.set_height(imageRes0.Y);
            craw.SerializeToString(&_Output.serialize_string);
        }
    }
}

// 后处理设置
void ACameraSensor::SetPostProcessSettings(
    const FCameraConfig& config, FPostProcessSettings& PostProcessSettings, float screen_scale)
{
    // // 清晰度
    // // PostProcessSettings.bOverride_ScreenPercentage = 1;
    // // PostProcessSettings.ScreenPercentage =
    // //     100.0f * screen_scale * (1 + config.Exquisite * (config.Exquisite > 0 ? 1 : -0.2));

    // Exposure; 曝光设置

    // PostProcessSettings.bOverride_AutoExposureMethod = 1;
    // PostProcessSettings.AutoExposureMethod = AEM_Manual;
    // PostProcessSettings.bOverride_AutoExposureMethod = true;
    // PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;

    // PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
    // PostProcessSettings.AutoExposureMinBrightness = 1.0f;

    // PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
    // PostProcessSettings.AutoExposureMaxBrightness = 1.0f;

    // PostProcessSettings.bOverride_AutoExposureBias = true;
    // PostProcessSettings.AutoExposureBias = 0.0f;

    // // noise
    // if (config.noise_Intensity > 0)
    // {
    //     PostProcessSettings.bOverride_FilmGrainIntensity = 1;
    //     PostProcessSettings.FilmGrainIntensity = config.noise_Intensity;
    // }

    // if (config.LensFlares > 0)
    // {
    //     PostProcessSettings.bOverride_LensFlareIntensity = 1;
    //     PostProcessSettings.LensFlareIntensity = config.LensFlares * 16.0;
    // }

    // // Motion blur
    // PostProcessSettings.bOverride_MotionBlurAmount = 1;
    // PostProcessSettings.MotionBlurAmount = config.motionBlur_Amount;

    // // vignetting
    // PostProcessSettings.bOverride_VignetteIntensity = true;
    // PostProcessSettings.VignetteIntensity = config.vignette_Intensity;

    // // Color Grading
    // PostProcessSettings.bOverride_WhiteTemp = 1;
    // PostProcessSettings.bOverride_WhiteTint = 1;
    // PostProcessSettings.WhiteTemp = config.ColorTemperature;
    // PostProcessSettings.WhiteTint = config.WhiteHint;

    // if (config.color_gray)
    // {
    //     PostProcessSettings.bOverride_ColorSaturation = 1;
    //     PostProcessSettings.ColorSaturation.W = 0;
    // }
    // if(SkyPostProcessComponent)
    //{
    // PostProcessSettings = SkyPostProcessComponent->Settings;
    // UE_LOG(LogTemp, Display, TEXT("ACameraSensor: Apply SkyPostProcess!"));
    // PostProcessSettings.bOverride_AutoExposureMethod = true;
    // PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
    // PostProcessSettings.bOverride_AutoExposureBias = true;
    // PostProcessSettings.AutoExposureBias = 1;
    // PostProcessSettings.bOverride_AutoExposureSpeedDown = true;
    // PostProcessSettings.bOverride_AutoExposureSpeedUp = true;
    // /*Settings.bOverride_AutoExposureMaxBrightness = true;
    // Settings.bOverride_AutoExposureMinBrightness = true;
    // Settings.AutoExposureMaxBrightness = 1;
    // Settings.AutoExposureMinBrightness = 1;*/
    // PostProcessSettings.AutoExposureSpeedDown = 10;
    // PostProcessSettings.AutoExposureSpeedUp = 10;

    PostProcessSettings.bOverride_MotionBlurAmount = 1;
    PostProcessSettings.MotionBlurAmount = 0;

    PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
    PostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
    PostProcessSettings.bOverride_ReflectionMethod = true;
    PostProcessSettings.ReflectionMethod = EReflectionMethod::Lumen;

    // PostProcessSettings.bOverride_LumenSurfaceCacheResolution = true;
    // PostProcessSettings.LumenSurfaceCacheResolution = 0.001;
    //}
}