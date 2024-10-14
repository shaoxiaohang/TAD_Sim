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
        NewInPtr->mapDataBasePath = UTF8_TO_TCHAR(helper.map_file_path().c_str());
        NewInPtr->mapDataBasePath = NewInPtr->mapDataBasePath.Replace(TEXT("\\"), TEXT("/"));
        NewInPtr->mapDataBaseName = FPaths::GetCleanFilename(NewInPtr->mapDataBasePath);
        NewInPtr->sceneBuffer = helper.scene_pb();

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
}

void NetworkModule::Stop(tx_sim::StopHelper& helper)
{
}

FEvent* NetworkModule::getThreadSuspendedEvent()
{
    return threadSuspendedEvent;
}
