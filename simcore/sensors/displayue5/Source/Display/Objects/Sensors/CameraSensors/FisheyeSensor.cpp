#include "FisheyeSensor.h"

#include "Engine/TextureRenderTarget2D.h"
#include "FisheyeCameraModel.h"
#include "FisheyeSceneViewExtension.h"
#include "HadMap/Public/HadmapManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Objects/Transports/TransportPawn.h"
#include "Runtime/Engine/Classes/Components/SceneCaptureComponent2D.h"
#include "Runtime/ImageWrapper/Public/IImageWrapper.h"
#include "Runtime/ImageWrapper/Public/IImageWrapperModule.h"
#include "Utils/DataFunctionLibrary.h"
#include "Utils/ProjectionUtil.h"
#include "sensor_raw.pb.h"

AFisheyeSensor::AFisheyeSensor()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(FName(TEXT("Root")));
    PrimaryActorTick.bCanEverTick = true;
    bCaptureEveryFrame = false;

    bUseMultipleCapture = false;

    CaptureRenderTarget =
        CreateDefaultSubobject<UTextureRenderTarget2D>(FName(*FString::Printf(TEXT("CaptureRenderTarget"))));
    CaptureRenderTarget->CompressionSettings = TextureCompressionSettings::TC_Default;
    CaptureRenderTarget->SRGB = false;
    CaptureRenderTarget->bAutoGenerateMips = false;
    CaptureRenderTarget->bGPUSharedFlag = true;
    CaptureRenderTarget->AddressX = TextureAddress::TA_Clamp;
    CaptureRenderTarget->AddressY = TextureAddress::TA_Clamp;
    CaptureRenderTarget->Filter = TextureFilter::TF_Nearest;

    // 45.0 = 45.0f;
}

void AFisheyeSensor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bCaptureEveryFrame)
    {
        if (PostProcessMaterial)
        {
            auto CameraDir = GetActorForwardVector();
            PostProcessMaterial->SetVectorParameterValue(FName(TEXT("CameraDir")), CameraDir);
            UE_LOG(LogTemp, Display, TEXT("FisheyeSensor: CameraDir %f %f %f"), CameraDir.X, CameraDir.Y, CameraDir.Z);
            // CaptureComponent2D->MarkRenderStateDirty();
        }
        if (bUseMultipleCapture)
        {
            for (int i = 0; i < CaptureComponents.Num(); i++)
            {
                USceneCaptureComponent2D* SceneCaptureComponent2D = CaptureComponents[i];
                SceneCaptureComponent2D->CaptureScene();
            }
        }
        else
        {
            for (int i = 0; i < CameraLayouts.Num(); i++)
            {
                const FCameraLayout CameraLayout = CameraLayouts[i];
                CaptureComponent2D->CustomProjectionMatrix = CameraLayout.ProjectionMatrix;
                CaptureComponent2D->SetRelativeRotation(CameraLayout.Direction);
                CaptureComponent2D->CaptureScene();
            }
        }
    }
}

bool AFisheyeSensor::Init(const FSensorConfig& _Config)
{
    ASensorActor::Init(_Config);

    GConfig->GetBool(TEXT("Sensor"), TEXT("PublicMsg"), bPublicMsg, GGameIni);

    const FFisheyeConfig* NewCameraSensorConfig = Cast_Sim<const FFisheyeConfig>(_Config);

    sensorConfig = *NewCameraSensorConfig;
    if (!NewCameraSensorConfig)
    {
        UE_LOG(LogTemp, Warning, TEXT("AFisheyeSensor: Cant Cast to FCameraConfig!"));
        return false;
    }

    bIsRGB = sensorConfig.bIsRGB;

    // ID
    id = NewCameraSensorConfig->id;
    // frequency
    frequency = NewCameraSensorConfig->frequency;

    imageRes.X = NewCameraSensorConfig->res_Horizontal;
    imageRes.Y = NewCameraSensorConfig->res_Vertical;

    UE_LOG(LogTemp, Display, TEXT("AFisheyeSensor: SensorConfig %f %f"), imageRes.X, imageRes.Y);
    UE_LOG(LogTemp, Display, TEXT("AFisheyeSensor: SensorConfig intrinsic_Matrix %d"),
        sensorConfig.intrinsic_Matrix.Num());
    UE_LOG(LogTemp, Display, TEXT("AFisheyeSensor: SensorConfig distortion_Parameters %d"),
        sensorConfig.distortion_Parameters.Num());

    // 获取内参矩阵的值
    const auto& im = sensorConfig.intrinsic_Matrix;
    Fx = im[0];
    Fy = im[4];
    Cx = im[2];
    Cy = im[5];
    UE_LOG(LogTemp, Display, TEXT("AFisheyeSensor: intrinsic_Matrix %f %f %f %f"), Fx, Fy, Cx, Cy);
    if (NewCameraSensorConfig->distortion_Parameters.Num() > 0)
        K1 = NewCameraSensorConfig->distortion_Parameters[0];
    if (NewCameraSensorConfig->distortion_Parameters.Num() > 1)
        K2 = NewCameraSensorConfig->distortion_Parameters[1];
    if (NewCameraSensorConfig->distortion_Parameters.Num() > 2)
        K3 = NewCameraSensorConfig->distortion_Parameters[2];
    if (NewCameraSensorConfig->distortion_Parameters.Num() > 3)
        K4 = NewCameraSensorConfig->distortion_Parameters[3];
    if (Fx <= 0 || Fy <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("AFisheyeSensor: Fx Fy errror %f %f"), Fx, Fy);
        return false;
    }

    // K1 = 0.0;
    // K2 = 0.0;
    // K3 = 0.0;
    // K4 = 0.0;

    Fov_H = 2.0f * FMath::RadiansToDegrees(FMath::Atan2(imageRes.X, 2.0f * Fx));
    Fov_V = 2.0f * FMath::RadiansToDegrees(FMath::Atan2(imageRes.Y, 2.0f * Fy));

    UE_LOG(LogTemp, Log, TEXT("FisheyeSensor: Fov_H=%f, Fov_V=%f"), Fov_H, Fov_V);

    SetupDistortionMap();

    InitSceneCaptureComponent();

    SceneViewExtension = MakeShared<FFisheyeSceneViewExtension>(this);
    CaptureComponent2D->SceneViewExtensions.Add(SceneViewExtension);

    // UseLitShowFlags(CaptureComponent2D->ShowFlags);

    // update scene capture
    // CaptureComponent2D->ShowFlags.SetVignette(false);
    // CaptureComponent2D->ShowFlags.SetTemporalAA(false);
    // CaptureComponent2D->ShowFlags.SetLumenDetailTraces(false);
    // CaptureComponent2D->ShowFlags.SetLumenShortRangeAmbientOcclusion(false);
    // CaptureComponent2D->ShowFlags.SetLumenGlobalIllumination(false);
    // CaptureComponent2D->ShowFlags.SetLumenReflections(false);
    // CaptureComponent2D->ShowFlags.SetLumenScreenTraces(false);
    // CaptureComponent2D->PostProcessSettings.bOverride_AutoExposureMethod = true;
    // CaptureComponent2D->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
    // CaptureComponent2D->PostProcessSettings.bOverride_AutoExposureBias = true;
    // CaptureComponent2D->PostProcessSettings.AutoExposureBias = -2;
    // CaptureComponent2D->PostProcessSettings.bOverride_CameraISO = true;
    // CaptureComponent2D->PostProcessSettings.CameraISO = 30;
    CaptureComponent2D->UpdateContent();

    if (bUseMultipleCapture)
    {
        CreateOtherCaptureComponents();
    }

    return true;
}

void AFisheyeSensor::Update(const FSensorInput& _Input, FSensorOutput& _Output)
{
    if (bIsRGB)
    {
        UseLitShowFlags(CaptureComponent2D->ShowFlags, !bIsRGB);
    }
    else
    {
        UseUnlitShowFlags(CaptureComponent2D->ShowFlags);
    }

    const FFisheyeInput* CameraInput = Cast_Sim<const FFisheyeInput>(_Input);
    // 频率限制
    if (frequency > 0 && (CameraInput->timeStamp - timeStamp) < 999.9999999 / frequency)
    {
        // UE_LOG(LogTemp, Warning, TEXT("%s: Camera frequency async, has return, Frequency %f
        // TimeStamp is: %f Camera TimeStamp %f"), *this->GetName(), frequency, timeStamp,
        // CameraInput->timeStamp);
        // UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor:  return, Frequency %f TimeStamp is: %f Camera TimeStamp %f"),
        //     frequency, timeStamp, CameraInput->timeStamp);
        return;
    }
    timeStamp = CameraInput->timeStamp;
    double timeStamp_ego = CameraInput->timeStamp_ego;

    if (!bCaptureEveryFrame)
    {
        if (PostProcessMaterial)
        {
            auto CameraDir = GetActorForwardVector();
            PostProcessMaterial->SetVectorParameterValue(FName(TEXT("CameraDir")), CameraDir);
            // CameraDir = CameraDir.GetSafeNormal();
            // UE_LOG(LogTemp, Display, TEXT("FisheyeSensor: CameraDir %f %f %f"), CameraDir.X, CameraDir.Y,
            // CameraDir.Z);
            // CaptureComponent2D->MarkRenderStateDirty();
        }
        if (bUseMultipleCapture)
        {
            for (int i = 0; i < CaptureComponents.Num(); i++)
            {
                USceneCaptureComponent2D* SceneCaptureComponent2D = CaptureComponents[i];
                SceneCaptureComponent2D->CaptureScene();
            }
        }
        else
        {
            for (int i = 0; i < CameraLayouts.Num(); i++)
            {
                const FCameraLayout CameraLayout = CameraLayouts[i];
                CaptureComponent2D->CustomProjectionMatrix = CameraLayout.ProjectionMatrix;
                CaptureComponent2D->SetRelativeRotation(CameraLayout.Direction);
                CaptureComponent2D->CaptureScene();
                FlushRenderingCommands();
            }
        }
    }

    if (bPublicMsg)
    {
        // FlushRenderingCommands();

        std::vector<uint8> BitData;

        TArray64<uint8_t> imgbuf;

        TArray<uint8>& Data = SceneViewExtension->GetReadbackData();
        const int NumPixels = imageRes.X * imageRes.Y;
        FColor* Bitmap = reinterpret_cast<FColor*>(Data.GetData());
        BitData.resize(NumPixels * 4);
        memcpy(BitData.data(), Bitmap, NumPixels * 4);

        if (BitData.empty())
        {
            UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: read image buf faild."));
        }
        else
        {
            //UE_LOG(LogTemp, Display, TEXT("BitData.size() %d"), BitData.size());
            if (sensorConfig.typeName == "Fisheye")
            {
                if (!PublishRGB(_Output, BitData, timeStamp_ego))
                {
                    UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: publish rgb faild."));
                }
            }
            else if (sensorConfig.typeName == "FisheyeDepth")
            {
                if (!PublishDepth(_Output, Bitmap, timeStamp_ego))
                {
                    UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: publish depth faild."));
                }
            }
            else if (sensorConfig.typeName == "FisheyeSemantic")
            {
                if (!PublishSemantic(_Output, BitData, timeStamp_ego))
                {
                    UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: publish semantic faild."));
                }
            }
            else if (sensorConfig.typeName == "FisheyeNormal")
            {
                if (!PublishNormal(_Output, BitData, timeStamp_ego))
                {
                    UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: publish normal faild."));
                }
            }

            // IImageWrapperModule& ImageWrapperModule =
            //     FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
            // TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
            // if (ImageWrapper->SetRaw(BitData.data(), sizeof(FColor) * imageRes.X * imageRes.Y, imageRes.X,
            // imageRes.Y,
            //         ERGBFormat::BGRA, 8))
            // {
            //     imgbuf = ImageWrapper->GetCompressed(ImageQuality);
            // }
            // if (imgbuf.Num() > 0)
            // {
            //     sim_msg::CameraRaw craw;
            //     craw.set_id(id);
            //     craw.set_timestamp(timeStamp_ego);
            //     craw.set_type("JPEG");
            //     craw.set_image_data(imgbuf.GetData(), imgbuf.Num());

            //     double X = 0, Y = 0, Z = 0;
            //     hadmapue4::HadmapManager::Get()->LocalToLonLat(GetActorLocation(), X, Y, Z);
            //     craw.mutable_pose()->set_longitude(X);
            //     craw.mutable_pose()->set_latitude(Y);
            //     craw.mutable_pose()->set_altitude(Z);
            //     auto Rot = GetActorRotation();
            //     craw.mutable_pose()->set_roll(Rot.Roll * PI / 180.f);
            //     craw.mutable_pose()->set_pitch(-Rot.Pitch * PI / 180.f);
            //     craw.mutable_pose()->set_yaw(-(Rot.Yaw + 90.f) * PI / 180.f);
            //     craw.set_width(imageRes.X);
            //     craw.set_height(imageRes.Y);
            //     craw.SerializeToString(&_Output.serialize_string);

            //     UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: send image buf success."));
            // }
            // else
            // {
            //     UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: read image buf faild."));
            // }
        }
    }
}

ISimActorInterface* AFisheyeSensor::Install(const FSensorConfig& _Config)
{
    ISimActorInterface* SimActor = Super::Install(_Config);

    if (SimActor)
    {
        ATransportPawn* Transport = Cast<ATransportPawn>(SimActor);
        AActor* Ego = Cast<AActor>(SimActor);
        IgnoreActor(Ego);
        return Transport;
    }
    return nullptr;
}

void AFisheyeSensor::IgnoreActor(AActor* actor)
{
    if (actor)
    {
        if (bUseMultipleCapture)
        {
            for (int i = 0; i < CaptureComponents.Num(); i++)
            {
                USceneCaptureComponent2D* SceneCaptureComponent2D = CaptureComponents[i];
                SceneCaptureComponent2D->HiddenActors.Add(actor);
            }
        }
        else
        {
            if (CaptureComponent2D)
            {
                CaptureComponent2D->HiddenActors.Add(actor);
            }
        }
    }
}

bool AFisheyeSensor::PublishRGB(FSensorOutput& _Output, const std::vector<uint8>& BitData, double timeStamp_ego)
{
    TArray64<uint8_t> imgbuf;
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
    if (ImageWrapper && ImageWrapper->SetRaw(BitData.data(), sizeof(FColor) * imageRes.X * imageRes.Y, imageRes.X,
                            imageRes.Y, ERGBFormat::BGRA, 8))
    {
        imgbuf = ImageWrapper->GetCompressed(ImageQuality);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("CameraSensor: encode jpeg faild."));
        return false;
    }
    sim_msg::CameraRaw craw;
    craw.set_id(id);
    craw.set_timestamp(timeStamp_ego);
    //UE_LOG(LogTemp, Display, TEXT("AFisheyeSensor: public rgb raw timestamp %f"), timeStamp_ego);
    if (imgbuf.Num() > 0)
    {
        craw.set_type("JPEG");
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
    craw.set_width(imageRes.X);
    craw.set_height(imageRes.Y);
    craw.SerializeToString(&_Output.serialize_string);

    return true;
}

bool AFisheyeSensor::PublishDepth2(FSensorOutput& _Output, const std::vector<uint8>& BitData, double timeStamp_ego)
{
    TArray64<uint8_t> imgbuf;
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
    if (ImageWrapper && ImageWrapper->SetRaw(BitData.data(), sizeof(FColor) * imageRes.X * imageRes.Y, imageRes.X,
                            imageRes.Y, ERGBFormat::BGRA, 8))
    {
        imgbuf = ImageWrapper->GetCompressed(ImageQuality);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("CameraSensor: encode jpeg faild."));
        return false;
    }
    sim_msg::CameraRaw craw;
    craw.set_id(id);
    craw.set_timestamp(timeStamp_ego);
    //UE_LOG(LogTemp, Display, TEXT("AFisheyeSensor: public rgb raw timestamp %f image size %d"), timeStamp_ego,
      //  imgbuf.Num());
    if (imgbuf.Num() > 0)
    {
        craw.set_type("PNG");
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
    craw.set_width(imageRes.X);
    craw.set_height(imageRes.Y);
    craw.SerializeToString(&_Output.serialize_string);

    return true;
}

bool AFisheyeSensor::PublishDepth(FSensorOutput& _Output, FColor* Pixels, double timeStamp_ego)
{
    if (!Pixels)
    {
        return false;
    }

    // TArray<FColor> BitMap;
    // BitMap.SetNum(BitData.size() / 4);
    // memcpy(BitMap.GetData(), BitData.data(), BitData.size());

    std::vector<uint16_t> DepthData;
    int NumPixels = imageRes.X * imageRes.Y;
    DepthData.resize(NumPixels);
    ParallelFor(NumPixels, [&](int Index) {
        FColor Color = Pixels[Index];
        float depth = (Color.R * 65536.0 + Color.G * 256.0 + Color.B) * 0.0001;
        uint16_t depth_uint_16 = depth * 256;
        if (depth_uint_16 >= 256 * 256)
        {
            //UE_LOG(LogTemp, Log, TEXT("DepthSensor: Depth is %f"), depth);
            depth_uint_16 = 0;
        }
        DepthData[Index] = depth_uint_16;
    });

    // UE_LOG(LogTemp, Display, TEXT("BitMap.size() %d"), BitMap.Num());

    sim_msg::CameraRaw craw;
    craw.set_id(id);
    craw.set_timestamp(timeStamp_ego);
    craw.set_type("PNG");

    //UE_LOG(LogTemp, Display, TEXT("AFisheyeSensor: public depth raw timestamp %f"), timeStamp_ego);

    const void* DataPtr = static_cast<const void*>(DepthData.data());
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

    return true;
}

bool AFisheyeSensor::PublishSemantic(FSensorOutput& _Output, const std::vector<uint8>& BitData, double timeStamp_ego)
{
    TArray64<uint8_t> imgbuf;

    TArray<uint8_t> BitMap;
    BitMap.SetNum(BitData.size() / 4);
    for (int i = 0; i < BitMap.Num(); i++)
    {
        BitMap[i] = BitData[i * 4];
    }
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
    if (ImageWrapper && ImageWrapper->SetRaw(BitMap.GetData(), sizeof(uint8_t) * imageRes.X * imageRes.Y, imageRes.X,
                            imageRes.Y, ERGBFormat::Gray, 8))
    {
        imgbuf = ImageWrapper->GetCompressed();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: encode semantic faild."));
        return false;
    }

    //UE_LOG(LogTemp, Display, TEXT("AFisheyeSensor: public semantic raw timestamp %f"), timeStamp_ego);

    sim_msg::CameraRaw craw;
    craw.set_id(id);
    craw.set_timestamp(timeStamp_ego);
    craw.set_type("PNG");
    craw.set_image_data(imgbuf.GetData(), imgbuf.Num());

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

    return true;
}

void AFisheyeSensor::SaveDirectionMapToTxt(const TArray<FVector>& DirectionMap, const int& Width, const int& Height)
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    const FString FileName = FPaths::Combine(FPaths::ProjectDir(), TEXT("_fisheye"), TEXT("DirectionMap.txt"));
    FString FullPath = PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*FileName);

    FString PathPart, FileNamePart, ExtentionPart;
    FPaths::Split(FullPath, PathPart, FileNamePart, ExtentionPart);

    if (!PlatformFile.DirectoryExists(*PathPart))
        PlatformFile.CreateDirectoryTree(*PathPart);

    TArray<FString> Lines;
    for (int j = 0; j < Height; j++)
    {
        FString Line = TEXT("");
        for (int i = 0; i < Width; i++)
        {
            int index = i + j * Width;
            FVector DirectionRay = DirectionMap[index].GetUnsafeNormal();
            Line += FString::SanitizeFloat(DirectionRay.X) + TEXT(" ");
        }
        Lines.Emplace(Line);
    }
    FFileHelper::SaveStringArrayToFile(Lines, *FullPath);
}

bool AFisheyeSensor::PublishNormal(FSensorOutput& _Output, const std::vector<uint8>& BitData, double timeStamp_ego)
{
    TArray64<uint8_t> imgbuf;
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
    if (ImageWrapper && ImageWrapper->SetRaw(BitData.data(), sizeof(FColor) * imageRes.X * imageRes.Y, imageRes.X,
                            imageRes.Y, ERGBFormat::BGRA, 8))
    {
        imgbuf = ImageWrapper->GetCompressed(ImageQuality);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("FisheyeSensor: encode normal faild."));
        return false;
    }
    sim_msg::CameraRaw craw;
    craw.set_id(id);
    craw.set_timestamp(timeStamp_ego);
    //UE_LOG(LogTemp, Display, TEXT("AFisheyeSensor: public normal raw timestamp %f"), timeStamp_ego);
    if (imgbuf.Num() > 0)
    {
        craw.set_type("PNG");
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
    craw.set_width(imageRes.X);
    craw.set_height(imageRes.Y);
    craw.SerializeToString(&_Output.serialize_string);

    return true;
}

void AFisheyeSensor::InitSceneCaptureComponent()
{
    FString str_PostProcess = sensorConfig.PostProcessMaterial;

    if (!str_PostProcess.IsEmpty())
    {
        // 加载材质
        UMaterial* NewMat = LoadObject<UMaterial>(NULL, *str_PostProcess);
        if (!NewMat)
        {
            UE_LOG(LogTemp, Warning, TEXT("AFisheyeSensor: Cant get Material!"));
            return;
        }
        // 创建材质实例动态
        PostProcessMaterial = UMaterialInstanceDynamic::Create(NewMat, this);
        if (!PostProcessMaterial)
        {
            UE_LOG(LogTemp, Warning, TEXT("AFisheyeSensor: Cant get MaterialInstanceDynamic!"));
            return;
        }
    }

    const bool bInForceLinearGamma = !bEnablePostProcessingEffects;

    CaptureRenderTarget->InitCustomFormat(
        imageRes.X * SourceImageScaleFactor, imageRes.Y * SourceImageScaleFactor, PF_B8G8R8A8, bInForceLinearGamma);

    CaptureComponent2D =
        NewObject<USceneCaptureComponent2D>(this, FName(*FString::Printf(TEXT("SceneCaptureComponent2D"))));
    check(IsValid(CaptureComponent2D));

    CaptureComponent2D->SetupAttachment(RootComponent);
    CaptureComponent2D->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
    CaptureComponent2D->bAlwaysPersistRenderingState = true;

    CaptureComponent2D->Deactivate();
    CaptureComponent2D->TextureTarget = CaptureRenderTarget;

    CaptureComponent2D->CaptureSource = ESceneCaptureSource::SCS_FinalToneCurveHDR;
    CaptureComponent2D->FOVAngle = Fov_H;
    CaptureComponent2D->bUseRayTracingIfEnabled = true;
    CaptureComponent2D->bCaptureOnMovement = bCaptureEveryFrame;
    CaptureComponent2D->bCaptureEveryFrame = bCaptureEveryFrame;

    // Use GlobalPostProcess as post process settings if available, otherwise use default settings
    APostProcessVolume* GlobalPostProcess = UDataFunctionLibrary::GetGlobalPostProcess(this);
    if (GlobalPostProcess != nullptr)
    {
        UE_LOG(LogTemp, Log, TEXT("Camera Using Global Post Process Config"));
        CaptureComponent2D->PostProcessSettings = GlobalPostProcess->Settings;

        CaptureComponent2D->PostProcessSettings.bOverride_MotionBlurAmount = 1;
        CaptureComponent2D->PostProcessSettings.MotionBlurAmount = 0;

        // CaptureComponent2D->PostProcessSettings.bOverride_AntiAliasingMethod = true;
        // CaptureComponent2D->PostProcessSettings.AntiAliasingMethod = EAntiAliasingMethod::AAM_FXAA; //
        // 也可选择AAM_FXAA CaptureComponent2D->PostProcessSettings.bOverride_TemporalAA_Upsampling = true;
        // CaptureComponent2D->PostProcessSettings.TemporalAA_Upsampling = 0.8f; //
        // 控制锐化程度‌:ml-citation{ref="5,6" data="citationList"}

        // CaptureComponent2D->PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
        // CaptureComponent2D->PostProcessSettings.DynamicGlobalIlluminationMethod =
        //     EDynamicGlobalIlluminationMethod::Lumen;
        // CaptureComponent2D->PostProcessSettings.bOverride_ReflectionMethod = true;
        // CaptureComponent2D->PostProcessSettings.ReflectionMethod = EReflectionMethod::Lumen;

        // CaptureComponent2D->PostProcessSettings.bOverride_VignetteIntensity = true;
        // CaptureComponent2D->PostProcessSettings.VignetteIntensity = 0.0f;
        // // CaptureComponent2D->ShowFlags.SetVignette(false);
        // // CaptureComponent2D->ShowFlags.SetTemporalAA(false);
        // // CaptureComponent2D->ShowFlags.SetLumenDetailTraces(false);
        // // CaptureComponent2D->ShowFlags.SetLumenShortRangeAmbientOcclusion(false);
        // // CaptureComponent2D->ShowFlags.SetLumenGlobalIllumination(false);
        // // CaptureComponent2D->ShowFlags.SetLumenReflections(false);
        // // CaptureComponent2D->ShowFlags.SetLumenScreenTraces(false);
        // // CaptureComponent2D->PostProcessSettings.bOverride_AutoExposureMethod = true;
        // // CaptureComponent2D->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
        // CaptureComponent2D->PostProcessSettings.bOverride_AutoExposureBias = true;
        // CaptureComponent2D->PostProcessSettings.AutoExposureBias = 0;
        // // CaptureComponent2D->PostProcessSettings.bOverride_CameraISO = true;
        // // CaptureComponent2D->PostProcessSettings.CameraISO = 30;

        // // 设置自动曝光的最小和最大 EV100
        // CaptureComponent2D->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
        // CaptureComponent2D->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
        // CaptureComponent2D->PostProcessSettings.AutoExposureMinBrightness = 1.0f;     // Min EV100 (默认 1.0)
        // CaptureComponent2D->PostProcessSettings.AutoExposureMaxBrightness = 1.0f;    // Max EV100 (默认 12.0)

        // CaptureComponent2D->PostProcessSettings.bOverride_AutoExposureBias = false;
        // CaptureComponent2D->PostProcessSettings.bOverride_ColorGamma = true;
        // CaptureComponent2D->PostProcessSettings.ColorGamma = FVector4(1.0, 1.0, 1.0, 1.0);
        // // previewComponent->PostProcessSettings.bOverride_AutoExposureBias = false;
        // // previewComponent->PostProcessSettings.bOverride_ColorGamma = true;
        // // previewComponent->PostProcessSettings.ColorGamma = FVector4(1.0, 1.0, 1.0, 1.0);
        // UseUnlitShowFlags(CaptureComponent2D->ShowFlags);

        if (!bIsRGB)
        {
            UseUnlitShowFlags(CaptureComponent2D->ShowFlags);
        }
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("Camera Using Default Post Process Config"));

        CaptureComponent2D->PostProcessSettings.bOverride_MotionBlurAmount = 1;
        CaptureComponent2D->PostProcessSettings.MotionBlurAmount = 0;

        CaptureComponent2D->PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
        CaptureComponent2D->PostProcessSettings.DynamicGlobalIlluminationMethod =
            EDynamicGlobalIlluminationMethod::Lumen;
        CaptureComponent2D->PostProcessSettings.bOverride_ReflectionMethod = true;
        CaptureComponent2D->PostProcessSettings.ReflectionMethod = EReflectionMethod::Lumen;

        if (!bIsRGB)
        {
            UseUnlitShowFlags(CaptureComponent2D->ShowFlags);
        }

        // CaptureComponent2D->PostProcessSettings.bOverride_AutoExposureMethod = true;
        // CaptureComponent2D->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
        // CaptureComponent2D->PostProcessSettings.bOverride_AutoExposureBias = true;
        // CaptureComponent2D->PostProcessSettings.AutoExposureBias = -2;

        // CaptureComponent2D->PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
        // CaptureComponent2D->PostProcessSettings.DynamicGlobalIlluminationMethod =
        //     EDynamicGlobalIlluminationMethod::Lumen;
        // CaptureComponent2D->PostProcessSettings.bOverride_ReflectionMethod = true;
        // CaptureComponent2D->PostProcessSettings.ReflectionMethod = EReflectionMethod::Lumen;

        // CaptureComponent2D->PostProcessSettings.bOverride_LumenSurfaceCacheResolution = true;
        // CaptureComponent2D->PostProcessSettings.LumenSurfaceCacheResolution = 0.001;
    }
    if (PostProcessMaterial)
    {
        CaptureComponent2D->PostProcessSettings.AddBlendable(PostProcessMaterial, 1);
    }

    CaptureComponent2D->CreationMethod = EComponentCreationMethod::Instance;
    CaptureComponent2D->bUseCustomProjectionMatrix = true;
    CaptureComponent2D->CustomProjectionMatrix =
        util::CalcProjectionMatrix(Fx, Fy, Cx, Cy, imageRes.X, imageRes.Y, GNearClippingPlane);
    CaptureComponent2D->RegisterComponent();
    CaptureComponent2D->UpdateContent();
    CaptureComponent2D->Activate();

    // Make sure that there is enough time in the render queue.
    UKismetSystemLibrary::ExecuteConsoleCommand(GetWorld(), FString("g.TimeoutForBlockOnRenderFence 300000"));
    UKismetSystemLibrary::ExecuteConsoleCommand(GetWorld(), FString("r.AntiAliasingMethod 0"));

// UseLitShowFlags(CaptureComponent2D->ShowFlags);
}

void AFisheyeSensor::SetupDistortionMap()
{
    UE_LOG(LogTemp, Display, TEXT("AFisheyeSensor: SetupDistortionMap"));
    int Width = imageRes.X;
    int Height = imageRes.Y;
    UE_LOG(LogTemp, Display, TEXT("width: %d, height: %d"), Width, Height);
    UE_LOG(LogTemp, Display, TEXT("fx: %f, fy: %f, cx: %f, cy: %f, k1: %f, k2: %f, k3: %f, k4: %f"), Fx, Fy, Cx, Cy, K1,
        K2, K3, K4);

    FFisheyeCameraModel FisheyeCameraModel(Width, Height, FVector2d(Fx, Fy), FVector2d(Cx, Cy), K1, K2, K3, K4);

    TArray<FVector> DirectionMapping = FisheyeCameraModel.GenerateDirectionMapping();

    //SaveDirectionMapToTxt(DirectionMapping, Width, Height);

    // camera layout
    //     2
    //   1 0 3
    //     4

    {
        FCameraLayout CameraLayout;
        CameraLayout.Direction = FQuat(FRotator(0, 0, 0));
        CameraLayout.LocalFovStart = FVector2f(-45, -45);
        CameraLayout.LocalFovEnd = FVector2f(45, 45);
        CameraLayout.ProjectionMatrix =
            util::CalcProjectionMatrix(FVector2f(-45, 45), FVector2f(-45, 45), GNearClippingPlane);
        CameraLayout.Intrinsics =
            FVector4d(Width * 0.5, Height * 0.5, Width * 0.5 / FMath::Tan(FMath::DegreesToRadians(45)),
                Height * 0.5 / FMath::Tan(FMath::DegreesToRadians(45)));
        CameraLayouts.Add(CameraLayout);
    }
    // 1
    {
        FCameraLayout CameraLayout;
        CameraLayout.Direction = FQuat(FQuat(FRotator(0, -90, 0)));
        CameraLayout.LocalFovStart = FVector2f(0, -45);
        CameraLayout.LocalFovEnd = FVector2f(45, 45);
        CameraLayout.ProjectionMatrix =
            util::CalcProjectionMatrix(FVector2f(0, 45), FVector2f(-45, 45), GNearClippingPlane);
        CameraLayout.Intrinsics = FVector4d(0, Height * 0.5, Width / FMath::Tan(FMath::DegreesToRadians(45)),
            Height * 0.5 / FMath::Tan(FMath::DegreesToRadians(45)));
        CameraLayouts.Add(CameraLayout);
    }
    // 2
    {
        FCameraLayout CameraLayout;
        CameraLayout.Direction = FQuat(FRotator(90, 0, 0));
        CameraLayout.LocalFovStart = FVector2f(-45, -45);
        CameraLayout.LocalFovEnd = FVector2f(45, 0);
        CameraLayout.ProjectionMatrix =
            util::CalcProjectionMatrix(FVector2f(-45, 45), FVector2f(-45, 0), GNearClippingPlane);
        CameraLayout.Intrinsics = FVector4d(Width * 0.5, 0, Width * 0.5 / FMath::Tan(FMath::DegreesToRadians(45)),
            Height / FMath::Tan(FMath::DegreesToRadians(45)));
        CameraLayouts.Add(CameraLayout);
    }
    // 3
    {
        FCameraLayout CameraLayout;
        CameraLayout.Direction = FQuat(FRotator(0, 90, 0));
        CameraLayout.LocalFovStart = FVector2f(-45, -45);
        CameraLayout.LocalFovEnd = FVector2f(0, 45);
        CameraLayout.ProjectionMatrix =
            util::CalcProjectionMatrix(FVector2f(-45, 0), FVector2f(-45, 45), GNearClippingPlane);
        CameraLayout.Intrinsics = FVector4d(Width, Height * 0.5, Width / FMath::Tan(FMath::DegreesToRadians(45)),
            Height * 0.5 / FMath::Tan(FMath::DegreesToRadians(45)));
        CameraLayouts.Add(CameraLayout);
    }
    // 4
    {
        FCameraLayout CameraLayout;
        CameraLayout.Direction = FQuat(FRotator(-90, 0, 0));
        CameraLayout.LocalFovStart = FVector2f(-45, 0);
        CameraLayout.LocalFovEnd = FVector2f(45, 45);
        CameraLayout.ProjectionMatrix =
            util::CalcProjectionMatrix(FVector2f(-45, 45), FVector2f(0, 45), GNearClippingPlane);
        CameraLayout.Intrinsics = FVector4d(Width * 0.5, Height, Width * 0.5 / FMath::Tan(FMath::DegreesToRadians(45)),
            Height / FMath::Tan(FMath::DegreesToRadians(45)));
        CameraLayouts.Add(CameraLayout);
    }

    // // 0
    // {
    //     FCameraLayout CameraLayout;
    //     CameraLayout.Direction = FQuat(FRotator(0, 0, 0));
    //     CameraLayout.LocalFovStart = FVector2f(-45, -45);
    //     CameraLayout.LocalFovEnd = FVector2f(45, 45);
    //     CameraLayout.ProjectionMatrix =
    //         util::CalcProjectionMatrix(FVector2f(-45, 45), FVector2f(-45, 45), GNearClippingPlane);
    //     CameraLayout.Intrinsics =
    //         FVector4d(Width * 0.5, Height * 0.5, Width * 0.5 / FMath::Tan(FMath::DegreesToRadians(45)),
    //             Height * 0.5 / FMath::Tan(FMath::DegreesToRadians(45)));
    //     CameraLayouts.Add(CameraLayout);
    // }
    // // 1
    // {
    //     FCameraLayout CameraLayout;
    //     CameraLayout.Direction = FQuat(FQuat(FRotator(0, -90, 0)));
    //     CameraLayout.LocalFovStart = FVector2f(-45, -45);
    //     CameraLayout.LocalFovEnd = FVector2f(45, 45);
    //     CameraLayout.ProjectionMatrix =
    //         util::CalcProjectionMatrix(FVector2f(-45, 45), FVector2f(-45, 45), GNearClippingPlane);
    //     CameraLayout.Intrinsics =
    //         FVector4d(Width * 0.5, Height * 0.5, Width * 0.5 / FMath::Tan(FMath::DegreesToRadians(45)),
    //             Height * 0.5 / FMath::Tan(FMath::DegreesToRadians(45)));
    //     CameraLayouts.Add(CameraLayout);
    // }
    // // 2
    // {
    //     FCameraLayout CameraLayout;
    //     CameraLayout.Direction = FQuat(FRotator(90, 0, 0));
    //     CameraLayout.LocalFovStart = FVector2f(-45, -45);
    //     CameraLayout.LocalFovEnd = FVector2f(45, 45);
    //     CameraLayout.ProjectionMatrix =
    //         util::CalcProjectionMatrix(FVector2f(-45, 45), FVector2f(-45, 45), GNearClippingPlane);
    //     CameraLayout.Intrinsics =
    //         FVector4d(Width * 0.5, Height * 0.5, Width * 0.5 / FMath::Tan(FMath::DegreesToRadians(45)),
    //             Height * 0.5 / FMath::Tan(FMath::DegreesToRadians(45)));
    //     CameraLayouts.Add(CameraLayout);
    // }
    // // 3
    // {
    //     FCameraLayout CameraLayout;
    //     CameraLayout.Direction = FQuat(FRotator(0, 90, 0));
    //     CameraLayout.LocalFovStart = FVector2f(-45, -45);
    //     CameraLayout.LocalFovEnd = FVector2f(45, 45);
    //     CameraLayout.ProjectionMatrix =
    //         util::CalcProjectionMatrix(FVector2f(-45, 45), FVector2f(-45, 45), GNearClippingPlane);
    //     CameraLayout.Intrinsics =
    //         FVector4d(Width * 0.5, Height * 0.5, Width * 0.5 / FMath::Tan(FMath::DegreesToRadians(45)),
    //             Height * 0.5 / FMath::Tan(FMath::DegreesToRadians(45)));
    //     CameraLayouts.Add(CameraLayout);
    // }
    // // 4
    // {
    //     FCameraLayout CameraLayout;
    //     CameraLayout.Direction = FQuat(FRotator(-90, 0, 0));
    //     CameraLayout.LocalFovStart = FVector2f(-45, -45);
    //     CameraLayout.LocalFovEnd = FVector2f(45, 45);
    //     CameraLayout.ProjectionMatrix =
    //         util::CalcProjectionMatrix(FVector2f(-45, 45), FVector2f(-45, 45), GNearClippingPlane);
    //     CameraLayout.Intrinsics =
    //         FVector4d(Width * 0.5, Height * 0.5, Width * 0.5 / FMath::Tan(FMath::DegreesToRadians(45)),
    //             Height * 0.5 / FMath::Tan(FMath::DegreesToRadians(45)));
    //     CameraLayouts.Add(CameraLayout);
    // }

    UVMapping.SetNumZeroed(Width * Height);
    UVCameraMapping.SetNum(Width * Height);
    for (int j = 0; j < Height; j++)
    {
        for (int i = 0; i < Width; i++)
        {
            const int Index = i + j * Width;

            for (int CameraIndex = 0; CameraIndex < CameraLayouts.Num(); CameraIndex++)
            {
                const FCameraLayout& CameraLayout = CameraLayouts[CameraIndex];
                FVector DirectionRay = DirectionMapping[Index];
                FVector LocalDirectionRay = CameraLayout.Direction.Inverse() * DirectionRay;
                double Cx = CameraLayout.Intrinsics.X;
                double Cy = CameraLayout.Intrinsics.Y;
                double Fx = CameraLayout.Intrinsics.Z;
                double Fy = CameraLayout.Intrinsics.W;
                FVector2f UV = FVector2f(Cx, Cy) + FVector2f(LocalDirectionRay.Y, -LocalDirectionRay.Z) /
                                                       LocalDirectionRay.X * FVector2f(Fx, Fy);

                // UV = FVector2f(FMath::FloorToFloat(UV.X), FMath::FloorToFloat(UV.Y));

                // if(i == 0 ){
                //     // UE_LOG(LogTemp, Display, TEXT("CameraIndex: %d"), CameraIndex);
                //     UE_LOG(LogTemp, Display, TEXT("UV: %f, %f"), UV.X, UV.Y);
                // }

                // float DotResult = FVector::DotProduct(DirectionRay, CameraLayout.Direction.GetForwardVector());

                // UE_LOG(LogTemp, Display, TEXT("dot: %f"), dot);

                // float FisheyeGain = 1.0f;

                // LocalDirectionRay = LocalDirectionRay.GetSafeNormal();

                // // 计算夹角θ（弧度）
                // float Theta = FMath::Acos(LocalDirectionRay.X);    // X是光轴（Forward）

                // // 等距投影模型：r = f * θ
                // float r = Fx * Theta;

                // // 计算成像平面上的方向（Y-Z平面投影）
                // // 注意：FRU坐标系下，Y是Right，Z是Up，因此：
                // // - 水平方向（U）对应Y（Right）
                // // - 垂直方向（V）对应Z（Up）
                // FVector2f NormalizedDirection(LocalDirectionRay.Y, LocalDirectionRay.Z);    // 注意Y是Right，Z是Up
                // NormalizedDirection.Normalize();                                            // 归一化到单位圆

                // // 最终UV坐标（考虑主点偏移）
                // FVector2f UV = FVector2f(Cx, Cy) + NormalizedDirection * r;

                UV /= FVector2f(Width, Height);
                // bool next = DotResult < 0.0;

                if (UV.X >= 0 && UV.X <= 1 && UV.Y >= 0 && UV.Y <= 1 && DirectionRay.GetUnsafeNormal().X > 1e-8)
                {
                    // if (next)
                    // {
                    //     continue;
                    // }
                    // static int count = 0;
                    // // UE_LOG(LogTemp, Display, TEXT("CameraIndex: %d"), CameraIndex);
                    // if (CameraIndex == 3 || CameraIndex == 4)
                    // {
                    //     count++;
                    //     if (count < 100)
                    //     {
                    //         UE_LOG(LogTemp, Display, TEXT("CameraIndex: %d"), CameraIndex);
                    //     }
                    //     // UE_LOG(LogTemp, Display, TEXT("UV: %f, %f"), UV.X, UV.Y);
                    //     // UV = FVector2f(UV.X, UV.Y);
                    // }
                    UVMapping[Index] = UV;
                    UVCameraMapping[Index] = CameraIndex;
                    break;
                }
                else
                {
                    UVCameraMapping[Index] = -1;
                }

                // if (UV.X >= 0 && UV.X <= 1 && UV.Y >= 0 && UV.Y <= 1 && DirectionRay.GetUnsafeNormal().X > 1e-8)
                // {
                //     UVMapping[Index] = UV;
                //     UVCameraMapping[Index] = CameraIndex;
                //     break;
                // }
                // else
                // {
                //     UVCameraMapping[Index] = -1;
                // }
            }
        }
    }
}

void AFisheyeSensor::CreateOtherCaptureComponents()
{
    CaptureComponent2D->CustomProjectionMatrix = CameraLayouts[0].ProjectionMatrix;
    CaptureComponent2D->SetRelativeRotation(CameraLayouts[0].Direction);

    CaptureComponents.Add(CaptureComponent2D);
    for (int i = 1; i < 5; i++)
    {
        FCameraLayout CameraLayout = CameraLayouts[i];
        auto SceneCaptureComponent2D =
            NewObject<USceneCaptureComponent2D>(this, FName(*FString::Printf(TEXT("SceneCaptureComponent2D_%d"), i)));
        SceneCaptureComponent2D->SetMobility(EComponentMobility::Movable);
        SceneCaptureComponent2D->PrimitiveRenderMode = CaptureComponent2D->PrimitiveRenderMode;
        SceneCaptureComponent2D->bCaptureOnMovement = CaptureComponent2D->bCaptureOnMovement;
        SceneCaptureComponent2D->bCaptureEveryFrame = CaptureComponent2D->bCaptureEveryFrame;
        SceneCaptureComponent2D->bAlwaysPersistRenderingState = CaptureComponent2D->bAlwaysPersistRenderingState;

        SceneCaptureComponent2D->bUseCustomProjectionMatrix = true;
        SceneCaptureComponent2D->CustomProjectionMatrix = CameraLayout.ProjectionMatrix;
        SceneCaptureComponent2D->SetRelativeRotation(CameraLayout.Direction);
        SceneCaptureComponent2D->SetRelativeLocation(FVector(0, 0, 0));
        SceneCaptureComponent2D->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
        SceneCaptureComponent2D->CreationMethod = CaptureComponent2D->CreationMethod;
        SceneCaptureComponent2D->RegisterComponent();

        SceneCaptureComponent2D->CaptureSource = CaptureComponent2D->CaptureSource;
        SceneCaptureComponent2D->bUseRayTracingIfEnabled = CaptureComponent2D->bUseRayTracingIfEnabled;
        SceneCaptureComponent2D->SceneViewExtensions.Add(CaptureComponent2D->SceneViewExtensions[0]);
        SceneCaptureComponent2D->TextureTarget = CaptureComponent2D->TextureTarget;
        SceneCaptureComponent2D->ShowFlags = CaptureComponent2D->ShowFlags;
        SceneCaptureComponent2D->PostProcessSettings = CaptureComponent2D->PostProcessSettings;

        if (PostProcessMaterial)
        {
            SceneCaptureComponent2D->PostProcessSettings.AddBlendable(PostProcessMaterial, 1);
        }

        CaptureComponents.Add(SceneCaptureComponent2D);
    }
}