// Fill out your copyright notice in the Description page of Project Settings.

#include "TLidarSensor.h"

#include "DepthCamera.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/DisplayGameInstance.h"
#include "Framework/DisplayGameStateBase.h"
#include "Framework/DisplayPlayerController.h"
#include "Framework/SaveDataThread.h"
#include "HadMap/Public/HadmapManager.h"
#include "Kismet/KismetMathLibrary.h"
#include "SimMsg/sensor_raw.pb.h"
#include "TLidarBufferDepth.h"
#include "TLidarBufferDepthOld.h"
#include "TLidarBufferRaycast.h"
#include "lidar/HSLidar.h"
#include "lidar/LidarModel.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

// std::chrono::steady_clock::time_point ttttt0;// = std::chrono::steady_clock::now();

ATLidarSensor::ATLidarSensor(const FObjectInitializer& ObjectInitializer)
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if
    // you don't need it.
    PrimaryActorTick.bCanEverTick = false;
    root = CreateDefaultSubobject<USceneComponent>(FName(TEXT("root")));
    RootComponent = root;
}

void ATLidarSensor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (senddataThreadHandle)
    {
        senddataThreadHandle->EnsureCompletion();
    }
    senddataThreadHandle.Reset();
    Super::EndPlay(EndPlayReason);
}

void ATLidarSensor::Destroy(FString _Reason)
{
    if (senddataThreadHandle)
    {
        senddataThreadHandle->EnsureCompletion();
    }
    senddataThreadHandle.Reset();
    Super::Destroy(_Reason);
}

// 加载内参文件的路径
FString ATLidarSensor::LoadAngleDefinition(const FString& fpath)
{
    FString angle_string;
    if (fpath.Len() == 0)
        return angle_string;
    // 如果本地没有，则执行脚本，从云端下载
    FString srcpath = fpath;
    if (!FPaths::FileExists(srcpath))
    {
        srcpath = FPaths::ProjectLogDir() + TEXT("angle.tmp.") + FString::FromInt(config.id);
        srcpath = FPaths::ConvertRelativePathToFull(srcpath);

        FString exepath = TEXT("");
        FString param = TEXT("");
        FString batpath = FPaths::ProjectSavedDir() + TEXT("Script/Download");
#if PLATFORM_WINDOWS
        exepath = TEXT("cmd.exe");
        param = TEXT("/C ");
        batpath += TEXT(".bat");
#else
        exepath = TEXT("/bin/bash");
        batpath += TEXT(".sh");
#endif
        batpath = FPaths::ConvertRelativePathToFull(batpath);
        if (FPaths::FileExists(batpath))
        {
            FString commandLine = param + batpath + TEXT(" ") + fpath + TEXT(" ") + srcpath;
            UE_LOG(LogTemp, Log, TEXT("Run script: %s %s"), *exepath, *commandLine);
            auto proc =
                FPlatformProcess::CreateProc(*exepath, *commandLine, false, true, true, nullptr, 0, nullptr, nullptr);
            FPlatformProcess::WaitForProc(proc);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Cannot find %s"), *batpath);
        }
    }
    if (FPaths::FileExists(srcpath))
    {
        FFileHelper::LoadFileToString(angle_string, *srcpath);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("angle file %s not exists: "), *srcpath);
    }
    UE_LOG(LogTemp, Log, TEXT("read angle file size is %d"), angle_string.Len());
    return angle_string;
}

// 创建激光雷达
bool ATLidarSensor::CreateLasers()
{
    bool dualRet = false;
    GConfig->GetBool(TEXT("Sensor"), TEXT("LidarDualReturn"), dualRet, GGameIni);
    FString cfgdir = config.cfgDir + "/";
    FString AbsolutePath = FPaths::ConvertRelativePathToFull(cfgdir);
    UE_LOG(LogTemp, Log, TEXT("config dir = %s"), *AbsolutePath);
    // 自定义型号
    if (config.model == "User")
    {
    }
    // 速腾聚创型号
    else if (config.model.StartsWith("RS"))
    {
    }
    // 禾赛型号
    else if (config.model.StartsWith("HS"))
    {
        // 各个型号的衰减
        cfgdir += config.model;
        if (config.model == "HS32")
        {
        }
        else if (config.model == "HS128")
        {
            LidarSensor = std::make_shared<hslidar::HSLidar128>();
            lidarMd.set_intensity(0.0009f);    // 200m (on 10% reflectivity target
        }
        else if (config.model == "HS128AT")
        {
            LidarSensor = std::make_shared<hslidar::HSLidar128AT>();
            lidarMd.set_intensity(0.001f);    // 180m (on 10% reflectivity target
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Create Lidar faild:%s"), *config.model);
            return false;
        }
        static_cast<hslidar::HSLidar*>(LidarSensor.get())->SetID(config.id);
        static_cast<hslidar::HSLidar*>(LidarSensor.get())->setIP(config.ip, config.port);
        static_cast<hslidar::HSLidar*>(LidarSensor.get())->setRotationFrequency(config.frequency);
        if (!static_cast<hslidar::HSLidar*>(LidarSensor.get())
                 ->setAngleFromString(LoadAngleDefinition(config.AngleDefinition)))
        {
            if (!static_cast<hslidar::HSLidar*>(LidarSensor.get())->loadInterReference(TCHAR_TO_ANSI(*(cfgdir))))
            {
                UE_LOG(LogTemp, Warning, TEXT("Create Lidar faild:%s"),
                    ANSI_TO_TCHAR(static_cast<hslidar::HSLidar*>(LidarSensor.get())->error().c_str()));
                return false;
            }
        }
        if (dualRet)
        {
            UE_LOG(LogTemp, Log, TEXT("Set Lidar Dual Return"));
            LidarSensor->setReturnMode(lidar::RT_Dual);
        }
        if (!static_cast<hslidar::HSLidar*>(LidarSensor.get())->Init())
        {
            UE_LOG(LogTemp, Warning, TEXT("Create Lidar faild:%s"),
                ANSI_TO_TCHAR(static_cast<hslidar::HSLidar*>(LidarSensor.get())->error().c_str()));
            return false;
        }
    }
    else if (config.model.StartsWith("Velodyne"))
    {
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Create Lidar faild:%s"), *config.model);
        return false;
    }
    lidarMd.set_extinctionCoe(config.ExtinctionCoe);
    lidarMd.set_extinctionCoe(config.ExtinctionCoe);
    if (config.Attenuation > 0)
    {
        lidarMd.set_intensity(config.Attenuation);
    }
    if (!lidarMd.load_refdata(TCHAR_TO_ANSI(*(config.cfgDir + "/reflection.dat"))))
    {
        UE_LOG(LogTemp, Log, TEXT("Cannot load reflection.dat"));
    }

    // ASensorManager* SM = Cast<ASensorManager>(this->GetOwner());
    // if (SM && SM->GetVehicle())
    //{
    //     AEgoVehicle* Ego = Cast<AEgoVehicle>(SM->GetVehicle());
    //     if (Ego)
    //     {
    //         EnvmM = Ego->GetEnvironmentManager();
    //     }
    // }

    LidarMeasurement.TimeStamp0 = -1;

    if (LidarSensor)
    {
        // UE_LOG(LogTemp, Log, TEXT("Lidar info: type is %d"), LidarSensor->getType());
        UE_LOG(LogTemp, Log, TEXT("Lidar info: ray num is %d"), LidarSensor->getRaysNum());
        UE_LOG(
            LogTemp, Log, TEXT("Lidar info: horizontal scan min unit is %d"), LidarSensor->getHorizontalScanMinUnit());
        UE_LOG(LogTemp, Log, TEXT("Lidar info: rotation frequency is %f"), LidarSensor->getRotationFrequency());
        UE_LOG(LogTemp, Log, TEXT("Lidar info: horizontal scan count is %d"), LidarSensor->getHorizontalScanCount());
    }

    return LidarSensor.get() != nullptr;
}

bool ATLidarSensor::Init(const FSensorConfig& _Config)
{
    UE_LOG(LogTemp, Log, TEXT("ATLidarSensor Init"));

    if (!ASensorActor::Init(_Config))
        return false;
    const FLidarConfig* NewLidarSensorConfig = Cast_Sim<const FLidarConfig>(_Config);
    // FLidarConfig* NewLidarSensorConfig = CastStruct<FTLidarSensorConfigStruct>(_NewConfig);
    // this->SetActorRelativeLocation(NewLidarSensorConfig->location);
    // this->SetActorRelativeRotation(NewLidarSensorConfig->rotation.Quaternion());

    config = *NewLidarSensorConfig;
    if (!CreateLasers())
        return false;
    if (!FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*config.savePath))
    {
        // UE_LOG(LogTemp, Log, TEXT("CameraSensorComponent: Generate savePath."));
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*config.savePath);
    }

    // 当前深度获取方法
    FString method;
    if (GConfig->GetString(TEXT("Sensor"), TEXT("LidarMethod"), method, GGameIni))
    {
        method = method.ToLower();
        // 射线法
        if (method == TEXT("raycast"))
        {
            UE_LOG(LogTemp, Log, TEXT("Lidar method is Raycast"));
            lidarBuffer = std::make_shared<ALidarBufferRaycast>();
            static_cast<ALidarBufferRaycast*>(lidarBuffer.get())->SetShowPoint(config.bDrawPoint);
        }
        // 深度渲染方法
        else if (method == TEXT("depth"))
        {
            UE_LOG(LogTemp, Log, TEXT("Lidar method is Depth"));
            lidarBuffer = std::make_shared<ALidarBufferDepth>();
        }
        else if (method == TEXT("depth_old"))
        {
            UE_LOG(LogTemp, Log, TEXT("Lidar method is Depth Old"));
            lidarBuffer = std::make_shared<ALidarBufferDepthOld>();
        }
#if PLATFORM_WINDOWS
        // 光追方法
        else if (method == TEXT("raytracing"))
        {
            auto CVaRaytring = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing"));
            bool bIsRaytringEnabled = (CVaRaytring && CVaRaytring->GetInt() != 0);
            if (bIsRaytringEnabled)
            {
                UE_LOG(LogTemp, Log, TEXT("Lidar method is raytracing"));
                lidarBuffer = std::make_shared<ALidarBufferRayTracing>();
            }
        }
#endif
    }
    if (!lidarBuffer)
    {
        UE_LOG(LogTemp, Log, TEXT("Lidar method is Depth"));
        lidarBuffer = std::make_shared<ALidarBufferDepth>();
    }

    GConfig->GetBool(TEXT("Sensor"), TEXT("PublicMsg"), public_msg, GGameIni);
    config.hil = (GetDisplayInstance() && GetDisplayInstance()->nHILpos.X > 0) || !public_msg;

    lidarBuffer->Init(config, LidarSensor, this, &lidarMd);
    if (!config.savePath.IsEmpty() && !FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*config.savePath))
    {
        // UE_LOG(LogTemp, Log, TEXT("CameraSensorComponent: Generate savePath."));
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*config.savePath);
    }

    // 创建发送数据的线程，用于点云生成、发送UDP、单帧
    senddataThreadHandle = MakeShared<SendDataThread>(this);

    FString mattype;
    if (GConfig->GetString(TEXT("Sensor"), TEXT("LidarCoordType"), mattype, GGameIni))
    {
        coordType = mattype.ToLower();
        if (coordType == TEXT("vehicle"))
        {
            FTransform tf;
            tf.SetLocation(config.installLocation * 0.01f);
            tf.SetRotation(config.installRotation.Quaternion());
            lidarBuffer->setRotationTranslation(tf);

            UE_LOG(LogTemp, Log, TEXT("Lidar coord is vehicle"));
        }
        else if (coordType == TEXT("enu"))
        {
            UE_LOG(LogTemp, Log, TEXT("Lidar coord is enu"));
        }
    }
    GConfig->GetBool(TEXT("Sensor"), TEXT("LidarFrameAlign"), lidarFrameAlign, GGameIni);

    return true;
}

void ATLidarSensor::Update(const FSensorInput& _Input, FSensorOutput& _Output)
{
    // ttttt0 = std::chrono::steady_clock::now();
    if (!LidarSensor.get())
    {
        return;
    }
    if (!senddataThreadHandle)
    {
        return;
    }

    const FLidarInput* Input = Cast_Sim<const FLidarInput>(_Input);
    // 频率控制
    if (lidarFrameAlign && config.frequency > 0 && (Input->timeStamp - timeStamp_last) < 999.9999999 / config.frequency)
    {
        return;
    }
    if (lidarFrameAlign)
    {
        UE_LOG(LogTemp, Log, TEXT("LidarFrameAlign: %f %f"), Input->timeStamp, config.frequency);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("Not LidarFrameAlign %f %f"), Input->timeStamp, config.frequency);
    }
    timeStamp_last = Input->timeStamp;

    double timeStamp = Input->timeStamp;
    if (GetDisplayInstance() && GetDisplayInstance()->nHILpos.X < 0)
    {
        timeStamp = Input->timeStamp_ego;
        if (config.installSlot == TEXT("C1") && Input->timeStamp_tail > 0.001)
        {
            timeStamp = Input->timeStamp_tail;
        }
    }
    else
    {
        public_msg = false;
    }

    /*
    static FString runtimeConfig;
    if (runtimeConfig.IsEmpty())
    {
        if (GConfig->GetString(TEXT("Sensor"), TEXT("TadsimConfigPath"), runtimeConfig, GGameIni))
        {
            runtimeConfig += "/runtime.ini";
        }
    }
    static unsigned int flushing = 0;
    bool disable = false;
    bool flush = (flushing++) % 20 == 0;
    if (flush)
    {
        //UE_LOG(LogTemp, Log, TEXT("Flush lidar %d `s runtime.ini"), config.id);
        ReloadConfig(NULL, *runtimeConfig);
        GConfig->Flush(true, runtimeConfig);
    }
    if (GConfig->GetBool(TEXT("Lidar"), *(TEXT("Disable") + FString::FromInt(config.id)), disable,
    runtimeConfig))
    {
        if (disable)
        {
            if (flush)
            {
                UE_LOG(LogTemp, Log, TEXT("Lidar %d is disable"), config.id);
            }
            return;
        }
    }*/
    // 计算当前帧的旋转角度
    if (LidarMeasurement.TimeStamp0 < 0)
    {
        UE_LOG(LogTemp, Log, TEXT("LidarMeasurement.TimeStamp0 < 0 %f"), timeStamp);
        LidarMeasurement.TimeStamp0 = timeStamp;
        LidarMeasurement.LidarBodyLoc0 = GetActorLocation();
        // return;
    }
    LidarMeasurement.TimeStamp = timeStamp;
    float DeltaTime = (timeStamp - LidarMeasurement.TimeStamp0) * 0.001f;
    if (timeStamp == 0.0)
    {
        DeltaTime = 1.f / LidarSensor->getRotationFrequency();
        UE_LOG(LogTemp, Log, TEXT("set delta time to 100ms when timeStamp == 0"));
    }
    UE_LOG(LogTemp, Log, TEXT("DeltaTime  %f %f %f "), DeltaTime, timeStamp, LidarMeasurement.TimeStamp0);
    if ((DeltaTime - 1.f / LidarSensor->getRotationFrequency()) > -0.001f)
    {
        UE_LOG(
            LogTemp, Log, TEXT("DeltaTime  %f %f %f"), timeStamp, DeltaTime, 1.f / LidarSensor->getRotationFrequency());
        LidarMeasurement.HorizontalPos = 0;
        DeltaTime = 1.f / LidarSensor->getRotationFrequency();
    }

    LidarMeasurement.HorizontalToScan = FMath::RoundHalfFromZero(
        LidarSensor->getRotationFrequency() * LidarSensor->getHorizontalScanCount() * DeltaTime);
    if (LidarMeasurement.HorizontalToScan > LidarSensor->getHorizontalScanCount())
    {
        LidarMeasurement.HorizontalPos = 0;
        LidarMeasurement.HorizontalToScan = LidarSensor->getHorizontalScanCount();
    }
    LidarMeasurement.HorizontalToScan -= LidarMeasurement.HorizontalToScan % LidarSensor->getHorizontalScanMinUnit();

    if (LidarMeasurement.HorizontalToScan == 0)
    {
        return;
    }
    LidarMeasurement.TimeSpan = (double) LidarMeasurement.HorizontalToScan * 1000.0 /
                                (LidarSensor->getRotationFrequency() * LidarSensor->getHorizontalScanCount());

    UE_LOG(LogTemp, Log, TEXT("HorizontalToScan %f %d"), LidarMeasurement.TimeSpan, LidarMeasurement.HorizontalToScan);

    lidarMd.set_null();

    // receive weather parameters
    // if (!envManager)
    // {
    //     auto gamestate = GetWorld()->GetGameState<ADisplayGameStateBase>();
    //     if (gamestate && gamestate->syncSystem && gamestate->syncSystem->envManager)
    //     {
    //         envManager = gamestate->syncSystem->envManager;
    //     }
    // }
    // if (envManager)
    // {
    //     FSimEnvOriginInfo weather;
    //     envManager->GetCurrentEnvData().GetEnvOriginInfo(weather);
    //     lidarMd.set_snow(FMath::Max(weather.SnowFall, weather.RainFall * 0.1f));
    //     lidarMd.set_rain(weather.RainFall);
    //     lidarMd.set_fog(weather.Visibility);
    // }
    lidarMd.set_snow(0);
    lidarMd.set_rain(0);
    lidarMd.set_fog(30000);

    check(LidarMeasurement.HorizontalToScan % LidarSensor->getHorizontalScanMinUnit() == 0);

    LidarMeasurement.LidarBodyLoc = GetActorLocation();
    LidarMeasurement.LidarBodyRot = GetActorRotation();

    // 获取深度数据
    // UE_LOG(LogTemp, Log, TEXT("--------gettbuffer begin: %d"),
    // std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
    // ttttt0).count());
    TSharedPtr<LidarBuffer> buf = lidarBuffer->GetTBuffer(LidarMeasurement);
    if (!buf)
    {
        UE_LOG(LogTemp, Log, TEXT("Cannot get lidar buffer."));
        return;
    }

    // UE_LOG(LogTemp, Log, TEXT("--------gettbuffer end: %d"),
    // std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
    // ttttt0).count());

    // 推到线程上
    senddataThreadHandle->SetData(LidarMeasurement, buf);

    // publish msg
    if ((public_msg || !config.savePath.IsEmpty()) &&
        (LidarMeasurement.HorizontalPos + LidarMeasurement.HorizontalToScan) >= LidarSensor->getHorizontalScanCount())
    {
        // UE_LOG(LogTemp, Log, TEXT("--------waiting msg begin: %d"),
        // std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
        // ttttt0).count());
        SendDataThread::FrameData* tframe = 0;
        while (tframe == 0)
        {
            FPlatformProcess::Sleep(0.001);
            senddataThreadHandle->m_lidarFrame.Lock();
            if (!senddataThreadHandle->lidarFrame.empty() && senddataThreadHandle->lidarFrame.front()->ok)
            {
                tframe = senddataThreadHandle->lidarFrame.front();
                senddataThreadHandle->lidarFrame.pop_front();
            }
            senddataThreadHandle->m_lidarFrame.Unlock();
        }
        // UE_LOG(LogTemp, Log, TEXT("--------waiting msg end: %d"),
        // std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
        // ttttt0).count());

        Save(tframe->data, (tframe->timestamp0 + tframe->timestamp1) * 0.5);
        if (public_msg)
        {
            sim_msg::LidarRaw lraw;
            lraw.set_id(config.id);
            lraw.set_timestamp_begin(tframe->timestamp0);
            lraw.set_timestamp_end(tframe->timestamp1);
#pragma pack(push, 4)
            struct TMP
            {
                float x = 0;
                float y = 0;
                float z = 0;
                float i = 0;     // intensity
                uint32 t = 0;    // type
            };
#pragma pack(pop)
            TArray<TMP> ptlists;
            ptlists.Reserve(tframe->data.points.size());

            int count = 0;
            FTransform tf;
            FRotator Rotator(0, 60.0, 0);
            tf.SetRotation(Rotator.Quaternion());
            // 点云坐标转换
            for (const auto& p : tframe->data.points)
            {
                if (p.distance > 0)
                {
                    TMP tp;
                    if (coordType == TEXT("enu"))
                    {
                        tp.x = -p.y;
                        tp.y = -p.x;    // left to right
                    }
                    else
                    {
                        tp.x = p.x;
                        tp.y = -p.y;    // left to right
                    }
                    FVector point(tp.x, tp.y, tp.z);
                    point = tf.TransformPosition(point);
                    tp.x = point.X;
                    tp.y = point.Y;
                    tp.z = point.Z;
                    tp.i = p.instensity;
                    tp.t = p.tag_c;
                    ptlists.Add(tp);
                }
            }
            lraw.set_count(ptlists.Num());
            lraw.set_coord_type(sim_msg::LidarRaw_CoordType_COORD_SENSOR);
            // POSE
            FVector loc0 = tframe->loc0;
            FVector loc1 = tframe->loc1;
            FRotator rot0 = tframe->rot0;
            FRotator rot1 = tframe->rot1;
            if (coordType == TEXT("vehicle"))
            {
                lraw.set_coord_type(sim_msg::LidarRaw_CoordType_COORD_VEHICLE);
                rot0 = UKismetMathLibrary::ComposeRotators(config.installRotation.GetInverse(), rot0);
                rot1 = UKismetMathLibrary::ComposeRotators(config.installRotation.GetInverse(), rot1);
                loc0 -= rot0.RotateVector(config.installLocation);
                loc1 -= rot1.RotateVector(config.installLocation);
            }
            else if (coordType == TEXT("enu"))
            {
                lraw.set_coord_type(sim_msg::LidarRaw_CoordType_COORD_ENU);
                loc0 = FVector(0);
                loc1 = FVector(0);
                rot0 = FRotator(0, 0, 0);
                rot1 = FRotator(0, 0, 0);
            }
            double X = 0, Y = 0, Z = 0;
            hadmapue4::HadmapManager::Get()->LocalToLonLat(loc0, X, Y, Z);
            lraw.mutable_pose_first()->set_longitude(X);
            lraw.mutable_pose_first()->set_latitude(Y);
            lraw.mutable_pose_first()->set_altitude(Z);
            hadmapue4::HadmapManager::Get()->LocalToLonLat(loc1, X, Y, Z);
            lraw.mutable_pose_last()->set_longitude(X);
            lraw.mutable_pose_last()->set_latitude(Y);
            lraw.mutable_pose_last()->set_altitude(Z);
            lraw.mutable_pose_first()->set_roll(rot0.Roll * PI / 180.f);
            lraw.mutable_pose_first()->set_pitch(-rot0.Pitch * PI / 180.f);
            lraw.mutable_pose_first()->set_yaw(-(rot0.Yaw + 90.f) * PI / 180.f);
            lraw.mutable_pose_last()->set_roll(rot1.Roll * PI / 180.f);
            lraw.mutable_pose_last()->set_pitch(-rot1.Pitch * PI / 180.f);
            lraw.mutable_pose_last()->set_yaw(-(rot1.Yaw + 90.f) * PI / 180.f);

            if (ptlists.Num() == 0)
            {
                UE_LOG(LogTemp, Log, TEXT("Valid point num is zero."));
            }
            else
                lraw.set_point_lists(ptlists.GetData(), ptlists.Num() * sizeof(TMP));

            lraw.SerializeToString(&_Output.serialize_string);
        }

        // UE_LOG(LogTemp, Log, TEXT("--------serialize msg end: %d"),
        // std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
        // ttttt0).count());
        delete tframe;
    }
    // prepare for next frame
    LidarMeasurement.HorizontalPos =
        (LidarMeasurement.HorizontalPos + LidarMeasurement.HorizontalToScan) % LidarSensor->getHorizontalScanCount();
    LidarMeasurement.TimeStamp0 = timeStamp;
    LidarMeasurement.LidarBodyLoc0 = GetActorLocation();
}

ISimActorInterface* ATLidarSensor::Install(const FSensorConfig& _Config)
{
    auto SimActor = Super::Install(_Config);
    auto DepthLidar = static_cast<ALidarBufferDepth*>(lidarBuffer.get());
    if (DepthLidar && SimActor)
    {
        AActor* Ego = Cast<AActor>(SimActor);
        if (Ego)
        {
            DepthLidar->IgnoreActor(Ego);
        }
    }
    return SimActor;
}

// 点云保存pcd
bool ATLidarSensor::Save(const lidar::TraditionalLidar::lidar_ptset& data, double timeStamp)
{
    if (config.savePath.IsEmpty())
    {
        return true;
    }

    if (!FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*config.savePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("CameraSensorComponent WARN: Directory dont Exists!"));
    }

    size_t pn = 0;
    for (const auto& p : data.points)
    {
        if (p.distance > 0)
        {
            pn++;
        }
    }
    if (pn == 0)
    {
        return true;
    }
    FString savename = config.savePath + config.model + TEXT("_") + FString::FromInt(config.id) + TEXT("_") +
                       FString::FromInt(timeStamp) + TEXT(".") + TEXT("pcd");

    FString StringData;
    StringData += "# .PCD v.7 - Point Cloud Data file format\r\n";
    StringData += "VERSION 0.7\r\n";
    StringData += "FIELDS x y z intensity label\r\n";
    StringData += "SIZE 4 4 4 4 4\r\n";
    StringData += "TYPE F F F F I \r\n";
    StringData += "COUNT 1 1 1 1 1\r\n";
    StringData += FString(TEXT("WIDTH ")) + FString::FromInt(pn) + LINE_TERMINATOR;
    StringData += "HEIGHT 1\r\n";
    StringData += "VIEWPOINT 0 0 0 1 0 0 0\r\n";
    StringData += FString(TEXT("POINTS ")) + FString::FromInt(pn) + LINE_TERMINATOR;
    StringData += "DATA ascii\r\n";

    for (auto& p : data.points)
    {
        if (p.distance > 0)
        {
            if (coordType == TEXT("enu"))
            {
                StringData += FString::SanitizeFloat(-p.y);
                StringData += " ";
                StringData += FString::SanitizeFloat(-p.x);
            }
            else
            {
                StringData += FString::SanitizeFloat(p.x);
                StringData += " ";
                StringData += FString::SanitizeFloat(-p.y);
            }
            StringData += " ";
            StringData += FString::SanitizeFloat(p.z);
            StringData += " ";
            StringData += FString::SanitizeFloat(p.instensity);
            StringData += " ";
            StringData += FString::Printf(TEXT("%u"), p.tag_c);

            StringData += LINE_TERMINATOR;
        }
    }

    return GetDisplayInstance()->GetSaveDataHandle()->SaveString(StringData, savename);
}

ATLidarSensor::SendDataThread::SendDataThread(ATLidarSensor* ls) : lidarActor(ls)
{
    // 生成多个线程，一个负责发送，其余的负责处理点云
    send_key = true;
    // Initialize FEvent (as a cross platform (Confirmed Mac/Windows))
    Thread.Add(FRunnableThread::Create(this, TEXT("SendDataThread0"), 0, EThreadPriority::TPri_Normal));
    Thread.Add(FRunnableThread::Create(this, TEXT("SendDataThread1"), 0, EThreadPriority::TPri_Normal));
    m_Kill = false;
}

ATLidarSensor::SendDataThread::~SendDataThread()
{
    for (auto t : Thread)
    {
        if (t)
        {
            // Cleanup the worker thread
            delete t;
        }
    }
}

void ATLidarSensor::SendDataThread::SetData(const FTLidarMeasurement& measure, TSharedPtr<LidarBuffer> buffer)
{
    // 保存接受到的数据
    m_lidarBuffer.Lock();
    if (lidarBuffers.size() > 5)
    {
        UE_LOG(LogTemp, Warning, TEXT("Lidar SendData thread too slow"));
    }
    BufferData buf;
    buf.buffer = buffer;
    buf.measure = measure;
    lidarBuffers.push_back(buf);
    m_lidarBuffer.Unlock();
}

bool ATLidarSensor::SendDataThread::Init()
{
    return true;
}

uint32 ATLidarSensor::SendDataThread::Run()
{
    // FPlatformProcess::Sleep(0.3);

    bool send_th = false;
    m_lidarBuffer.Lock();
    send_th = send_key;
    send_key = false;
    m_lidarBuffer.Unlock();
    int hw = lidarActor->LidarSensor->getHorizontalScanCount();
    int rn = lidarActor->LidarSensor->getRaysNum();
    int rtn = lidarActor->LidarSensor->getReturnNum();
    if (send_th)
    {
        // 发送线程
        bool saved = !lidarActor->config.savePath.IsEmpty();
        while (!m_Kill)
        {
            double timestamp = 0;
            FVector loc;     // first loc
            FRotator rot;    // first rot
            TSharedPtr<lidar::TraditionalLidar::lidar_ptset> Buffer;
            m_lidarBuffer.Lock();
            if (lidarBuffers.size() > 100)
            {
                // 队列数据太多，肯定处理不过来了，丢弃几个
                UE_LOG(LogTemp, Warning, TEXT("Lidar SendData FRONT buf blocked"));
                lidarBuffers.pop_front();
                lidarBuffers.pop_front();
                lidarBuffers.pop_front();
            }
            if (!lidarBuffers.empty() && lidarBuffers.front().state == 2)
            {
                Buffer = lidarBuffers.front().dataPtr;
                timestamp = lidarBuffers.front().measure.TimeStamp;
                loc = lidarBuffers.front().measure.LidarBodyLoc;
                rot = lidarBuffers.front().measure.LidarBodyRot;
                UE_LOG(LogTemp, Warning, TEXT("lidarBuffers %d %f"), lidarBuffers.size(), timestamp);
                lidarBuffers.pop_front();
            }
            m_lidarBuffer.Unlock();
            if (Buffer == 0)
            {
                FPlatformProcess::Sleep(0.001);
                continue;
            }
            // 没数据，先睡会觉
            if (Buffer->channels.empty() || Buffer->points.empty())
            {
                FPlatformProcess::Sleep(0.001);
                continue;
            }
            // UE_LOG(LogTemp, Log, TEXT("--------send pt begin: %d"),
            // std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()
            // - ttttt0).count());
            if (lidarActor->public_msg || !lidarActor->config.savePath.IsEmpty())
            {
                // 收集一帧的数据，然后发送出去
                m_lidarFrame.Lock();
                if (lidarFrame.empty())
                {
                    // 新的一帧
                    lidarFrame.push_back(new FrameData());
                    lidarFrame.back()->data.channels.reserve(hw);
                    lidarFrame.back()->data.points.reserve(hw * rn * rtn);
                    lidarFrame.back()->ok = false;
                    lidarFrame.back()->timestamp0 = std::numeric_limits<double>::max();
                    lidarFrame.back()->timestamp1 = -std::numeric_limits<double>::max();
                    lidarFrame.back()->loc0 = loc;
                    lidarFrame.back()->rot0 = rot;
                    lidarFrame.back()->loc1 = loc;
                    lidarFrame.back()->rot1 = rot;
                }
                FrameData* nstframe = lidarFrame.back();
                m_lidarFrame.Unlock();

                int fd0 = Buffer->channels.size();
                // 如果当前包是一帧，直接丢掉收集的数据，使用当前包为当前帧
                if (Buffer->channels.size() >= hw)
                {
                    nstframe->data.channels.clear();
                    nstframe->data.points.clear();
                    nstframe->timestamp0 = std::numeric_limits<double>::max();
                    nstframe->timestamp1 = -std::numeric_limits<double>::max();
                    nstframe->loc0 = loc;
                    nstframe->rot0 = rot;
                    fd0 = Buffer->channels.size();
                }
                else
                {
                    // 否则，从包里找位置，看多少能填满当前帧
                    int fd = -1;
                    for (fd = Buffer->channels.size() - 1; fd >= 0; fd--)
                    {
                        if (Buffer->channels.at(fd).hor_pos == (hw - 1))
                        {
                            fd0 = fd + 1;
                            break;
                        }
                    }
                }
                // 更新当前的刻度
                nstframe->data.channels.insert(
                    nstframe->data.channels.end(), Buffer->channels.begin(), Buffer->channels.begin() + fd0);
                nstframe->data.points.insert(
                    nstframe->data.points.end(), Buffer->points.begin(), Buffer->points.begin() + fd0 * rn * rtn);
                nstframe->timestamp0 = std::min(timestamp, nstframe->timestamp0);
                nstframe->timestamp1 = std::max(timestamp, nstframe->timestamp1);
                nstframe->loc1 = loc;
                nstframe->rot1 = rot;

                // 收集完成，发送出去，并开始新的收集
                if (fd0 < Buffer->channels.size() || nstframe->data.channels.size() >= hw)
                {
                    for (size_t i = 0; i < hw; ++i)
                    {
                        nstframe->data.channels[i].points = &nstframe->data.points[i * rn * rtn];
                    }
                    // 更新布朗噪声
                    lidarActor->lidarMd.update_brownion_pos();

                    m_lidarFrame.Lock();
                    nstframe->ok = true;
                    // 新的一帧
                    lidarFrame.push_back(new FrameData());
                    lidarFrame.back()->data.channels.reserve(hw);
                    lidarFrame.back()->data.points.reserve(hw * rn * rtn);
                    lidarFrame.back()->ok = false;
                    lidarFrame.back()->timestamp0 = std::numeric_limits<double>::max();
                    lidarFrame.back()->timestamp1 = -std::numeric_limits<double>::max();
                    lidarFrame.back()->loc0 = loc;
                    lidarFrame.back()->rot0 = rot;
                    lidarFrame.back()->loc1 = loc;
                    lidarFrame.back()->rot1 = rot;
                    nstframe = lidarFrame.back();
                    m_lidarFrame.Unlock();
                    if (fd0 < Buffer->channels.size())
                    {
                        nstframe->data.channels.insert(
                            nstframe->data.channels.end(), Buffer->channels.begin() + fd0, Buffer->channels.end());
                        nstframe->data.points.insert(
                            nstframe->data.points.end(), Buffer->points.begin() + fd0 * rn * rtn, Buffer->points.end());
                        nstframe->timestamp0 = std::min(timestamp, nstframe->timestamp0);
                        nstframe->timestamp1 = std::max(timestamp, nstframe->timestamp1);
                    }
                }
            }
            // UE_LOG(LogTemp, Log, TEXT("--------send pt end: %d"),
            // std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()
            // - ttttt0).count());
            //  发送当前包，一般为DUP方式，由具体型号决定
            // uint32_t pn = lidarActor->LidarSensor->package(*Buffer);
            // UE_LOG(LogTemp, Log, TEXT("sendth:%d, %d, %d"), Buffer.measure.HorizontalPos,
            // Buffer.measure.HorizontalToScan, pn);
        }
    }
    else
    {
        // 点云处理线程
        while (!m_Kill)
        {
            bool has_buf = false;
            BufferData* buffer = 0;
            m_lidarBuffer.Lock();
            if (!lidarBuffers.empty())
            {
                for (auto& buf : lidarBuffers)
                {
                    if (buf.state == 0)
                    {
                        buf.state = 1;
                        buffer = &buf;
                        break;
                    }
                }
            }
            m_lidarBuffer.Unlock();

            if (!buffer)
            {
                FPlatformProcess::Sleep(0.001);
                continue;
            }
            // UE_LOG(LogTemp, Log, TEXT("--------get pt begin: %d"),
            // std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()
            // - ttttt0).count());
            //  设置POSE
            buffer->dataPtr = MakeShared<lidar::TraditionalLidar::lidar_ptset>();
            if (lidarActor->coordType == TEXT("enu"))
            {
                FTransform tf;
                tf.SetLocation(buffer->measure.LidarBodyLoc * 0.01f);
                tf.SetRotation(buffer->measure.LidarBodyRot.Quaternion());
                lidarActor->lidarBuffer->setRotationTranslation(tf);
            }
            // 调用点云处理
            if (lidarActor->lidarBuffer->GetPoints(buffer->buffer.Get(), buffer->measure, *buffer->dataPtr))
            {
                // UE_LOG(LogTemp, Log, TEXT("--------get pt over: %d"),
                // std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()
                // - ttttt0).count());

                m_lidarBuffer.Lock();
                buffer->state = 2;
                m_lidarBuffer.Unlock();
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("GetPoints faild"));
            }
            // UE_LOG(LogTemp, Log, TEXT("--------get pt end: %d"),
            // std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()
            // - ttttt0).count());
        }
    }

    return 0;
}

void ATLidarSensor::SendDataThread::EnsureCompletion()
{
    m_Kill = true;    // Thread kill condition "while (!m_Kill){...}"
    for (auto t : Thread)
    {
        if (t)
            t->WaitForCompletion();
    }
}
