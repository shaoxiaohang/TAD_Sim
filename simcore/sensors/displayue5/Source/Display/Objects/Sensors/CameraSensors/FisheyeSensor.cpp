#include "FisheyeSensor.h"

#include "CineCameraComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTargetCube.h"
#include "Engine/World.h"
#include "Framework/DisplayGameInstance.h"
#include "Framework/DisplayPlayerController.h"
#include "Framework/SaveDataThread.h"
#include "HAL/PlatformFileManager.h"
#include "HighResScreenshot.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/FileHelper.h"
#include "Objects/Transports/TransportPawn.h"
#include "Runtime/Engine/Classes/Components/SceneCaptureComponent2D.h"
#include "Runtime/Engine/Classes/Components/SceneCaptureComponentCube.h"
#include "Runtime/Engine/Classes/Materials/MaterialInstanceDynamic.h"
#include "Runtime/ImageWrapper/Public/IImageWrapperModule.h"
#include "SimMsg/sensor_raw.pb.h"
#include "TexJpeg.h"
#include "Utils/GeometryUtil.h"
#include "Utils/ProjectionUtil.h"

AFisheyeSensor::AFisheyeSensor()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(FName(TEXT("Root")));

    previewComponent = CreateDefaultSubobject<UCineCameraComponent>(FName(TEXT("PreviewCamera")));
    previewComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
    previewComponent->SetUseFieldOfViewForLOD(true);
    previewComponent->SetActive(false);
    captureComponentCube = CreateDefaultSubobject<USceneCaptureComponentCube>(FName(TEXT("CaptureCameraCube")));
    captureComponent2D = CreateDefaultSubobject<USceneCaptureComponent2D>(FName(TEXT("CaptureCamera")));
    captureComponentCube->SetActive(false);
    captureComponent2D->SetActive(false);
}

AFisheyeSensor::~AFisheyeSensor()
{
}

bool AFisheyeSensor::Init(const FSensorConfig& _Config)
{
    ASensorActor::Init(_Config);
    // FSensorConfig* NewConfigPtr = &_Config;
    // FCameraConfig* NewCameraSensorConfig = static_cast<FCameraConfig*>(NewConfigPtr);
    const FFisheyeConfig* NewCameraSensorConfig = Cast_Sim<const FFisheyeConfig>(_Config);

    if (!NewCameraSensorConfig)
    {
        UE_LOG(LogTemp, Warning, TEXT("FisheyeCameraSensorComponent: Cant Cast to FCameraConfig!"));
        return false;
    }
    sensorConfig = *NewCameraSensorConfig;

    // 创建保存路径
    if (!sensorConfig.savePath.IsEmpty() &&
        !FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*sensorConfig.savePath))
    {
        // UE_LOG(LogTemp, Log, TEXT("CameraSensorComponent: Generate savePath."));
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*sensorConfig.savePath);
    }
    // 获取内参
    const auto& im = sensorConfig.intrinsic_Matrix;
    const auto& dp = sensorConfig.distortion_Parameters;
    double NewFov_H = 0;
    double NewFov_V = 0;
    double fx = im[0];
    double fy = im[4];
    double cx = im[2];
    double cy = im[5];
    double skew = im[1];
    double k1 = dp[0];
    double k2 = dp[1];
    double k3 = dp[2];
    double k4 = dp[3];

    GConfig->GetFloat(TEXT("Sensor"), TEXT("ProjectionFarPlane"), FrustumFarPlane, GGameIni);

    // double maxxfov = 0, maxyfov = 0;
    // // 计算了相机传感器的最大水平和垂直视野角度
    // {
    //     TArray<FVector2D> bounds;
    //     for (int i = 0; i < 11; i++)
    //     {
    //         double y = sensorConfig.res_Vertical * i * 0.1;
    //         for (int j = 0; j < 11; j++)
    //         {
    //             // Calculate bounding box points and add them to an array
    //             double x = sensorConfig.res_Horizontal * j * 0.1;
    //             bounds.Add(FVector2D(x, y));
    //         }
    //     }
    //     for (const auto& bd : bounds)
    //     {
    //         double tX = bd.X;
    //         double tY = bd.Y;
    //         double y = (tY - cy) / fy;
    //         double x = (tX - cx - skew * y) / fx;
    //         double theta_d = sqrt(x * x + y * y);
    //         double theta = theta_d;
    //         const double EPS = 0.000001;
    //         double scale = 1.0;
    //         if (theta_d > EPS)
    //         {
    //             for (int j = 0; j < 10; j++)
    //             {
    //                 double theta2 = theta * theta, theta4 = theta2 * theta2, theta6 = theta4 * theta2,
    //                        theta8 = theta6 * theta2;
    //                 double k0_theta2 = k1 * theta2, k1_theta4 = k2 * theta4, k2_theta6 = k3 * theta6,
    //                        k3_theta8 = k4 * theta8;
    //                 double theta_fix = (theta * (1 + k0_theta2 + k1_theta4 + k2_theta6 + k3_theta8) - theta_d) /
    //                                    (1 + 3 * k0_theta2 + 5 * k1_theta4 + 7 * k2_theta6 + 9 * k3_theta8);
    //                 theta = theta - theta_fix;
    //                 if (abs(theta_fix) < EPS)
    //                     break;
    //             }
    //             scale = tan(theta) / theta_d;
    //             theta_d = theta;
    //         }
    //         if (theta_d > -1.5 && theta_d < 1.5)
    //         {
    //             x *= scale;
    //             y *= scale;
    //             // Calculate new camera's field-of-view based on maximum horizontal and vertical angles
    //             maxxfov = FMath::Max(maxxfov, (double) FMath::Abs(FMath::Atan(x)));
    //             maxyfov = FMath::Max(maxyfov, (double) FMath::Abs(FMath::Atan(y)));
    //         }
    //     }
    // }
    // UE_LOG(LogTemp, Log, TEXT("Fisheye: max fov= %f, %f"), maxxfov, maxyfov);
    // if (maxxfov <= 0 || maxyfov <= 0)
    // {
    //     UE_LOG(LogTemp, Warning, TEXT("Fisheye: Cant get cal max fov!"));
    //     return false;
    // }
    // 使用cube获取图像
    // if (maxxfov > 1.3 || maxyfov > 1.3)    // 75du
    {
        UE_LOG(LogTemp, Log, TEXT("Fisheye: use USceneCaptureComponentCube"));
        UMaterial* NewMat = LoadObject<UMaterial>(NULL,
            TEXT("Material'/Game/SensorSim/Camera/Material/Mat_Camera_Capture_fisheye.Mat_Camera_Capture_fisheye'"));
        if (!NewMat)
        {
            UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor Component: Cant get Material!"));
            return false;
        }
        cameraPostProcess = UMaterialInstanceDynamic::Create(NewMat, this);
        if (!cameraPostProcess)
        {
            UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor Component: Cant get MaterialInstanceDynamic!"));
            return false;
        }
        UE_LOG(LogTemp, Log,
            TEXT("FisheyeSensor: cx=%f, cy=%f, fx=%f, fy=%f, skew=%f, k1=%f, k2=%f, k3=%f, k4=%f, w=%f, h=%f"), cx, cy,
            fx, fy, skew, k1, k2, k3, k4, NewCameraSensorConfig->res_Horizontal, NewCameraSensorConfig->res_Vertical);
        renderTargetCube = NewObject<UTextureRenderTargetCube>();
        renderTargetCube->Init(
            FMath::Min(sensorConfig.res_Horizontal, sensorConfig.res_Vertical), EPixelFormat::PF_B8G8R8A8);    //* 3 / 2
        captureComponentCube->bUseRayTracingIfEnabled = true;
        captureComponentCube->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
        captureComponentCube->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
        captureComponentCube->bCaptureEveryFrame = true;
        captureComponentCube->bAutoActivate = true;
        captureComponentCube->Activate();
        captureComponentCube->ShowFlags.SetEyeAdaptation(false);
        captureComponentCube->ShowFlags.SetVisualizeLocalExposure(false);

        captureComponentCube->TextureTarget = renderTargetCube;

        // 设置pp处理参数
        // 内参
        cameraPostProcess->SetScalarParameterValue(FName(TEXT("cx")), cx);
        cameraPostProcess->SetScalarParameterValue(FName(TEXT("cy")), cy);
        cameraPostProcess->SetScalarParameterValue(FName(TEXT("fx")), fx);
        cameraPostProcess->SetScalarParameterValue(FName(TEXT("fy")), fy);
        cameraPostProcess->SetScalarParameterValue(FName(TEXT("skew")), skew);
        cameraPostProcess->SetScalarParameterValue(FName(TEXT("k1")), k1);
        cameraPostProcess->SetScalarParameterValue(FName(TEXT("k2")), k2);
        cameraPostProcess->SetScalarParameterValue(FName(TEXT("k3")), k3);
        cameraPostProcess->SetScalarParameterValue(FName(TEXT("k4")), k4);
        cameraPostProcess->SetScalarParameterValue(FName(TEXT("w")), sensorConfig.res_Horizontal);
        cameraPostProcess->SetScalarParameterValue(FName(TEXT("h")), sensorConfig.res_Vertical);
        cameraPostProcess->SetTextureParameterValue(FName(TEXT("Param_Cube")), renderTargetCube);
        // Blur
        cameraPostProcess->SetScalarParameterValue(FName(TEXT("BlurIntensity")), sensorConfig.blur_Intensity);
        cameraPostProcess->SetScalarParameterValue(FName(TEXT("Res_X")), sensorConfig.res_Horizontal);
        cameraPostProcess->SetScalarParameterValue(FName(TEXT("Res_Y")), sensorConfig.res_Vertical);

        NewFov_H = 2.0f * FMath::RadiansToDegrees(FMath::Atan2(sensorConfig.res_Horizontal, 2.0f * fx));
        NewFov_V = 2.0f * FMath::RadiansToDegrees(FMath::Atan2(sensorConfig.res_Vertical, 2.0f * fy));

        UE_LOG(LogTemp, Log, TEXT("CameraSensor: NewFov_H=%f, NewFov_V=%f"), NewFov_H, NewFov_V);

        float flen = 100.f;
        // Set the current focus length of the camera
        previewComponent->CurrentFocalLength = flen;
        // Update the minimum and maximum focal lengths for this component's lens settings
        previewComponent->LensSettings.MinFocalLength = flen;
        previewComponent->LensSettings.MaxFocalLength = flen;
        // Set the minimum and maximum aperture values for this component's lens settings
        previewComponent->LensSettings.MinFStop = 7.0f;
        previewComponent->LensSettings.MaxFStop = 7.0f;
        // Calculate the sensor dimensions based on the new field-of-view (FOV), focallength, and aspect ratio
        previewComponent->Filmback.SensorWidth = FMath::Tan(FMath::DegreesToRadians(NewFov_H) / 2) * 2 * flen;
        previewComponent->Filmback.SensorHeight = FMath::Tan(FMath::DegreesToRadians(NewFov_V) / 2) * 2 * flen;
            // Set the post process settings to match the newly calculated FOV
            SetPostProcessSettings(previewComponent->PostProcessSettings);
        // Add the mid-camera postprocess effect with an opacity value of 1.0 to the list of blendables in the post
        // process settings
        previewComponent->PostProcessSettings.AddBlendable(cameraPostProcess, 1);
        if (GetDisplayInstance()->nHILpos.X < 0)
        {
            // 创建一个新的纹理目标2D对象
            renderTarget2D = NewObject<UTextureRenderTarget2D>();
            // 设置共享标志为true，以便可以在GPU上共享
            renderTarget2D->bGPUSharedFlag = 1;
            // 设置渲染目标格式为RGBA8
            renderTarget2D->RenderTargetFormat = RTF_RGBA8;
            // 设置目标gamma
            renderTarget2D->TargetGamma = targetGamma;
            // 初始化自定义格式，设置图像大小和像素格式
            renderTarget2D->InitCustomFormat(
                sensorConfig.res_Horizontal, sensorConfig.res_Vertical, EPixelFormat::PF_B8G8R8A8, true);
            // 将场景捕获组件附加到根组件上
            captureComponent2D->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
            // 设置场景捕获组件的源类型为最终颜色（线性深度）
            captureComponent2D->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
            captureComponent2D->TextureTarget = renderTarget2D;
            SetPostProcessSettings(captureComponent2D->PostProcessSettings);
            captureComponent2D->PostProcessSettings.AddBlendable(cameraPostProcess, 1);
            // 设置场景捕获组件是否每帧进行捕获
            captureComponent2D->bCaptureEveryFrame = true;
            // 设置场景捕获组件为活动状态
            captureComponent2D->bAutoActivate = true;
            captureComponent2D->Activate();
            captureComponent2D->ShowFlags.SetMotionBlur(true);
        }
    }
    // else
    // {
    //     if (captureComponentCube)
    //     {
    //         captureComponentCube->bCaptureEveryFrame = false;
    //         captureComponentCube->bAutoActivate = false;
    //         captureComponentCube->Deactivate();
    //     }

    //     UE_LOG(LogTemp, Log, TEXT("Fisheye: use UTextureRenderTarget2D"));
    //     // Calculate new camera's field-of-view based on maximum horizontal and vertical angles
    //     double ofx = 0.5 * NewCameraSensorConfig->res_Horizontal / FMath::Tan(maxxfov);
    //     double ofy = 0.5 * NewCameraSensorConfig->res_Vertical / FMath::Tan(maxyfov);
    //     double of = fmin(ofx, ofy);
    //     ofx = of;
    //     ofy = of;
    //     maxxfov = FMath::Atan(0.5 * NewCameraSensorConfig->res_Horizontal / of);
    //     maxyfov = FMath::Atan(0.5 * NewCameraSensorConfig->res_Vertical / of);
    //     // NEW fov
    //     double NewFov_H = FMath::RadiansToDegrees(maxxfov) * 2;
    //     double NewFov_V = FMath::RadiansToDegrees(maxyfov) * 2;
    //     double fov_scale = FMath::Max(fx / ofx, fy / ofy);

    //     UMaterial* NewMat =
    //         LoadObject<UMaterial>(NULL, TEXT("Material'/Game/SensorSim/Camera/Material/"
    //                                          "Mat_Camera_Capture_fisheye_pinhole.Mat_Camera_Capture_fisheye_pinhole'"));
    //     if (!NewMat)
    //     {
    //         UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor Component: Cant get Material!"));
    //         return false;
    //     }
    //     cameraPostProcess = UMaterialInstanceDynamic::Create(NewMat, this);
    //     if (!cameraPostProcess)
    //     {
    //         UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor Component: Cant get MaterialInstanceDynamic!"));
    //         return false;
    //     }
    //     // 设置pp处理参数
    //     cameraPostProcess->SetScalarParameterValue(FName(TEXT("cx")), cx);
    //     cameraPostProcess->SetScalarParameterValue(FName(TEXT("cy")), cy);
    //     cameraPostProcess->SetScalarParameterValue(FName(TEXT("fx")), fx);
    //     cameraPostProcess->SetScalarParameterValue(FName(TEXT("fy")), fy);
    //     cameraPostProcess->SetScalarParameterValue(FName(TEXT("skew")), skew);
    //     cameraPostProcess->SetScalarParameterValue(FName(TEXT("k1")), k1);
    //     cameraPostProcess->SetScalarParameterValue(FName(TEXT("k2")), k2);
    //     cameraPostProcess->SetScalarParameterValue(FName(TEXT("k3")), k3);
    //     cameraPostProcess->SetScalarParameterValue(FName(TEXT("k4")), k4);
    //     cameraPostProcess->SetScalarParameterValue(FName(TEXT("ofx")), ofx);
    //     cameraPostProcess->SetScalarParameterValue(FName(TEXT("ofy")), ofy);
    //     cameraPostProcess->SetScalarParameterValue(FName(TEXT("w")), NewCameraSensorConfig->res_Horizontal);
    //     cameraPostProcess->SetScalarParameterValue(FName(TEXT("h")), NewCameraSensorConfig->res_Vertical);
    //     // Blur
    //     cameraPostProcess->SetScalarParameterValue(FName(TEXT("BlurIntensity")),
    //     NewCameraSensorConfig->blur_Intensity); cameraPostProcess->SetScalarParameterValue(FName(TEXT("Res_X")),
    //     NewCameraSensorConfig->res_Horizontal); cameraPostProcess->SetScalarParameterValue(FName(TEXT("Res_Y")),
    //     NewCameraSensorConfig->res_Vertical);

    //     UE_LOG(LogTemp, Log,
    //         TEXT("CameraSensor: cx=%f, cy=%f, fx=%f, fy=%f, skew=%f, k1=%f, k2=%f, k3=%f, k4=%f, ofx=%f, ofy=%f,
    //         w=%f, "
    //              "h=%f, fscale=%f"),
    //         cx, cy, fx, fy, skew, k1, k2, k3, k4, ofx, ofy, NewCameraSensorConfig->res_Horizontal,
    //         NewCameraSensorConfig->res_Vertical, fov_scale);

    //     // 设置预览组件的焦距
    //     float flen = 100.f;
    //     previewComponent->CurrentFocalLength = flen;
    //     previewComponent->LensSettings.MinFocalLength = flen;
    //     previewComponent->LensSettings.MaxFocalLength = flen;
    //     previewComponent->LensSettings.MinFStop = 7.0f;
    //     previewComponent->LensSettings.MaxFStop = 7.0f;
    //     previewComponent->Filmback.SensorWidth = FMath::Tan(FMath::DegreesToRadians(NewFov_H) / 2) * 2 * flen;
    //     previewComponent->Filmback.SensorHeight = FMath::Tan(FMath::DegreesToRadians(NewFov_V) / 2) * 2 * flen;

    //     SetPostProcessSettings(captureComponent2D->PostProcessSettings, FMath::Max(1.0, fov_scale * 0.7));
    //     previewComponent->PostProcessSettings.AddBlendable(cameraPostProcess, 1);
    //     if (GetDisplayInstance()->nHILpos.X < 0)
    //     {
    //         // 设置传感器图像的渲染
    //         renderTarget2D = NewObject<UTextureRenderTarget2D>();
    //         renderTarget2D->bGPUSharedFlag = 1;
    //         renderTarget2D->RenderTargetFormat = RTF_RGBA8;
    //         renderTarget2D->TargetGamma = targetGamma;
    //         renderTarget2D->InitCustomFormat(
    //             NewCameraSensorConfig->res_Horizontal, NewCameraSensorConfig->res_Vertical, PF_B8G8R8A8, true);
    //         captureComponent2D = NewObject<USceneCaptureComponent2D>();
    //         captureComponent2D->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
    //         captureComponent2D->bCaptureEveryFrame = true;
    //         captureComponent2D->bAutoActivate = true;
    //         captureComponent2D->SetActive(true);
    //         captureComponent2D->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    //         captureComponent2D->ProjectionType = ECameraProjectionMode::Perspective;
    //         captureComponent2D->OrthoWidth = 20000.f;
    //         captureComponent2D->bUseCustomProjectionMatrix = true;
    //         if ((int32) ERHIZBuffer::IsInverted)
    //         {
    //             captureComponent2D->CustomProjectionMatrix =
    //                 FReversedZPerspectiveMatrix(FMath::DegreesToRadians(NewFov_H * 0.5),
    //                     FMath::DegreesToRadians(NewFov_V * 0.5f), 1.f, 1.f, GNearClippingPlane, GNearClippingPlane);
    //         }
    //         else
    //         {
    //             captureComponent2D->CustomProjectionMatrix =
    //                 FPerspectiveMatrix(FMath::DegreesToRadians(NewFov_H * 0.5f),
    //                     FMath::DegreesToRadians(NewFov_V * 0.5f), 1.f, 1.f, GNearClippingPlane, GNearClippingPlane);
    //         }
    //         captureComponent2D->FOVAngle = fmaxf(NewFov_H, NewFov_V);
    //         captureComponent2D->TextureTarget = renderTarget2D;

    //         captureComponent2D->ShowFlags.SetMotionBlur(true);
    //         SetPostProcessSettings(captureComponent2D->PostProcessSettings, FMath::Max(1.0, fov_scale * 0.7));
    //         captureComponent2D->PostProcessSettings.AddBlendable(cameraPostProcess, 1);
    //     }
    // }
    if (GetDisplayInstance()->nHILpos.X < 0)
    {
        bool memshared = false, gpushared = false, style = false;
        // 获取配置文件中"Sensor"部分的配置项
        GConfig->GetBool(TEXT("Sensor"), TEXT("CameraMemShare"), memshared, GGameIni);
        GConfig->GetBool(TEXT("Sensor"), TEXT("CameraGPUShare"), gpushared, GGameIni);
        GConfig->GetBool(TEXT("Sensor"), TEXT("CameraStyle"), style, GGameIni);
        GConfig->GetBool(TEXT("Sensor"), TEXT("PublicMsg"), public_msg, GGameIni);
        GConfig->GetInt(TEXT("Sensor"), TEXT("JpegQuality"), imageQuality, GGameIni);
        FString gpuid = TEXT("0");
        if (!FParse::Value(FCommandLine::Get(), TEXT("-graphicsadapter="), gpuid))
        {
            gpuid = TEXT("0");
        }

        texJpg = MakeShared<UTexJpeg>(FCString::Atoi(*gpuid));
        texJpg->texRT = renderTarget2D;
        if (!texJpg->InitResources(true))
        {
            texJpg.Reset();
            UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: texJpg init faild."));
        }

        // 内存共享
        if (memshared)
        {
            UE_LOG(LogTemp, Log, TEXT("FisheyeSensor: memshare is on."));
            sharedWriter = MakeShared<SharedMemoryWriter>();
            if (!sharedWriter->init(
                    std::string("Tadsim_") + TCHAR_TO_ANSI(*imageName) + "_" + std::to_string(sensorConfig.id), 100))
            {
                UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: sharedWriter init faild."));
                sharedWriter.Reset();
            }
        }
        // GPU显存共享
        if (gpushared && texJpg)
        {
            UE_LOG(LogTemp, Log, TEXT("FisheyeSensor: gpu share is on."));
            // ipc
            sharedWriterGpu = MakeShared<SharedMemoryWriter>();
            if (!sharedWriterGpu->init(std::string("Tadsim_") + TCHAR_TO_ANSI(*imageName) + "_" +
                                       std::to_string(sensorConfig.id) + "_GPU"))
            {
                UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: sharedWriterGpu init faild."));
                sharedWriterGpu.Reset();
            }

            std::vector<uint8> buf(1024);
            if (texJpg->IpcCreate(buf.data() + 8))
            {
                int w = sensorConfig.res_Horizontal, h = sensorConfig.res_Vertical;
                memcpy(buf.data(), &w, sizeof(int));
                memcpy(buf.data() + 4, &h, sizeof(int));
                std::time_t t = std::time(0);
                UE_LOG(LogTemp, Log, TEXT("FisheyeSensor: shared gpu verification code is %ld."), t);
                if (!sharedWriterGpu->write(buf, t))
                {
                    UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: sharedgpu write faild."));
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: share ipc faild."));
            }
        }
    }

    bool bDrawFrustum = false;
    GConfig->GetBool(TEXT("Sensor"), TEXT("DrawFrustum"), bDrawFrustum, GGameIni);
    if (bDrawFrustum)
    {
        DrawFrustum(NewFov_V / 2.0f, NewFov_H / 2.0f);
    }

    return true;
}

void AFisheyeSensor::DrawFrustum(float UpperFov, float LeftFov)
{
    FrustumMaterial =
        LoadObject<UMaterial>(NULL, TEXT("Material'/Game/SensorSim/Camera/Material/Mat_Frustum.Mat_Frustum'"));
    if (FrustumMaterial)
    {
        FrustumComponent = NewObject<UCustomMeshComponent>(this);
        UMaterialInstanceDynamic* material_ins = UMaterialInstanceDynamic::Create(FrustumMaterial, this);
        FrustumComponent->SetMaterial(0, material_ins);

        float maxFov = std::max(UpperFov, LeftFov);
        float minFov = std::min(UpperFov, LeftFov);
        float fullHeight = FrustumFarPlane * FMath::Sin(FMath::DegreesToRadians(maxFov));
        float lowerHeight = FrustumFarPlane * FMath::Sin(FMath::DegreesToRadians(minFov));
        float scale = lowerHeight / fullHeight;
        float vScale = 1;
        float hScale = 1;
        if (LeftFov > UpperFov)
        {
            vScale = scale;
        }
        else if (UpperFov > LeftFov)
        {
            hScale = scale;
        }

        TArray<FCustomMeshTriangle> triangles;

        float PhiStart = 0.0f;
        float PhiLength = 2 * PI;
        float ThetaStart = 0.0f;
        float ThetaLength = FMath::DegreesToRadians(maxFov);
        float ConeRadius = FMath::Sin(FMath::DegreesToRadians(maxFov)) * FrustumFarPlane;
        float ConeHeight = FMath::Cos(FMath::DegreesToRadians(maxFov)) * FrustumFarPlane;

        util::SphereGeometry(triangles, FrustumFarPlane, PhiStart, PhiLength, ThetaStart, ThetaLength);

        util::ConeGeometry(triangles, FVector(0.0f, 0.0f, 0.0f), ConeRadius, ConeHeight);

        /*
            ggggg min fov 0.98641971987165
    index-de5e1a2f.js:8686 ggggg max fov 1.0842567281137696
    index-de5e1a2f.js:8686 ggggg hscale 1
    index-de5e1a2f.js:8686 ggggg vscale 0.943549273038741
    index-de5e1a2f.js:8686 sphere radius 16
    index-de5e1a2f.js:8686 sphere length 1.0842567281137696
    index-de5e1a2f.js:8686 cone height 7.481117612060583
    index-de5e1a2f.js:8686 cone radius 14.143298033857484
        */

        UE_LOG(LogTemp, Log, TEXT("PhiStart = %f, PhiLength = %f, ThetaStart = %f, ThetaLength = %f"), PhiStart,
            PhiLength, ThetaStart, ThetaLength);
        UE_LOG(LogTemp, Log, TEXT("ConeRadius = %f, ConeHeight = %f"), ConeRadius, ConeHeight);
        UE_LOG(LogTemp, Log, TEXT("hScale = %f, vScale = %f"), hScale, vScale);
        UE_LOG(LogTemp, Log, TEXT("UpperFov = %f, LeftFov = %f"), UpperFov, LeftFov);

        FrustumComponent->SetRelativeScale3D(FVector(1.0f, hScale, vScale));

        auto PC = Cast<ADisplayPlayerController>(GetWorld()->GetFirstPlayerController());

        if (PC)
        {
            PC->OnEgoViewChange.AddUObject(this, &AFisheyeSensor::SwitchCamera);
        }

        // // Latitude angle range (from 0 to pi/2 for a half sphere)
        // float phiStart = 0.0f;                         // Top of the sphere
        // float phiEnd = M_PI / 2.0f;                    // Equator
        // float thetaStep = (2.0f * M_PI) / segments;    // Longitude angle range (0 to 2*pi)

        // // Generate vertices for the half sphere
        // std::vector<FVector> sphereVertices;

        // // Add the top point (apex of the half-sphere)
        // sphereVertices.push_back(FVector(ConeHeight, 0.0f, ConeRadius));

        // // Generate vertices in spherical coordinates
        // for (float phi = phiStart; phi <= phiEnd; phi += M_PI / (float) segments)
        // {
        //     for (int i = 0; i < segments; ++i)
        //     {
        //         float theta = i * thetaStep;

        //         // Convert spherical to Cartesian coordinates
        //         float x = SphereRadius * sin(phi) + ConeHeight;        // Radius * sin(phi)
        //         float y = SphereRadius * cos(phi) * cos(theta);    // Radius * cos(phi) * cos(theta)
        //         float z = ConeRadius * cos(phi) * sin(theta);    // Radius * cos(phi) * sin(theta)

        //         sphereVertices.push_back(FVector(x, y, z));    // Add the vertex to the list
        //     }
        // }

        // // Now we will generate triangles from these vertices:
        // // - The top center point and the points on the first latitude form triangles to the first ring of latitude.
        // // - For every two consecutive latitude rings, we connect the vertices to form the side triangles of the
        // // half-sphere.

        // // Generate triangles for the top of the sphere
        // for (int i = 1; i < segments; ++i)
        // {
        //     int nextIndex = i + 1;
        //     if (nextIndex == segments)
        //         nextIndex = 1;

        //     FCustomMeshTriangle tri;
        //     tri.Vertex0 = sphereVertices[0];
        //     tri.Vertex1 = sphereVertices[i];
        //     tri.Vertex2 = sphereVertices[nextIndex];
        //     // tri_angles.Add(tri);
        // }

        // // Generate triangles between latitude rings
        // int baseStartIndex = 1;    // Start after the top center
        // for (float phi = phiStart + M_PI / (float) segments; phi <= phiEnd; phi += M_PI / (float) segments)
        // {
        //     for (int i = 0; i < segments; ++i)
        //     {
        //         int nextIndex = (i + 1) % segments;

        //         FCustomMeshTriangle tri1;
        //         tri1.Vertex0 = sphereVertices[baseStartIndex + i];
        //         tri1.Vertex1 = sphereVertices[baseStartIndex + nextIndex];
        //         tri1.Vertex2 = sphereVertices[baseStartIndex + segments + i];
        //         tri_angles.Add(tri1);

        //         FCustomMeshTriangle tri2;
        //         tri2.Vertex0 = sphereVertices[baseStartIndex + nextIndex];
        //         tri2.Vertex1 = sphereVertices[baseStartIndex + segments + nextIndex];
        //         tri2.Vertex2 = sphereVertices[baseStartIndex + segments + i];
        //         tri_angles.Add(tri2);
        //     }

        //     baseStartIndex += segments;
        // }

        FrustumComponent->SetCustomMeshTriangles(triangles);
        FrustumComponent->RegisterComponent();
        FrustumComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
        UE_LOG(LogTemp, Log, TEXT("Draw Frustum"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Draw Frustum Failed!"));
    }
}

void AFisheyeSensor::SwitchCamera(const FName& CameraName)
{
    if (CameraName == "Camera_BirdView")
    {
        FrustumComponent->SetVisibility(true);
    }
    else if (CameraName == "Fisheye0")
    {
        FrustumComponent->SetVisibility(false);
    }
}

ISimActorInterface* AFisheyeSensor::Install(const FSensorConfig& _Config)
{
    ISimActorInterface* SimActor = Super::Install(_Config);

    /* Install previewCamera */
    if (SimActor)
    {
        // TODO: All simActors support install camera
        ATransportPawn* Transport = Cast<ATransportPawn>(SimActor);
        if (Transport)
        {
            if (Transport->InstallCamera(_Config.typeName + FString::FromInt(_Config.id), previewComponent))
            {
                // previewComponent->SetActive(true);
                auto hilpos = GetDisplayInstance()->nHILpos;
                if (hilpos.X >= 0 && hilpos.Y >= 0)
                {
                    Transport->SetDefaultCamera(_Config.typeName + FString::FromInt(_Config.id));

                    if (GEngine && GEngine->GameViewport)
                    {
                        if (GEngine->GameViewport->Viewport->IsFullscreen())
                        {
                            GEngine->GameViewport->HandleToggleFullscreenCommand();
                        }
                        GEngine->GameViewport->GetWindow()->MoveWindowTo(hilpos);
                        GEngine->GameViewport->HandleToggleFullscreenCommand();
                    }
                }
            }
            return Transport;
        }
    }
    return nullptr;
}

void AFisheyeSensor::Update(const FSensorInput& _Input, FSensorOutput& _Output)
{
    UE_LOG(LogTemp, Warning, TEXT("AFisheyeSensor::Update"));
    if (!renderTarget2D)    // sil
    {
        return;
    }
    const FFisheyeInput* CameraInput = Cast_Sim<const FFisheyeInput>(_Input);
    // 频率限制
    if (sensorConfig.frequency > 0 &&
        (float) (CameraInput->timeStamp - timeStamp) / 1000.f < 1.f / sensorConfig.frequency)
    {
        // UE_LOG(LogTemp, Warning, TEXT("%s: Camera frequency async, has return, TimeStamp is: %f"), *this->GetName(),
        // CameraInput->timeStamp);
        return;
    }
    timeStamp = CameraInput->timeStamp;
    double timeStamp_ego = CameraInput->timeStamp_ego;
    if (sensorConfig.installSlot == TEXT("C1") && CameraInput->timeStamp_tail > 0.001)
    {
        timeStamp_ego = CameraInput->timeStamp_tail;
    }

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
                UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: read raw image buf faild in texjpg"));
            }
        }
        else
        {
            FReadSurfaceDataFlags ReadPixelFlags(RCM_UNorm);
            FTextureRenderTarget2DResource* RTResource =
                (FTextureRenderTarget2DResource*) renderTarget2D->GetResource();
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
            UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: read image buf faild."));
        }
    };

    // 内存共享
    if (sharedWriter)
    {
        getRawBuff();

        if (BitData.empty() || !sharedWriter->write(BitData, timeStamp_ego))
        {
            UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: sharedWriter write faild."));
        }
    }
    // 显存共享
    if (sharedWriterGpu && cuda)
    {
        std::vector<uint8> buf(1024);
        if (texJpg->IpcShare(buf.data() + 8))
        {
            int w = sensorConfig.res_Horizontal, h = sensorConfig.res_Vertical;
            memcpy(buf.data(), &w, sizeof(int));
            memcpy(buf.data() + 4, &h, sizeof(int));
            // UE_LOG(LogTemp, Log, TEXT("CameraSensor: shared gpu write %f."), timeStamp_ego);
            if (!sharedWriterGpu->write(buf, timeStamp_ego))
            {
                UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: sharedgpu write faild."));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: share ipc faild."));
        }
    }

    // 落盘或发布
    TArray64<uint8_t> jpegbuf;
    if (!sensorConfig.savePath.IsEmpty() || public_msg)
    {
        // jpeg图像，优先cuda编码，否则用ue自带CPU编码
        if (imageFormat == EImageFormat::JPEG)
        {
            if (cuda)
            {
                if (!texJpg->JpegEncoding(jpegbuf))
                {
                    UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: read jpeg image buf faild in texjpg"));
                }
            }

            // cuda失败，用ue自带CPU编码
            if (jpegbuf.Num() == 0)
            {
                getRawBuff();

                if (BitData.empty())
                {
                    UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: read image buf faild."));
                }
                else
                {
                    IImageWrapperModule& ImageWrapperModule =
                        FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
                    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
                    if (ImageWrapper->SetRaw(BitData.data(), BitData.size(), sensorConfig.res_Horizontal,
                            sensorConfig.res_Vertical, ERGBFormat::BGRA, 8))
                    {
                        jpegbuf = ImageWrapper->GetCompressed(imageQuality);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: encode jpeg faild."));
                    }
                }
            }
        }
        else
        {
            getRawBuff();
        }

        if (!sensorConfig.savePath.IsEmpty())
        {
            if (jpegbuf.Num() > 0)
            {
                FString SavePath = sensorConfig.savePath + imageName + TEXT("_") + FString::FromInt(sensorConfig.id) +
                                   TEXT("_") + FString::SanitizeFloat(timeStamp_ego) + TEXT(".jpg");
                GetDisplayInstance()->GetSaveDataHandle()->SaveJPG(jpegbuf, SavePath);
            }
            else
            {
                Save();
            }
        }
        // public msg
        if (public_msg)
        {
            sim_msg::CameraRaw craw;
            craw.set_id(sensorConfig.id);
            craw.set_timestamp(timeStamp_ego);
            if (jpegbuf.Num() > 0)
            {
                craw.set_type("JPEG");
                craw.set_image_data(jpegbuf.GetData(), jpegbuf.Num());
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
            craw.set_width(sensorConfig.res_Horizontal);
            craw.set_height(sensorConfig.res_Vertical);
            craw.SerializeToString(&_Output.serialize_string);
        }
    }
}

bool AFisheyeSensor::Save()
{
    if (sensorConfig.savePath.IsEmpty())
    {
        return true;
    }
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(imageFormat);

    if (ImageWrapper->SetRaw(dataBuf.buffer.data(), dataBuf.buffer.size(), sensorConfig.res_Horizontal,
            sensorConfig.res_Vertical, ERGBFormat::BGRA, 8))
    {
        if (FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*sensorConfig.savePath))
        {
            FString SaveDir = sensorConfig.savePath + imageName + TEXT("_") + FString::FromInt(sensorConfig.id) +
                              TEXT("_") + FString::SanitizeFloat(timeStamp) + TEXT(".") +
                              TEXT("jpg") /*FString(GETENUMSTRING("EImageFormat",
                                             imageFormat)).ToLower()*/
                ;
            GetDisplayInstance()->GetSaveDataHandle()->SaveJPG(ImageWrapper->GetCompressed(imageQuality), SaveDir);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("CameraSensorComponent WARN: Directory dont Exists!"));
        }
    }
    return true;
}

void AFisheyeSensor::SetPostProcessSettings(FPostProcessSettings& PostProcessSettings, float screen_scale)
{
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

    // bloom
    // PostProcessSettings.bOverride_BloomIntensity = 1;
    // PostProcessSettings.BloomIntensity = sensorConfig.Bloom;

    // if (sensorConfig.LensFlares > 0)
    // {
    //     PostProcessSettings.bOverride_LensFlareIntensity = 1;
    //     PostProcessSettings.LensFlareIntensity = sensorConfig.LensFlares * 16.0;
    // }

    // // Exposure;
    // PostProcessSettings.bOverride_AutoExposureBias = sensorConfig.Exposure == 0;
    // PostProcessSettings.AutoExposureBias = sensorConfig.Compensation * sensorConfig.Transmittance * 0.01;
    // if (sensorConfig.Exposure == 1)
    // {
    //     PostProcessSettings.bOverride_AutoExposureMethod = 1;
    //     PostProcessSettings.AutoExposureMethod = AEM_Manual;
    //     PostProcessSettings.bOverride_CameraISO = 1;
    //     PostProcessSettings.CameraISO = sensorConfig.ISO;
    //     PostProcessSettings.bOverride_CameraShutterSpeed = 1;
    //     PostProcessSettings.CameraShutterSpeed = sensorConfig.ShutterSpeed;
    //     PostProcessSettings.bOverride_DepthOfFieldFstop = 1;
    //     PostProcessSettings.DepthOfFieldFstop = sensorConfig.Aperture;
    // }

    // // Motion blur
    // PostProcessSettings.bOverride_MotionBlurAmount = 1;
    // PostProcessSettings.MotionBlurAmount = sensorConfig.motionBlur_Amount;

    // // vignetting
    // PostProcessSettings.bOverride_VignetteIntensity = true;
    // PostProcessSettings.VignetteIntensity = sensorConfig.vignette_Intensity;

    // // Color Grading
    // PostProcessSettings.bOverride_WhiteTemp = 1;
    // PostProcessSettings.bOverride_WhiteTint = 1;
    // PostProcessSettings.WhiteTemp = sensorConfig.ColorTemperature;
    // PostProcessSettings.WhiteTint = sensorConfig.WhiteHint;

    // if (sensorConfig.color_gray)
    // {
    //     PostProcessSettings.bOverride_ColorSaturation = 1;
    //     PostProcessSettings.ColorSaturation.W = 0;
    // }
}
