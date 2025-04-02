#include "DisplayGameInstance.h"

#include "Data/CatalogDataSource.h"
#include "DisplayGameModeBase.h"
#include "DisplayNetworkManager.h"
#include "DisplayPlayerState.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "LoaderBPFunctionLibrary.h"
#include "SaveDataThread.h"

// hadmap
#include "HadmapManager.h"

#define CONSUMED_MAXTICK 1000000

DEFINE_LOG_CATEGORY(LogSimSystem);
DEFINE_LOG_CATEGORY(LogSimDebug);
DEFINE_LOG_CATEGORY_STATIC(LogSimGameInstance, Log, All);

UDisplayGameInstance::UDisplayGameInstance(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    // Apply for network thread
    threadSuspendedEvent = FPlatformProcess::GetSynchEventFromPool();
}

void UDisplayGameInstance::Init()
{
    UE_LOG(LogSimSystem, Log, TEXT("Build time: %s %s"), TEXT(__DATE__), TEXT(__TIME__));
    // add tick function for game instance
    TickDelegateHandle =
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UDisplayGameInstance::Tick));
    UGameInstance::Init();

    UE_LOG(LogSimSystem, Log, TEXT("Device id: %s "), *FGenericPlatformMisc::GetDeviceId());

    // Create SaveData thread
    savedataThreadHandle = MakeShared<SaveDataThread>();

    /* Create CatalogDataSource instance */
    CatalogDataSource = NewObject<UCatalogDataSource>();
    if (CatalogDataSource)
    {
        CatalogDataSource->AddToRoot();
    }

    RuntimeMeshLoader = NewObject<URuntimeMeshLoader>();
    if (RuntimeMeshLoader)
    {
        RuntimeMeshLoader->AddToRoot();
        RuntimeMeshLoader->Init();
        URuntimeMeshLoader::SetInstance(RuntimeMeshLoader);
    }

    /* ======Processing Command Arguments===== */

    // Get user directory
    FString UserDirStr;
    if (FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDirStr))
    {
        UE_LOG(LogSimSystem, Log, TEXT("UserDirStr:%s"), *UserDirStr);
    }

    // Get framesync mode
    FString ModeStr;
    if (FParse::Value(FCommandLine::Get(), TEXT("-mode="), ModeStr))
    {
        modeName = std::string(TCHAR_TO_UTF8(*ModeStr));
    }
    else
    {
        UE_LOG(LogSimSystem, Log, TEXT("-mode is null."));
    }
    if (modeName == "FrameSync")
    {
        bIsFrameSync = true;
    }
    else if (modeName == "FrameAsync")
    {
        bIsFrameSync = false;
    }
    UE_LOG(LogSimSystem, Log, TEXT("-mode use %s"), UTF8_TO_TCHAR(modeName.c_str()));

    GConfig->GetInt(TEXT("Mode"), TEXT("SyncModeWait"), SyncModeWait, GGameIni);
    UE_LOG(LogSimSystem, Log, TEXT("SyncModeWait is %u"), SyncModeWait);

    // Get VIL flag
    FString FlagVilSendMsg;
    if (FParse::Value(FCommandLine::Get(), TEXT("-vilmsg"), FlagVilSendMsg))
    {
        sendVilMsg = true;
    }
    UE_LOG(LogSimSystem, Log, TEXT("-novilmsg use, forbid send topic msg about vil!"));

    // Get HIL flag
    FString FlagHIL;
    if (FParse::Value(FCommandLine::Get(), TEXT("-hil="), FlagHIL))
    {
        FString xleft, yright;
        FlagHIL.Split(TEXT("x"), &xleft, &yright);
        nHILpos.X = FCString::Atof(*xleft);
        nHILpos.Y = FCString::Atof(*yright);
    }
    UE_LOG(LogSimSystem, Log, TEXT("hil pos = %f,%f"), nHILpos.X, nHILpos.Y);

    // Get Enviroment var
    FString EnviromentVar, EnglishValue;
    GConfig->GetString(TEXT("Language"), TEXT("EnviromentVar"), EnviromentVar, GGameIni);
    GConfig->GetString(TEXT("Language"), TEXT("EnglishValue"), EnglishValue, GGameIni);
    FString Language = FPlatformMisc::GetEnvironmentVariable(*EnviromentVar);

    if (Language.Equals(EnglishValue, ESearchCase::IgnoreCase))
    {
        UKismetInternationalizationLibrary::SetCurrentCulture(TEXT("en"), false);
    }
    else    // default chinese
    {
        UKismetInternationalizationLibrary::SetCurrentCulture(TEXT("ch"), false);
    }

    /* =====Load Config===== */

    // Get sync mode
    GConfig->GetBool(TEXT("Mode"), TEXT("Asynchronous"), asynchronousMode, GGameIni);
    GConfig->GetBool(TEXT("Mode"), TEXT("SyncOneFrame"), syncOneFrame, GGameIni);

    // Get lobby map path
    GConfig->GetString(TEXT("GlobalSettings"), TEXT("LobbyMapPath"), mapPath_Lobby, GGameIni);
}

SaveDataThread* UDisplayGameInstance::GetSaveDataHandle() const
{
    return savedataThreadHandle.Get();
}

bool UDisplayGameInstance::Tick(float DeltaSeconds)
{
#if WITH_EDITOR
    return true;
#endif

    // check clients all connected
    if (!bAllClientsLogin)
    {
        return true;
    }
    if (syncOneFrame)    // sigle frame
    {
        OutputData();    // SensorManger update
        SendSimData();
        ReceiveSimData();    //
        SyncSimData();       // location update.
    }
    else
    {
        OutputData();        // SensorManger update, sensor update from last location
        ReceiveSimData();    // read location, begin of step and waiting for step
        SyncSimData();       // update all actor location.
        SendSimData();       // public sensor
    }
    return true;
}

void UDisplayGameInstance::OutputData()
{
    // if (currentSimInData)
    // {
    //     UE_LOG(LogSimGameInstance, Log, TEXT("UDisplayGameInstance.OutputData = %s %f"), *currentSimInData->name,
    //         currentSimInData->timeStamp);
    // }
    if (!currentSimInData || currentSimInData->bIsConsumed > 0)
    {
        return;
    }
    if (currentSimInData->name == TEXT("UPDATE"))
    {
        FSimIn SensorInData;
        SensorInData.name = TEXT("OUTPUT_SENSOR");
        SensorInData.timeStamp = currentSimInData->timeStamp;
        FSimUpdateIn* UpdateIn = StaticCast<FSimUpdateIn*>(currentSimInData.Get());
        if (sim_msg::Location* Location = UpdateIn->egoData.Find(ModuleGroupName))
        {
            SensorInData.timeStamp_ego = Location->t() * 1000;
            UE_LOG(LogSimGameInstance, Log, TEXT("UDisplayGameInstance.OUTPUT_SENSOR = %f"), SensorInData.timeStamp_ego);
        }
        if (sim_msg::Location* LocationContainer = UpdateIn->egoContainerData.Find(ModuleGroupName))
        {
            SensorInData.timeStamp_tail = LocationContainer->t() * 1000;
        }
        SimInput(SensorInData);
    }
}

void UDisplayGameInstance::ReceiveSimData()
{
    if (currentSimInData && currentSimInData->bIsConsumed < CONSUMED_MAXTICK)
    {
        currentSimInData->bIsConsumed += 1;
    }
    if (bIsFrameSync)
    {
        uint32 waitTime = SyncModeWait <= 0 ? MAX_uint32 : SyncModeWait;
        if (!currentSimInData)
        {
            threadSuspendedEvent->Wait(waitTime);
        }
        else
        {
            if (currentSimInData->name == TEXT("UPDATE"))
            {
                threadSuspendedEvent->Wait(waitTime);
            }
            else if (currentSimInData->name == TEXT("RESET"))
            {
                if (currentSimInData->bIsConsumed > CONSUMED_MAXTICK)
                {
                    threadSuspendedEvent->Wait(waitTime);
                }
            }
            else if (currentSimInData->name == TEXT("STOP"))
            {
                if (!NeedExit)
                {
                    threadSuspendedEvent->Wait(waitTime);
                }
            }
        }
    }
    FScopeLock ScopeLock(&displayNetworkManager->displayModule->mutex_Input);
    if (simInDataArry.Num() > 0 && currentSimInData != simInDataArry.Top())
    {
        for (int i = simInDataArry.Num() - 2; i >= 2; i--)
        {
            simInDataArry.RemoveAt(i);
        }

        currentSimInData = simInDataArry.Top();
        // UE_LOG(LogSimGameInstance, Log, TEXT("UDisplayGameInstance.ReceiveSimData = %s %f"),
        //  *currentSimInData->name, currentSimInData->timeStamp);
        currentSimInData->bIsConsumed = 0;
    }
}

void UDisplayGameInstance::SendSimData()
{
    // if(currentSimInData)
    // {
    //     UE_LOG(LogSimGameInstance, Log, TEXT("UDisplayGameInstance.SendSimData = %s %f"),
    //         *currentSimInData->name, currentSimInData->timeStamp);
    // }
    if (!currentSimInData || currentSimInData->bIsConsumed > 0)
    {
        return;
    }
    if (currentSimInData->name == TEXT("INIT"))
    {
        bAllowSync = false;
    }
    else if (currentSimInData->name == TEXT("RESET"))
    {
        bAllowSync = false;
    }
    else if (currentSimInData->name == TEXT("UPDATE"))
    {
        {
            FScopeLock ScopeLock(&displayNetworkManager->displayModule->mutex_Output);
            // Write output data
            if (currentSimOutData.IsValid())
            {
                UE_LOG(LogSimGameInstance, Log, TEXT("UDisplayGameInstance.AddSimData = %s %f"),
                    *currentSimOutData->name, currentSimOutData->timeStamp);
                simOutDataArry.Add(currentSimOutData);
            }
            if (currentSimSensorOutData.IsValid())
            {
                UE_LOG(LogSimGameInstance, Log, TEXT("UDisplayGameInstance.AddSimSensorData = %s %f"),
                    *currentSimSensorOutData->name, currentSimSensorOutData->timeStamp);
                simOutDataArry.Add(currentSimSensorOutData);
            }

            currentSimOutData = nullptr;
            currentSimSensorOutData = nullptr;
        }
        bAllowSync = true;

        if (!asynchronousMode)
        {
            // UE_LOG(LogSimSystem, Log, TEXT("displayNetworkManager resume"));
            displayNetworkManager->resumeThread();
        }
    }
}

void UDisplayGameInstance::SyncSimData()
{
    // if(currentSimInData)
    // {
    //     UE_LOG(LogSimGameInstance, Log, TEXT("UDisplayGameInstance.SyncSimData = %s %f"),
    //         *currentSimInData->name, currentSimInData->timeStamp);
    // }
    if (!currentSimInData || currentSimInData->bIsConsumed > 0)
    {
        return;
    }
    if (currentSimInData->name == TEXT("INIT"))
    {
        // Sim_InitBeginLoadWorld();
    }
    if (currentSimInData->name == TEXT("RESET"))
    {
        Sim_ResetBeginLoadWorld();
    }
    if (currentSimInData->name == TEXT("UPDATE"))
    {
        SimInput(*currentSimInData);
    }
}

std::string UDisplayGameInstance::getAddress()
{
    return ipAddress;
}

std::string UDisplayGameInstance::getName()
{
    return moduleName;
}
                                                                                                                                                                                                                                                  
void UDisplayGameInstance::OnAllClientLevelLoaded()
{
    ULevel* Level = GetWorld()->GetCurrentLevel();
    FString LevelName = Level->GetPathName();
    if (!currentSimInData)
    {
        // Not create simmodule
        return;
    }

    if (currentSimInData->name == TEXT("INIT"))
    {
        FSimOut OutData;
        OutData.name = TEXT("OUTPUT_INIT");
        SimOutput(OutData);
    }
    else if (currentSimInData->name == TEXT("RESET"))
    {
        UE_LOG(LogSimGameInstance, Log, TEXT("Reset After Load World!"));
        // Update hadmap
        // if (hadmapHandle && hadmapHandle->IsMapReady() && hadmapHandle->GetMapMode() == hadmapue4::MapMode::ROUTINGMAP)
        // {
        //     hadmapHandle->UpdateRoutingmap(StaticCast<FSimResetIn*>(currentSimInData.Get())->startLon,
        //         StaticCast<FSimResetIn*>(currentSimInData.Get())->startLat,
        //         StaticCast<FSimResetIn*>(currentSimInData.Get())->startAlt);
        // }
        SimInput(*currentSimInData);
        currentSimInData->bIsConsumed = CONSUMED_MAXTICK + 1;
        UE_LOG(LogSimGameInstance, Log, TEXT("Execute ResetAfterLoadedWorld Over!"));

        displayNetworkManager->resumeThread();
    }
}

void UDisplayGameInstance::SimInput(const FSimData& Data)
{
    GetWorld()->GetAuthGameMode<ADisplayGameModeBase>()->SimInput(Data);
}

void UDisplayGameInstance::SimOutput(const FSimData& _Data)
{
    if (_Data.name == TEXT("OUTPUT_SENSOR"))
    {
        currentSimSensorOutData = MakeShared<FSimSensorUpdateOut>();
        *StaticCast<FSimSensorUpdateOut*>(currentSimSensorOutData.Get()) =
            *StaticCast<const FSimSensorUpdateOut*>(&_Data);
    }
    else if (_Data.name == TEXT("UPDATE"))
    {
        currentSimOutData = MakeShared<FSimUpdateOut>();
        *StaticCast<FSimUpdateOut*>(currentSimOutData.Get()) = *StaticCast<const FSimUpdateOut*>(&_Data);
    }
}

FString UDisplayGameInstance::GetGameConfig(const TCHAR* Section, const TCHAR* Key)
{
    FString sConfig;
    GConfig->GetString(Section, Key, sConfig, GGameIni);
    return sConfig;
}

int64 UDisplayGameInstance::GetEgoIDByGroupName(FString FindGroupName)
{
    if (FindGroupName.IsEmpty())
    {
        int64* FindValue = EgoName_ID_Mapping.Find(ModuleGroupName);
        if (FindValue)
        {
            return *FindValue;
        }
        else if (EgoName_ID_Mapping.Num() > 0)
        {
            return EgoName_ID_Mapping.begin().Value();
        }
        else
        {
            return 0;
        }
    }
    int64* FindValue = EgoName_ID_Mapping.Find(FindGroupName);
    return FindValue ? *FindValue : 0;
}

void UDisplayGameInstance::SetAsynchronousMode(bool _Active)
{
    asynchronousMode = _Active;
}

bool UDisplayGameInstance::RegisterClientToSim(APlayerController* NewPlayer)
{
    if (!NewPlayer || !NewPlayer->GetPlayerState<ADisplayPlayerState>())
    {
        UE_LOG(LogSimSystem, Log, TEXT("Register Client To Sim Failed! Player Is Illegal."));
        return false;
    }
    if (clientConfigArry.Num() < clientNum)
    {
        FClientInfo NewClient;
        NewClient.uniqueNetId = NewPlayer->GetPlayerState<ADisplayPlayerState>()->GetUniqueId();
        NewClient.playerName = NewPlayer->GetPlayerState<ADisplayPlayerState>()->GetPlayerName();
        clientConfigArry.Add(NewClient);

        UE_LOG(LogSimSystem, Log, TEXT("Register Client To Sim Successed! Client Name: %s, Client Num: %d/%d"),
            *(clientConfigArry.Last().playerName), clientConfigArry.Num(), clientNum);

        // Connect is full
        if (clientConfigArry.Num() == clientNum)
        {
            bAllClientsLogin = true;
// Create SimModuleThread to connect coordinator.
#if WITH_EDITOR
            UE_LOG(LogSimSystem, Log, TEXT("Editor Mode"));
#else
            CreateSimModuleThread();
#endif
        }
        return true;
    }
    else
    {
        UE_LOG(LogSimSystem, Warning,
            TEXT("Register Client To Sim Reject! Only Allow %d Client To Connect, Client Name: %s"), clientNum,
            *(NewPlayer->GetPlayerState<ADisplayPlayerState>()->GetPlayerName()));
        return false;
    }
}

bool UDisplayGameInstance::UnregisterClientFromSim(AController* Player)
{
    if (!Player || !Player->GetPlayerState<ADisplayPlayerState>())
    {
        UE_LOG(LogSimSystem, Warning, TEXT("Unregister Client From Sim Failed! Player Is Illegal."));
        return false;
    }

    for (size_t i = 0; i < clientConfigArry.Num(); i++)
    {
        if (Player->GetPlayerState<ADisplayPlayerState>()->GetUniqueId() == clientConfigArry[i].uniqueNetId)
        {
            clientConfigArry.RemoveAt(i);

            //  There is a client offline, shutdown simulator.
            bAllClientsLogin = false;
            ShutdownSimModuleThread();
            return true;
        }
    }

    UE_LOG(LogSimSystem, Warning, TEXT("Unregister Client From Sim Failed! Player Is Not Exist."));
    return false;
}

bool UDisplayGameInstance::CreateSimModuleThread()
{
    // If exist return
    if (displayNetworkManager)
    {
        UE_LOG(LogSimSystem, Log, TEXT("GI: Create SimModule Thread Failed, SimModule Thread Is Exist."));
        return false;
    }
    FString fAddress;
    if (FParse::Value(FCommandLine::Get(), TEXT("-address="), fAddress))    // TODO: check server
    {
        ipAddress = std::string(TCHAR_TO_UTF8(*fAddress));
    }
    else
    {
        UE_LOG(LogSimSystem, Log, TEXT("-address is null."));
    }

    FString fName;
    if (FParse::Value(FCommandLine::Get(), TEXT("-name="), fName))
    {
        moduleName = std::string(TCHAR_TO_UTF8(*fName));
    }
    else
    {
        UE_LOG(LogSimSystem, Log, TEXT("-name is null, use <Display>."));
    }
    displayNetworkManager = MakeShared<DisplayNetworkManager>(this);
    UE_LOG(LogSimSystem, Log, TEXT("GI: Create SimModule Thread Successed."));
    return true;
}

void UDisplayGameInstance::ShutdownSimModuleThread()
{
    if (displayNetworkManager)
    {
        displayNetworkManager->resumeThread();
        displayNetworkManager->shutDown();
        displayNetworkManager.Reset();
        UE_LOG(LogSimSystem, Log, TEXT("GI: Shut down SimModule Thread Successed."));
    }
    else
    {
        UE_LOG(LogSimSystem, Log, TEXT("GI: Shut down SimModule Thread Successed, Thread Not Exist."));
    }
}

void UDisplayGameInstance::Sim_InitBeginLoadWorld()
{
    bInitActionComplete = false;

    check(GetWorld());
    if (!GetWorld()->ServerTravel(mapPath_Lobby, false, false))
    {
        UE_LOG(LogSimGameInstance, Warning, TEXT("Server Travel Level Failed!"));
    }
    bIsAllClientsLoadedWorld = false;
    UE_LOG(LogSimGameInstance, Warning, TEXT("Server Travel Level Good!"));
}

void UDisplayGameInstance::Sim_ResetBeginLoadWorld()
{
    bResetActionComplete = false;

    UE_LOG(LogSimGameInstance, Log, TEXT("Reset Begin Load World!"));

    if (CatalogDataSource)
        CatalogDataSource->ClearData();

    ReadSceneFileAndConfig(*currentSimInData);

    FSimResetIn* ResetIn = StaticCast<FSimResetIn*>(currentSimInData.Get());
    FMapInfo NewMapInfo;
    FString ErrorMessage;
    if (GetMapInfo(ResetIn->mapIndex, ResetIn->mapDataBaseName, NewMapInfo, ErrorMessage))
    {
        ResetIn->mapOriginLon = NewMapInfo.origin_Lon;
        ResetIn->mapOriginLat = NewMapInfo.origin_Lat;
        ResetIn->mapOriginAlt = NewMapInfo.origin_Alt;
        ResetIn->mapPath = NewMapInfo.mapPath;
        ResetIn->mapName = NewMapInfo.mapName;
        ResetIn->decryptFilePath = NewMapInfo.decryptFilePath;
        UE_LOG(LogSimSystem, Log, TEXT("Sim_Reset mapIndex : %d %s origin_Lon: %f origin_Lat: %f"), ResetIn->mapIndex,
            *ResetIn->mapDataBaseName, ResetIn->mapOriginLon, ResetIn->mapOriginLat);
        // currentMapInfo = NewMapInfo;
    }
    else
    {
        ShutdownSimModuleThread();
        FGenericPlatformMisc::RequestExit(false);
        return;
    }

    // ReLoad config from game.ini
    int32 MapModeIndex = 0;
    GConfig->GetBool(TEXT("Mode"), TEXT("bLoadHadmap"), bNeedToLoadHadmap, GGameIni);
    GConfig->GetInt(TEXT("Mode"), TEXT("MapMode"), MapModeIndex, GGameIni);
    hadmapue4::MapMode MapMode = hadmapue4::MapMode::ROUTINGMAP;
    if (MapModeIndex == 0)
    {
        MapMode = hadmapue4::MapMode::ROUTINGMAP;
    }
    else
    {
        MapMode = hadmapue4::MapMode::MAPENGINE;
    }

    // Reset HadMap
    hadmapHandle = SHadmap;
    if(ResetIn->mapIndex == 3){
        hadmapHandle->bRevertedXY = true;
        UE_LOG(LogSimSystem, Log, TEXT("Hadmap reverted XY!"));
    }
    if (bNeedToLoadHadmap)
    {
        UE_LOG(LogSimSystem, Log, TEXT("Loading hadmap data file."));
        if (hadmapHandle)
        {
            if (hadmapHandle->Init(MapMode, ResetIn->mapDataBasePath, ResetIn->mapOriginLon, ResetIn->mapOriginLat,
                    ResetIn->mapOriginAlt, ResetIn->decryptFilePath, ResetIn->egoPath))
            {
                UE_LOG(LogSimSystem, Log, TEXT("Init hadmap success!"));
            }
            else
            {
                UE_LOG(LogSimSystem, Warning, TEXT("Init hadmap failed!"));
                ResetFaildStr = TEXT("Init hadmap failed!");
                ShutdownSimModuleThread();
                FGenericPlatformMisc::RequestExit(false);
                return;
            }
        }
        else
        {
            UE_LOG(LogSimSystem, Warning, TEXT("Init hadmap failed! hadmapHandle is null!"));
            ResetFaildStr = TEXT("Init hadmap failed! hadmapHandle is null!");
            ShutdownSimModuleThread();
            FGenericPlatformMisc::RequestExit(false);
            return;
        }
    }
    else
    {
        UE_LOG(LogSimSystem, Log, TEXT("Dont need load hadmap data file."));
        if (hadmapHandle)
        {
            if (hadmapHandle->Init(
                    ResetIn->mapOriginLon, ResetIn->mapOriginLat, ResetIn->mapOriginAlt,
                     ResetIn->decryptFilePath))
            {
                UE_LOG(LogSimSystem, Log, TEXT("Init hadmap success!"));
            }
            else
            {
                UE_LOG(LogSimSystem, Warning, TEXT("Init hadmap failed!"));
                ResetFaildStr = TEXT("Init hadmap failed!");
                ShutdownSimModuleThread();
                FGenericPlatformMisc::RequestExit(false);
                return;
            }
        }
        else
        {
            UE_LOG(LogSimSystem, Warning, TEXT("Init hadmap failed! hadmapHandle is null!"));
            ResetFaildStr = TEXT("Init hadmap failed! hadmapHandle is null!");
            ShutdownSimModuleThread();
            FGenericPlatformMisc::RequestExit(false);
            return;
        }
    }

    UE_LOG(LogSimGameInstance, Warning, TEXT("Server Travel Level %s !"), *ResetIn->mapPath);

    // Check map asset exist
    if (!FPackageName::DoesPackageExist(ResetIn->mapPath))
    {
        UE_LOG(LogSimSystem, Warning, TEXT("The map resource does not exist in the package file! (MapPath:%s)"),
            *ResetIn->mapPath);
        ResetFaildStr = TEXT("The map resource does not exist in the package file!");
        ShutdownSimModuleThread();
        FGenericPlatformMisc::RequestExit(false);
        return;
    }
    else
    {
        check(GetWorld() && GetWorld()->GetAuthGameMode());
        if (!GetWorld()->ServerTravel(ResetIn->mapPath, false, false))
        {
            UE_LOG(LogSimGameInstance, Warning, TEXT("Server Travel Level Failed!"));
        }
        bIsAllClientsLoadedWorld = false;
    }
}

void UDisplayGameInstance::ReadSceneFileAndConfig(FSimIn& _InData)
{
    // read scene xml
    FSimResetIn* SimResetInPtr = static_cast<FSimResetIn*>(&_InData);
    if (!SimResetInPtr || SimResetInPtr->name != TEXT("RESET"))
    {
        UE_LOG(LogSimGameInstance, Error, TEXT("Input data is illegal!"));
        return;
    }

    SimResetInPtr->egoType = TEXT("transport/Type-1");
    SimResetInPtr->egoName = TEXT("suv");

    sim_msg::Scene scene;
    if (!scene.ParseFromString(SimResetInPtr->sceneBuffer))
    {
        UE_LOG(LogSimGameMode, Warning, TEXT("ParseFromString faild."));
        return;
    }
    // UE_LOG(LogSimGameInstance, Log, TEXT("scenesceneBuffer : %s"), UTF8_TO_TCHAR(scene.DebugString().c_str()));
    SimResetInPtr->mapIndex = getMapIndex(SimResetInPtr->mapDataBaseName);
    SimResetInPtr->ModelPath = UTF8_TO_TCHAR(scene.setting().model3d_pathdir().c_str());
    if (SimResetInPtr->mapIndex == 0 && !FPaths::FileExists(SimResetInPtr->mapDataBasePath))
    {
        const auto& hadbuff = scene.setting().hadmap_data();
        if (!hadbuff.empty())
        {
            TArray<uint8> databuffer((uint8*) hadbuff.data(), hadbuff.size());
            SimResetInPtr->mapDataBasePath = FPaths::ProjectLogDir() + SimResetInPtr->mapDataBaseName;
            if (FPaths::FileExists(SimResetInPtr->mapDataBasePath))
            {
                IFileManager::Get().Delete(*SimResetInPtr->mapDataBasePath);
            }
            FFileHelper::SaveArrayToFile(databuffer, *SimResetInPtr->mapDataBasePath);
            UE_LOG(LogSimGameMode, Warning, TEXT("Saved hadmap to %s."), *SimResetInPtr->mapDataBasePath);
        }
    }
    FString Type = "";
    EgoName_ID_Mapping.Empty(scene.egos().size());
    for (int32 i = 0; i < scene.egos().size(); i++)
    {
        const auto& EgoData = scene.egos(i);
        if (EgoData.initial().common().waypoints().size() == 0)
        {
            continue;
        }
        FEgoInitInfo InitInfo;
        FString ElemGroupName = UTF8_TO_TCHAR(EgoData.group().c_str());
        int64 ElemID = i;    // EgoData.id();
        EgoName_ID_Mapping.Add(ElemGroupName, ElemID);
        UE_LOG(LogSimGameInstance, Log, TEXT("EgoName_ID_Mapping : %s_%ld"), *ElemGroupName, ElemID);

        InitInfo.EgoID = ElemID;
        InitInfo.startLon = EgoData.initial().common().waypoints(0).position().world().x();
        InitInfo.startLat = EgoData.initial().common().waypoints(0).position().world().y();
        InitInfo.startAlt = EgoData.initial().common().waypoints(0).position().world().z();
        InitInfo.startSpeed = EgoData.initial().common().waypoints(0).speed().value();
        InitInfo.startTheta = EgoData.initial().common().waypoints(0).heading_angle().value();

        Type = ANSI_TO_TCHAR(EgoData.physicles(0).common().model_3d().c_str());
        InitInfo.egoName = Type;
        if (!Type.IsEmpty())
        {
            if (Type.Contains(TEXT("mainsuv/sm_mainsuv1.fbx")))
            {
                InitInfo.egoType = TEXT("transport/Type-1");
            }
        }

        SimResetInPtr->EgoInitInfoArry.Add(InitInfo);
    }
    if (SimResetInPtr->EgoInitInfoArry.Num() == 0)
    {
        UE_LOG(LogSimGameInstance, Error, TEXT("sceneBuffer EgoInitInfoArry is null"));
    }
}

int32 UDisplayGameInstance::getMapIndex(const FString& mapname)
{
    UE_LOG(LogSimGameInstance, Log, TEXT("getMapIndex: %s %s"), *mapname, *FPaths::ProjectSavedDir());
    FConfigSection* Sec = GConfig->GetSectionPrivate(TEXT("MapIndex"), false, false, GGameIni);
    if (!Sec)
    {
        return 0;
    }
    for (FConfigSection::TIterator It(*Sec); It; ++It)
    {
        //UE_LOG(LogSimGameInstance, Log, TEXT("getMapIndex key: %s value : %s"), *It.Key().ToString(),
            //*It.Value().GetValue());
        FRegexPattern pattern(It.Key().ToString());
        FRegexMatcher matcher(pattern, mapname);
        if (matcher.FindNext() && !It.Value().GetValue().IsEmpty())
        {
            UE_LOG(LogSimGameInstance, Log, TEXT("Find mapIndex: %s"), *It.Value().GetValue());
            return FCString::Atoi(*It.Value().GetValue());
        }
    }
    return 0;
}

bool UDisplayGameInstance::GetMapInfo(
    int32 MapIndex, const FString& MapFileName, FMapInfo& MapInfo, FString& ErrorMessage)
{
    //MapIndex = getMapIndex(MapFileName);
    if (!GConfig->GetString(TEXT("MapName"), *FString::FromInt(MapIndex), MapInfo.mapName, GGameIni))
    {
        // UScriptStruct* Struct = MapInfo.StaticStruct();
        // FString Output = TEXT("");
        // Struct->ExportText(Output, &MapInfo, nullptr, this, (PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited
        // | PPF_IncludeTransient), nullptr); FString VariableName = StandardizeCase(Property->GetName());
        // Struct->ImportText(*MapInfoStr, &MapInfo, this, (PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited |
        // PPF_IncludeTransient), nullptr, Struct->GetName());
        UE_LOG(LogTemp, Error, TEXT("Can't find map!(MapIndex: %d, MapFileName: %s)"), MapIndex, *MapFileName);
        ErrorMessage = TEXT("Can't find map!");
        return false;
    }

    /* MapPath */
    if (!GConfig->GetString(TEXT("MapPath"), *MapInfo.mapName, MapInfo.mapPath, GGameIni))
    {
        UE_LOG(LogTemp, Error,
            TEXT(
                "Can't find map path! The path of the map is not defined!(MapIndex: %d, MapFileName: %s, MapName: %s)"),
            MapIndex, *MapFileName, *MapInfo.mapName);
        ErrorMessage = TEXT("Can't find map path! The path of the map is not defined!");
        return false;
    }

    /* MapOrigin */
    FJsonSerializableArray ValueArry;
    if (!GConfig->GetSingleLineArray(TEXT("MapOrigin"), *MapInfo.mapPath, ValueArry, GGameIni))
    {
        UE_LOG(LogTemp, Error,
            TEXT("Can't find map origin! The origin of the map is not defined!(MapIndex: %d, MapFileName: %s, MapName: "
                 "%s)"),
            MapIndex, *MapFileName, *MapInfo.mapName);
        ErrorMessage = TEXT("Can't find map origin! The origin of the map is not defined!");
        return false;
    }
    if (ValueArry.Num() == 3)
    {
        MapInfo.origin_Lon = FCString::Atod(*ValueArry[0]);
        MapInfo.origin_Lat = FCString::Atod(*ValueArry[1]);
        MapInfo.origin_Alt = FCString::Atod(*ValueArry[2]);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Map origin is illegal!(MapIndex: %d, MapFileName: %s, MapName: %s)"), MapIndex,
            *MapFileName, *MapInfo.mapName);
        ErrorMessage = TEXT("Map origin is illegal!");
        return false;
    }

    /* MapDecrypt */
    if (GConfig->GetString(TEXT("MapDecrypt"), *MapFileName, MapInfo.decryptFilePath, GGameIni))
    {
        if (!MapInfo.decryptFilePath.IsEmpty())
        {
            // Absolute decrypt file path
            MapInfo.decryptFilePath = FPaths::ProjectDir() + MapInfo.decryptFilePath;
        }
    }

    return true;
}
