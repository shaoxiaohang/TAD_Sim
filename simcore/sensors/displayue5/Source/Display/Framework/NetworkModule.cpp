#include "NetworkModule.h"

DEFINE_LOG_CATEGORY_STATIC(SimLogNet, Log, All);
TXSIM_MODULE(NetworkModule)

NetworkModule::NetworkModule()
{
    threadSuspendedEvent = FPlatformProcess::GetSynchEventFromPool();
}

NetworkModule::~NetworkModule()
{
    FPlatformProcess::ReturnSynchEventToPool(threadSuspendedEvent);
    threadSuspendedEvent = nullptr;
}

void NetworkModule::Init(tx_sim::InitHelper& helper)
{
    std::chrono::time_point<std::chrono::system_clock> Start, End;
    Start = std::chrono::system_clock::now();

    GConfig->GetString(TEXT("MessageTopic"), TEXT("Traffic"), TrafficTopic, GGameIni);
    GConfig->GetString(TEXT("MessageTopic"), TEXT("Location"), LocationTopic, GGameIni);

    // UE_LOG(SimLogNet, Log, TEXT("TRAFFIC %s "), *TrafficTopic);

    helper.Subscribe(TCHAR_TO_ANSI(*TrafficTopic));

    FString UnionTrafficTopic = UnionPrefixStr + LocationTopic;
    helper.Subscribe(TCHAR_TO_ANSI(*UnionTrafficTopic));

    if (!helper.GetParameter("time0").empty())
    {
        time0 = std::atof(helper.GetParameter("time0").c_str());
        UE_LOG(SimLogNet, Log, TEXT("Display init time0 = %f"), time0);
    }
    if (!helper.GetParameter("step_size").empty())
    {
        realstep = std::atof(helper.GetParameter("step_size").c_str());
        UE_LOG(SimLogNet, Log, TEXT("Display init realstep = %f"), realstep);
    }

    FString device;
    if (!FParse::Value(FCommandLine::Get(), TEXT("-topicId="), device))
    {
        if (!FParse::Value(FCommandLine::Get(), TEXT("-device="), device))
        {
            device = TEXT("0");
        }
    }
    SensorTopic = TEXT("DISPLAYSENSOR_") + device;
    PoseTopic = TEXT("DISPLAYPOSE_") + device;

    helper.Publish(std::string(TCHAR_TO_ANSI(*SensorTopic)));
    helper.Publish(std::string(TCHAR_TO_ANSI(*PoseTopic)));
    UE_LOG(SimLogNet, Log, TEXT("Display publish topic is %s"), *SensorTopic);

    {
        FScopeLock ScopeLock(&mutex_Input);

        TSharedPtr<FSimInitIn> NewInPtr = MakeShared<FSimInitIn>();
        NewInPtr->clientNum = 1;
        NewInPtr->name = TEXT("INIT");
        NewInPtr->timeStamp = -1;

        myGameInstance->simInDataArry.Add(NewInPtr);
        myGameInstance->bSimInDataRefreshed = true;
    }
    myGameInstance->threadSuspendedEvent->Trigger();
    End = std::chrono::system_clock::now();
    std::chrono::duration<double> CostTime = End - Start;
    UE_LOG(SimLogNet, Log, TEXT("Display Init Cost Time: %f seconds"), CostTime.count());
}

void NetworkModule::Reset(tx_sim::ResetHelper& helper)
{
    UE_LOG(SimLogNet, Log, TEXT("Reset begin"));
    std::chrono::time_point<std::chrono::system_clock> Start, End;
    Start = std::chrono::system_clock::now();
    {
        FScopeLock ScopeLock(&mutex_Input);
        TSharedPtr<FSimResetIn> NewInPtr = MakeShared<FSimResetIn>();
        NewInPtr->configFilePath = UTF8_TO_TCHAR(helper.scenario_file_path().c_str());
        NewInPtr->configFilePath = NewInPtr->configFilePath.Replace(TEXT("\\"), TEXT("/"));

        sim_msg::Location StartLoc;
        StartLoc.ParseFromString(helper.ego_start_location().c_str());

        NewInPtr->startLon = StartLoc.position().x();
        NewInPtr->startLat = StartLoc.position().y();
        NewInPtr->startAlt = StartLoc.position().z();
        NewInPtr->startSpeed =
            FVector(StartLoc.velocity().x(), StartLoc.velocity().y(), StartLoc.velocity().z()).Size();
        NewInPtr->startTheta = StartLoc.rpy().z();
        UE_LOG(SimLogNet, Log, TEXT("start yaw is %f"), NewInPtr->startTheta);
        NewInPtr->mapDataBasePath = UTF8_TO_TCHAR(helper.map_file_path().c_str());
        NewInPtr->mapDataBasePath = NewInPtr->mapDataBasePath.Replace(TEXT("\\"), TEXT("/"));
        NewInPtr->mapDataBaseName = FPaths::GetCleanFilename(NewInPtr->mapDataBasePath);
        NewInPtr->sceneBuffer = helper.scene_pb();

        auto EgoPath = helper.ego_path();
        for (auto& v : EgoPath)
        {
            FVector point;
            point.X = v.x;
            point.Y = v.y;
            point.Z = v.z;
            NewInPtr->egoPath.Add(point);
            UE_LOG(SimLogNet, Log, TEXT("ego path %f %f %f"), v.x, v.y, v.z);
        }

        FString tadsim_path;
        bool bUseLocalScenarioDir = false;
        GConfig->GetBool(TEXT("Sensor"), TEXT("bUseLocalScenarioDir"), bUseLocalScenarioDir, GGameIni);
        if (bUseLocalScenarioDir)
        {
            if (GConfig->GetString(TEXT("Sensor"), TEXT("TadsimConfigPath"), tadsim_path, GGameIni))
            {
            }
        }
        if (tadsim_path.IsEmpty())
        {
            if (FParse::Value(FCommandLine::Get(), TEXT("-tadsim_dir="), tadsim_path))
            {
            }
        }
        NewInPtr->tadsimPath =
            FPaths::GetPath(FPaths::GetPath(FPaths::GetPath(FPaths::GetPath(NewInPtr->configFilePath))));
        if (!tadsim_path.IsEmpty())
        {
            tadsim_path = tadsim_path.Replace(TEXT("\\"), TEXT("/"));

            UE_LOG(SimLogNet, Log, TEXT("tadsim dir is %s"), *tadsim_path);
            FString cfgStr =
                NewInPtr->configFilePath.Right(NewInPtr->configFilePath.Len() - NewInPtr->tadsimPath.Len() - 1);
            NewInPtr->configFilePath = FPaths::Combine(tadsim_path, cfgStr);

            cfgStr = NewInPtr->mapDataBasePath.Right(NewInPtr->mapDataBasePath.Len() - NewInPtr->tadsimPath.Len() - 1);
            NewInPtr->mapDataBasePath = FPaths::Combine(tadsim_path, cfgStr);

            NewInPtr->tadsimPath = tadsim_path;
        }

        NewInPtr->name = TEXT("RESET");
        NewInPtr->timeStamp = 0;
        myGameInstance->simInDataArry.Add(NewInPtr);
        asynchronousMode = myGameInstance->GetGameConfig(TEXT("Mode"), TEXT("Asynchronous")) == TEXT("true");
        myGameInstance->SetAsynchronousMode(asynchronousMode);
        myGameInstance->bSimInDataRefreshed = true;

        myGameInstance->ModuleGroupName = UTF8_TO_TCHAR(helper.group_name().c_str());
    }

    myGameInstance->threadSuspendedEvent->Trigger();
    threadSuspendedEvent->Wait();

    End = std::chrono::system_clock::now();
    std::chrono::duration<double> CostTime = End - Start;
    UE_LOG(SimLogNet, Log, TEXT("Display Reset Cost Time: %f seconds"), CostTime.count());

    if (!myGameInstance->ResetFaildStr.IsEmpty())
    {
        throw std::runtime_error(TCHAR_TO_ANSI(*myGameInstance->ResetFaildStr));
    }
}

void NetworkModule::Step(tx_sim::StepHelper& helper)
{
    double timestamp = helper.timestamp();
    UE_LOG(SimLogNet, Log, TEXT("NetworkModule Step %f"), timestamp);
    double bei = realstep > 0 ? ((timestamp - time0) / realstep) : 1.0;
    if (bei < 0 || FMath::Modf(bei, &bei) > 1e-4)
    {
        // UE_LOG(SimLogNet, Log, TEXT("Display step jump: %f"), timestamp);
        return;
    }

    std::chrono::time_point<std::chrono::system_clock> Start, End;
    std::chrono::duration<double> CostTime;
    Start = std::chrono::system_clock::now();

    {
        FScopeLock ScopeLock(&mutex_Input);
        simUpdateIn.timeStamp = timestamp;

        std::string strUnionLocation;
        helper.GetSubscribedMessage(TCHAR_TO_ANSI(*(UnionPrefixStr + LocationTopic)), strUnionLocation);

        std::string strTraffic;
        helper.GetSubscribedMessage(TCHAR_TO_ANSI(*TrafficTopic), strTraffic);

        TSharedPtr<FSimUpdateIn> NewInPtr = MakeShared<FSimUpdateIn>();
        NewInPtr->name = TEXT("UPDATE");
        NewInPtr->timeStamp = timestamp;

        sim_msg::Union UnionLocation;
        UnionLocation.ParseFromString(strUnionLocation);

        NewInPtr->egoData.Empty(UnionLocation.messages_size());
        for (int32 i = 0; i < UnionLocation.messages_size(); ++i)
        {
            const auto& msg = UnionLocation.messages(i);
            std::string groupname = msg.groupname();
            std::string content = msg.content();
            sim_msg::Location locationMsg;
            if (locationMsg.ParseFromString(content))
            {
                NewInPtr->egoData.Emplace(UTF8_TO_TCHAR(groupname.c_str()), locationMsg);
            }
            UE_LOG(SimLogNet, Log, TEXT("LOCATION group %s time %f x %.8f y %.8f z %.8f"),
                UTF8_TO_TCHAR(groupname.c_str()), timestamp, locationMsg.position().x(), locationMsg.position().y(),
                locationMsg.position().z());
        }
        NewInPtr->trafficData.ParseFromString(strTraffic);
        // UE_LOG(SimLogNet, Log, TEXT("TRAFFIC %f %s"), timestamp,
        // UTF8_TO_TCHAR(NewInPtr->trafficData.DebugString().c_str()));
        myGameInstance->simInDataArry.Add(NewInPtr);
        UE_LOG(SimLogNet, Log, TEXT("ADD SIMDATA %s %f "), *NewInPtr->name, timestamp);
        myGameInstance->bSimInDataRefreshed = true;
    }

    if (myGameInstance->bIsFrameSync)
    {
        myGameInstance->threadSuspendedEvent->Trigger();
    }

    // End = std::chrono::system_clock::now();
    // CostTime = End - Start;
    // UE_LOG(SimLogNet, Log, TEXT("Display Update sync1 Cost Time: %f seconds"),
    // CostTime.count());//*/
    //  Wait for gameinstance complete
    if (!asynchronousMode)
    {
        threadSuspendedEvent->Wait();
    }

    PublicUpdateMessage(helper);

    End = std::chrono::system_clock::now();
    CostTime = End - Start;
    UE_LOG(SimLogNet, Log, TEXT("Display Update Cost Time: %f seconds"), CostTime.count());    //*/
}

void NetworkModule::Stop(tx_sim::StopHelper& helper)
{
}

FEvent* NetworkModule::getThreadSuspendedEvent()
{
    return threadSuspendedEvent;
}

void NetworkModule::PublicUpdateMessage(tx_sim::StepHelper& helper)
{
    FScopeLock ScopeLock(&mutex_Output);
    UE_LOG(SimLogNet, Log, TEXT("PublicUpdateMessage Size: %d"), myGameInstance->simOutDataArry.Num());
    for (const auto& sout : myGameInstance->simOutDataArry)
    {
        if (sout->datatype == 1)
        {
            const FSimUpdateOut* simOut = StaticCast<const FSimUpdateOut*>(sout.Get());
            std::string payload_;
            payload_.clear();
            if (simOut->trafficPose.SerializeToString(&payload_) && payload_.size())
            {
                // UE_LOG(SimLogNet, Log, TEXT("PublictrafficPose %s"),
                //     ANSI_TO_TCHAR(simOut->trafficPose.DebugString().c_str()));
                UE_LOG(SimLogNet, Log, TEXT("Send trafficPose: %f"), simOut->trafficPose.timestamp());
                helper.PublishMessage(std::string(TCHAR_TO_ANSI(*PoseTopic)), payload_);
            }
        }
        if (sout->datatype == 2)
        {
            const FSimSensorUpdateOut* simSenOut = StaticCast<const FSimSensorUpdateOut*>(sout.Get());
            std::string payload_;
            if (simSenOut->sensorData.SerializeToString(&payload_) && payload_.size())
            {
                helper.PublishMessage(std::string(TCHAR_TO_ANSI(*SensorTopic)), payload_);
            }
            UE_LOG(SimLogNet, Log, TEXT("Send SensorData: %f"), simSenOut->sensorData.timestamp());
        }
    }
    myGameInstance->simOutDataArry.SetNum(0);
}
