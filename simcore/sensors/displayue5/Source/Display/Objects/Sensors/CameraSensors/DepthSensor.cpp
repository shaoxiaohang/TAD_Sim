#include "DepthSensor.h"

#include "CineCameraComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/DisplayGameInstance.h"
#include "Framework/SaveDataThread.h"
#include "HAL/PlatformFileManager.h"
#include "HadMap/Public/HadmapManager.h"
#include "Misc/FileHelper.h"
#include "Runtime/Engine/Classes/Components/SceneCaptureComponent2D.h"
#include "Runtime/ImageWrapper/Public/IImageWrapperModule.h"
#include "Kismet/KismetSystemLibrary.h"

ADepthSensor::ADepthSensor()
{
    str_PostProcess =
        TEXT("Material'/Game/SensorSim/Camera/Material/Mat_Camera_Capture_depth.Mat_Camera_Capture_depth'");

    targetGamma = 1;

    imageName = TEXT("Depth");
    imageFormat = EImageFormat::PNG;

    bCaptureEveryFrame = true;
}
ADepthSensor::~ADepthSensor()
{
}

bool ADepthSensor::Init(const FSensorConfig& _Config)
{
    if (!ACameraSensor::Init(_Config))
        return false;

    captureComponent->PostProcessSettings.bOverride_AutoExposureBias = false;
    captureComponent->PostProcessSettings.bOverride_ColorGamma = true;
    captureComponent->PostProcessSettings.ColorGamma = FVector4(1.0, 1.0, 1.0, 1.0);
    // previewComponent->PostProcessSettings.bOverride_AutoExposureBias = false;
    // previewComponent->PostProcessSettings.bOverride_ColorGamma = true;
    // previewComponent->PostProcessSettings.ColorGamma = FVector4(1.0, 1.0, 1.0, 1.0);
    UseUnlitShowFlags(captureComponent->ShowFlags);
    UKismetSystemLibrary::ExecuteConsoleCommand(GetWorld(), FString("g.TimeoutForBlockOnRenderFence 300000"));
    return true;
}

void ADepthSensor::Update(const FSensorInput& _Input, FSensorOutput& _Output)
{
    if (!renderTarget2D)
    {
        return;
    }
    const FCameraInput* CameraInput = Cast_Sim<const FCameraInput>(_Input);
    // 频率限制
    if (frequency > 0 && (CameraInput->timeStamp - timeStamp) < 999.9999999 / frequency)
    {
        return;
    }
    timeStamp = CameraInput->timeStamp;
    double timeStamp_ego = CameraInput->timeStamp_ego;

    if (!bCaptureEveryFrame)
    {
        UE_LOG(LogTemp, Log, TEXT("DepthSensor: CaptureScene"));
        captureComponent->CaptureScene();
        FlushRenderingCommands();
    }


    // if (egoActor)
    // {
    //     FTransform transform = egoActor->GetActorTransform();
    //     FVector location = transform.GetLocation();
    //     location /= 100;
    //     FQuat quat = FQuat::MakeFromEuler(FVector(
    //         transform.GetRotation().Euler().X, transform.GetRotation().Euler().Y, transform.GetRotation().Euler().Z));
    //     FString posedata = FString::Printf(TEXT("%f %f %f %f %f %f %f %f\n"), timeStamp, location.X, location.Y,
    //         location.Z, quat.X, quat.Y, quat.Z, quat.W);
    //     FString poseFile = TEXT("/home/aaa/workspace/hsim/pose.txt");
    //     FFileHelper::SaveStringToFile(
    //         posedata, *poseFile, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
    // }

    FReadSurfaceDataFlags ReadPixelFlags(RCM_UNorm);
    FTextureRenderTarget2DResource* RTResource =
        (FTextureRenderTarget2DResource*) renderTarget2D->GameThread_GetRenderTargetResource();

    std::vector<uint16_t> DepthData;

    if (RTResource)
    {
        TArray<FColor> Pixels;
        RTResource->ReadPixels(Pixels, ReadPixelFlags);
        DepthData.resize(Pixels.Num());

        ParallelFor(Pixels.Num(), [&](int Index) {
            FColor Color = Pixels[Index];
            float depth = (Color.R * 65536.0 + Color.G * 256.0 + Color.B) * 0.0001;
            uint16_t depth_uint_16 = depth * 256;
            if (depth_uint_16 >= 256 * 256)
            {
                // UE_LOG(LogTemp, Log, TEXT("DepthSensor: Depth is %f"), depth);
                depth_uint_16 = 0;
            }
            DepthData[Index] = depth_uint_16;
        });
    }

    if (public_msg)
    {
        sim_msg::CameraRaw craw;
        craw.set_id(id);
        craw.set_timestamp(timeStamp_ego);
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
        const void* DataPtr = static_cast<const void*>(DepthData.data());
        // size_t DataSize = DepthImage.total() * DepthImage.elemSize();
        // UE_LOG(LogTemp, Log, TEXT("DepthSensor: DataSize is %d"), DataSize);
        craw.set_image_data(DataPtr, DepthData.size() * sizeof(uint16_t));

        double X = 0, Y = 0, Z = 0;
        hadmapue4::HadmapManager::Get()->LocalToLonLat(GetActorLocation(), X, Y, Z);
        craw.mutable_pose()->set_longitude(X);
        craw.mutable_pose()->set_latitude(Y);
        craw.mutable_pose()->set_altitude(Z);
        auto Rot = GetActorRotation();
        craw.mutable_pose()->set_roll(Rot.Roll * PI / 180.f);
        craw.mutable_pose()->set_pitch(-Rot.Pitch * PI / 180.f);
        craw.mutable_pose()->set_yaw(-(Rot.Yaw + 90.f) * PI / 180.f);
        craw.set_width(imageRes.X);
        craw.set_height(imageRes.Y);
        craw.SerializeToString(&_Output.serialize_string);
    }
}