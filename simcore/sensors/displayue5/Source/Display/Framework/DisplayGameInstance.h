#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include <string>


#ifdef _MSC_VER
#include "AllowWindowsPlatformTypes.h"
#endif
// Protobuf
#include "traffic.pb.h"
#include "location.pb.h"
#include "planOutput.pb.h"
#include "trajectory.pb.h"
#include "grading.pb.h"
#include "controlSim.pb.h"
#include "planStatus.pb.h"
#include "sensor_raw.pb.h"
#include "environment.pb.h"
#include "union.pb.h"
#ifdef _MSC_VER
#include "HideWindowsPlatformTypes.h"
#endif

#include "SimInterface.h"
#include "Containers/Ticker.h"

#include "DisplayGameInstance.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogSimSystem, Log, All)
DECLARE_LOG_CATEGORY_EXTERN(LogSimDebug, Log, All)

class DisplayNetworkManager;


UCLASS(config = Game)
class DISPLAY_API UDisplayGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    UDisplayGameInstance(const FObjectInitializer& ObjectInitializer);

    virtual void Init() override;

    bool Tick(float DeltaSeconds);

    std::string getAddress();

    std::string getName();

    // Register client to simulator when client is login.
    bool RegisterClientToSim(APlayerController* NewPlayer);

    // Unregister client to simulator when client is login.
    bool UnregisterClientFromSim(AController* Player);

    // Create thread to communicate with coordinator
    bool CreateSimModuleThread();

    // Shutdown thread that communicate with coordinator
    void ShutdownSimModuleThread();

    void OnAllClientLevelLoaded();

    ///**
    // * Sim interface
    // * Receive action`s result to GameInstance from GameMode.
    // */
    // virtual void SimOutput(const FLocalData& _Output);

    // Get client config array
    FORCEINLINE TArray<FClientInfo> GetAllClientConfig() const
    {
        return clientConfigArry;
    };

    class UCatalogDataSource* GetCatalogDataSource()
    {
        return CatalogDataSource;
    }

    class URuntimeMeshLoader* GetRuntimeMeshLoader()
    {
        return RuntimeMeshLoader;
    }

    FString GetGameConfig(const TCHAR* Section, const TCHAR* Key);

    void SetAsynchronousMode(bool _Active);

    void SendSimData();
    void ReceiveSimData();
    void SyncSimData();
    void OutputData();

    // store simdata
    TArray<TSharedPtr<FSimIn>> simInDataArry;
    TSharedPtr<FSimIn> currentSimInData;
    // update data flag
    bool bSimInDataRefreshed = false;

    FEvent* threadSuspendedEvent;

    bool bAllClientsLogin = false;

    FString ResetFaildStr;

    FString ModuleGroupName;

protected:

    UPROPERTY()
    class UCatalogDataSource* CatalogDataSource;

    UPROPERTY()
    class URuntimeMeshLoader* RuntimeMeshLoader;

private:

    // Init from simulator
    void Sim_InitBeginLoadWorld();

private:

    int32 clientNum = 1;

    std::string ipAddress;

    std::string moduleName = "Display";

    std::string modeName = "FrameAsync";
    bool sendVilMsg = false;
    FVector2D nHILpos{-1, -1};

    // Init complete flag
    bool bInitActionComplete = false;
    // Is all clients loaded world.
    bool bIsAllClientsLoadedWorld = false;
    
    bool bIsFrameSync = false;
    bool asynchronousMode = true;
    bool syncOneFrame = false;
    bool NeedExit = false;
    int SyncModeWait = 0;
    FString mapPath_Lobby = TEXT("/Game/Basic");

    // All client configuration, include uniqueNetId, sensor config and so on.
    TArray<FClientInfo> clientConfigArry;

    TSharedPtr<DisplayNetworkManager> displayNetworkManager;

    FTSTicker::FDelegateHandle TickDelegateHandle;

    // save data thread
    TSharedPtr<class SaveDataThread> savedataThreadHandle;

};