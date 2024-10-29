#include "SensorManager.h"
#include "Kismet/KismetMathLibrary.h"
#include "Objects/Sensors/SensorFactory.h"
#include "Objects/Sensors/CameraSensors/CameraSensor.h"

DEFINE_LOG_CATEGORY_STATIC(SimLogSensorManager, Log, All);

void GetPropValue(const TMap<FString, FString>& params, const FString& para, double& value)
{
    if (params.Find(para))
    {
        value = FCString::Atod(**params.Find(para));
    }
};

void GetPropValue(const TMap<FString, FString>& params, const FString& para, float& value)
{
    if (params.Find(para))
    {
        value = FCString::Atof(**params.Find(para));
    }
};

void GetPropValue(const TMap<FString, FString>& params, const FString& para, int& value)
{
    if (params.Find(para))
    {
        value = FCString::Atoi(**params.Find(para));
    }
};
void GetPropValue(const TMap<FString, FString>& params, const FString& para, FString& value)
{
    if (params.Find(para))
    {
        value = *params.Find(para);
    }
};
void GetPropValue(const TMap<FString, FString>& params, const FString& para, bool& value)
{
    if (params.Find(para))
    {
        value = true;
        auto v = params.Find(para)->ToLower();
        if (v == TEXT("false") || v == TEXT("0") || v == TEXT("close") || v == TEXT("disable"))
            value = false;
    }
};

ASensorManager::ASensorManager()
{
}

void ASensorManager::Init(const FManagerConfig& Config)
{
    const FSensorManagerConfig* SensorConfig = Cast_Data<const FSensorManagerConfig>(Config);
    if (!SensorConfig)
    {
        UE_LOG(SimLogSensorManager, Error, TEXT("Can Not Get SensorConfig!"));
        return;
    }
    // Camera
    for (auto& Elem : SensorConfig->cameraArry)
    {
        ACameraSensor* CameraSensor =
            ASensorFactory::SpawnSensor<ACameraSensor>(GetWorld(), ACameraSensor::StaticClass(), Elem);
        if (CameraSensor)
        {

        }   
    }   
}

FSensorManagerConfig ASensorManager::ParseSensorString(const std::string& buffer, int64 EgoId)
{
    UE_LOG(SimLogSensorManager, Log, TEXT("Parse config from pb"));
    FSensorManagerConfig SensormanagerConfig;
    if (buffer.empty())
    {
        UE_LOG(SimLogSensorManager, Log, TEXT("pb is empty"));
        return SensormanagerConfig;
    }
    sim_msg::Scene scene;
    if (!scene.ParseFromString(buffer))
    {
        UE_LOG(SimLogSensorManager, Warning, TEXT("ParseFromString faild."));
        return SensormanagerConfig;
    }

    FString BaseSavePath;
    if (GConfig->GetString(TEXT("Sensor"), TEXT("SavePath"), BaseSavePath, GGameIni))
    {
        bool absPath = false;
#ifdef WIN32
        absPath = BaseSavePath.Find(TEXT(":")) > INDEX_NONE;
#else
        absPath = BaseSavePath.Len() > 0 && BaseSavePath[0] == TEXT('/');
#endif    // WIN32


        if (!absPath)
            BaseSavePath = FPaths::ProjectSavedDir() + BaseSavePath;
        BaseSavePath += TEXT("Ego_") + FString::FromInt(EgoId + 1) + TEXT("/");
        FString FileName = TEXT("SensorData");
        FileName += TEXT("_");
        FString Date;
        FDateTime datetime;
        UObject* parent = nullptr;
        if (UKismetMathLibrary::Now().ExportTextItem(Date, datetime, parent, 1, parent))
        {
            FileName += Date;
            FileName += TEXT("/");
        }
        BaseSavePath += FileName;
        UE_LOG(LogTemp, Log, TEXT("SensorManger: savePath is: %s."), *BaseSavePath);
    }

    FString device = TEXT("0");
    {
        if (!FParse::Value(FCommandLine::Get(), TEXT("-device="), device))
        {
            device = TEXT("0");
        }
        int idx = 0;
        if (device.FindChar('.', idx))
        {
            device = device.Mid(idx + 1);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("SensorManger: device is: %s."), *device);

    int32 FindEgoIndex = EgoId;
    if (FindEgoIndex < 0)
    {
        UE_LOG(SimLogSensorManager, Error, TEXT("Find ego index failed"));
        return SensormanagerConfig;
    }
    else
    {
        UE_LOG(SimLogSensorManager, Log, TEXT("Find ego index %d, Id %ld"), FindEgoIndex, EgoId);
    }

    UE_LOG(
        LogTemp, Log, TEXT("SensorManger: sensors count: %d."), scene.egos(FindEgoIndex).sensor_group().sensors_size());

    for (const auto& sensor : scene.egos(FindEgoIndex).sensor_group().sensors())
    {
        FSensorConfig Base;
        Base.device = ANSI_TO_TCHAR(sensor.extrinsic().device().c_str());
        {
            int idx = 0;
            if (Base.device.FindChar('.', idx))
            {
                Base.device = Base.device.Mid(idx + 1);
            }
        }
        if (Base.device != device)
        {
            UE_LOG(LogTemp, Log, TEXT("SensorManger: config skip: deivce=%s, id=%d"), *Base.device, Base.id);
            continue;
        }
        Base.id = sensor.extrinsic().id();
        Base.installSlot = ANSI_TO_TCHAR(sensor.extrinsic().installslot().c_str());
        Base.installLocation.X = sensor.extrinsic().locationx();
        Base.installLocation.Y = sensor.extrinsic().locationy();
        Base.installLocation.Z = sensor.extrinsic().locationz();
        Base.installRotation.Roll = sensor.extrinsic().rotationx();
        Base.installRotation.Pitch = sensor.extrinsic().rotationy();
        Base.installRotation.Yaw = sensor.extrinsic().rotationz();
        UE_LOG(LogTemp, Log, TEXT("SensorManger: sensor[%d] device is %s"), Base.id, *Base.device);
        UE_LOG(LogTemp, Log, TEXT("SensorManger: sensor[%d] install pos is %s, %s"), Base.id,
            *Base.installLocation.ToString(), *Base.installRotation.ToString())

        // Coordinate transform
        CoordinateTransform_RightHandToLeftHand(Base.installLocation, Base.installRotation);

        Base.targetType = ETrafficType::ST_Ego;
        Base.targetId = 0;

        TMap<FString, FString> Config;
        for (const auto& ss : sensor.intrinsic().params())
        {
            UE_LOG(LogTemp, Log, TEXT("SensorManger: %s: %s"), ANSI_TO_TCHAR(ss.first.c_str()),
                ANSI_TO_TCHAR(ss.second.c_str()));
            Config.Add(ANSI_TO_TCHAR(ss.first.c_str())) = ANSI_TO_TCHAR(ss.second.c_str());
        }

        if (sensor.type() == sim_msg::SENSOR_TYPE_CAMERA)
        {
            FCameraConfig NewConfig;
            *(FSensorConfig*) &NewConfig = Base;
            NewConfig.typeName = TEXT("Camera");
            NewConfig.targetId = EgoId;
            FString Intrinsic_Matrix = TEXT("");
            FString distortion_Parameters = TEXT("");

            // old
            GetPropValue(Config, FString(TEXT("CCD_Width")), NewConfig.ccd_Width);              ////---
            GetPropValue(Config, FString(TEXT("CCD_Height")), NewConfig.ccd_Height);            ////---
            GetPropValue(Config, FString(TEXT("Focal_Length")), NewConfig.focal_Length);        ////---
            GetPropValue(Config, FString(TEXT("FOV_Horizontal")), NewConfig.fov_Horizontal);    ////---
            GetPropValue(Config, FString(TEXT("FOV_Vertical")), NewConfig.fov_Vertical);        ////---
            GetPropValue(Config, FString(TEXT("Res_Horizontal")), NewConfig.res_Horizontal);    ////---
            GetPropValue(Config, FString(TEXT("Res_Vertical")), NewConfig.res_Vertical);        ////---
            GetPropValue(Config, FString(TEXT("IntrinsicParamType")), NewConfig.paraType);
            GetPropValue(Config, FString(TEXT("Intrinsic_Matrix")), Intrinsic_Matrix);              ////---
            GetPropValue(Config, FString(TEXT("Distortion_Parameters")), distortion_Parameters);    /////----
            GetPropValue(Config, FString(TEXT("Blur_Intensity")), NewConfig.blur_Intensity);
            GetPropValue(Config, FString(TEXT("MotionBlur_Amount")), NewConfig.motionBlur_Amount);
            GetPropValue(Config, FString(TEXT("Noise_Intensity")), NewConfig.noise_Intensity);
            GetPropValue(Config, FString(TEXT("Vignette_Intensity")), NewConfig.vignette_Intensity);
            GetPropValue(Config, FString(TEXT("DisplayMode")), NewConfig.color_gray);
            FString dm;
            GetPropValue(Config, FString(TEXT("DisplayMode")), dm);
            if (dm == TEXT("Gray"))
            {
                NewConfig.color_gray = 1;
            }

            GetPropValue(Config, FString(TEXT("Frequency")), NewConfig.frequency);
            GetPropValue(Config, FString(TEXT("CcdWidth")), NewConfig.ccd_Width);
            GetPropValue(Config, FString(TEXT("CcdHeight")), NewConfig.ccd_Height);
            GetPropValue(Config, FString(TEXT("CcdFocal")), NewConfig.focal_Length);
            GetPropValue(Config, FString(TEXT("FovHorizonal")), NewConfig.fov_Horizontal);
            GetPropValue(Config, FString(TEXT("FovVertial")), NewConfig.fov_Vertical);
            GetPropValue(Config, FString(TEXT("ResHorizonal")), NewConfig.res_Horizontal);
            GetPropValue(Config, FString(TEXT("ResVertial")), NewConfig.res_Vertical);
            GetPropValue(Config, FString(TEXT("Vignette")), NewConfig.vignette_Intensity);
            GetPropValue(Config, FString(TEXT("GrainIntensity")), NewConfig.noise_Intensity);
            GetPropValue(Config, FString(TEXT("MotionBlur")), NewConfig.motionBlur_Amount);
            GetPropValue(Config, FString(TEXT("LensFlares")), NewConfig.LensFlares);
            GetPropValue(Config, FString(TEXT("Blur")), NewConfig.blur_Intensity);
            GetPropValue(Config, FString(TEXT("Exquisite")), NewConfig.Exquisite);
            GetPropValue(Config, FString(TEXT("Bloom")), NewConfig.Bloom);
            GetPropValue(Config, FString(TEXT("Compensation")), NewConfig.Compensation);
            GetPropValue(Config, FString(TEXT("ShutterSpeed")), NewConfig.ShutterSpeed);
            GetPropValue(Config, FString(TEXT("ISO")), NewConfig.ISO);
            GetPropValue(Config, FString(TEXT("Aperture")), NewConfig.Aperture);
            GetPropValue(Config, FString(TEXT("ColorTemperature")), NewConfig.ColorTemperature);
            GetPropValue(Config, FString(TEXT("WhiteHint")), NewConfig.WhiteHint);
            GetPropValue(Config, FString(TEXT("Transmittance")), NewConfig.Transmittance);
            GetPropValue(Config, FString(TEXT("ExposureMode")), NewConfig.Exposure);
            GetPropValue(Config, FString(TEXT("ColorMode")), NewConfig.color_gray);
            GetPropValue(Config, FString(TEXT("IntrinsicType")), NewConfig.paraType);
            GetPropValue(Config, FString(TEXT("IntrinsicMat")), Intrinsic_Matrix);
            GetPropValue(Config, FString(TEXT("Distortion")), distortion_Parameters);

            if (!Intrinsic_Matrix.IsEmpty())
            {
                Intrinsic_Matrix = Intrinsic_Matrix.Replace(*FString(" "), *FString(""));    // Remove space
                FString LeftStr;
                FString RightStr;
                while (Intrinsic_Matrix.Split(",", &LeftStr, &RightStr))
                {
                    NewConfig.intrinsic_Matrix.Add(FCString::Atof(*LeftStr));
                    Intrinsic_Matrix = RightStr;
                }
                NewConfig.intrinsic_Matrix.Add(FCString::Atof(*Intrinsic_Matrix));
            }
            if (!distortion_Parameters.IsEmpty())
            {
                distortion_Parameters = distortion_Parameters.Replace(*FString(" "), *FString(""));    // Remove space
                FString LeftStr;
                FString RightStr;
                while (distortion_Parameters.Split(",", &LeftStr, &RightStr))
                {
                    NewConfig.distortion_Parameters.Add(FCString::Atof(*LeftStr));
                    distortion_Parameters = RightStr;
                }
                NewConfig.distortion_Parameters.Add(FCString::Atof(*distortion_Parameters));
            }
            FString savestring;
            GConfig->GetString(TEXT("Sensor"), TEXT("CameraSaved"), savestring, GGameIni);
            if (!BaseSavePath.IsEmpty() && savestring == TEXT("true"))
                NewConfig.savePath = BaseSavePath + TEXT("CameraData/") + TEXT("Camera_") +
                                     FString::FromInt(NewConfig.id) +
                                     TEXT("/");    // TODO: set save path in a standardized way
            SensormanagerConfig.cameraArry.Add(NewConfig);
        }
    }

    return SensormanagerConfig;

}

void ASensorManager::CoordinateTransform_RightHandToLeftHand(FVector& _Location, FRotator& _Rotation)
{
    _Location.Y = _Location.Y * (-1.f);
    _Rotation.Pitch = _Rotation.Pitch * (-1.f);
    _Rotation.Yaw = _Rotation.Yaw * (-1.f);
}
