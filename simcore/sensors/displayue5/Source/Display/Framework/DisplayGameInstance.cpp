#include "DisplayGameInstance.h"
#include "DisplayPlayerState.h"
#include "DisplayNetworkManager.h"
#include "SaveDataThread.h"
#include "Data/CatalogDataSource.h"
#include "LoaderBPFunctionLibrary.h"
#include "Kismet/KismetInternationalizationLibrary.h"

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

bool UDisplayGameInstance::Tick(float DeltaSeconds)
{
    // check clients all connected
    if (!bAllClientsLogin)
    {
        return true;
    }
    if (syncOneFrame)    // sigle frame
    {
        OutputData();    // SensorManger update
        //SendSimData();
        ReceiveSimData();    //
        //SyncSimData();       // location update.
    }
    else
    {
        OutputData();        // SensorManger update
        ReceiveSimData();    // read location, begin of step and waiting for step
        SyncSimData();       // location update.
        SendSimData();       // public sensor
    }
    return true;
}

void UDisplayGameInstance::OutputData()
{
    if (!currentSimInData || currentSimInData->bIsConsumed > 0)
    {
        return;
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
            //UE_LOG(LogSimSystem, Log, TEXT("ReceiveSimData waiting"));
            threadSuspendedEvent->Wait(waitTime);
        }
        else
        {
            if (currentSimInData->name == TEXT("UPDATE"))
            {
                // UE_LOG(LogSimSystem, Log, TEXT("ReceiveSimData waiting"));
                threadSuspendedEvent->Wait(waitTime);
            }
            else if (currentSimInData->name == TEXT("RESET"))
            {
                if (currentSimInData->bIsConsumed > CONSUMED_MAXTICK)
                {
                    // UE_LOG(LogSimSystem, Log, TEXT("ReceiveSimData waiting"));
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
        currentSimInData->bIsConsumed = 0;
    }
}

void UDisplayGameInstance::SendSimData()
{
    if (!currentSimInData || currentSimInData->bIsConsumed > 0)
    {
        return;
    }
}

void UDisplayGameInstance::SyncSimData()
{
    if (!currentSimInData || currentSimInData->bIsConsumed > 0)
    {
        return;
    }
    if (currentSimInData->name == TEXT("INIT"))
    {
        Sim_InitBeginLoadWorld();
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

}

FString UDisplayGameInstance::GetGameConfig(const TCHAR* Section, const TCHAR* Key)
{
    FString sConfig;
    GConfig->GetString(Section, Key, sConfig, GGameIni);
    return sConfig;
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
            CreateSimModuleThread();
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