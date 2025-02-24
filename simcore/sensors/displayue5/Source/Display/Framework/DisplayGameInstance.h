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
#include "egoMapData.pb.h"
#include "union.pb.h"
#include "scene.pb.h"
#ifdef _MSC_VER
#include "HideWindowsPlatformTypes.h"
#endif

#include "SimInterface.h"
#include "Containers/Ticker.h"

#include "DisplayGameInstance.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogSimSystem, Log, All)
DECLARE_LOG_CATEGORY_EXTERN(LogSimDebug, Log, All)

namespace hadmapue4
{
class HadmapManager;
}

class DisplayNetworkManager;
class SaveDataThread;

struct FLocalInitIn;
struct FLocalInitOut;

struct FLocalResetIn;
struct FLocalResetOut;

struct FLocalUpdateIn;
struct FLocalUpdateOut;

struct FSensorManagerConfig;



USTRUCT()
struct FMapInfo
{
    GENERATED_USTRUCT_BODY()
public:
    UPROPERTY()
    FString mapPath;
    UPROPERTY()
    FString mapName;
    UPROPERTY()
    double origin_Lon = 0;
    UPROPERTY()
    double origin_Lat = 0;
    UPROPERTY()
    double origin_Alt = 0;
    UPROPERTY()
    FString decryptFilePath;

    bool IsLegal() const
    {
        return true;
        // IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        // if (!PlatformFile.FileExists(*mapPath))
        // FString MapPath;
        // GConfig->GetString(TEXT("MapList"), *mapFileName, MapPath, GGameIni);
    }
};

struct FEgoInitInfo
{
    FString egoName = TEXT("suv");
    FString egoCategory = TEXT("car");
    FString egoType = "transport/Ego";
    int64 EgoID;
    double startLon;
    double startLat;
    double startAlt;
    double startTheta;
    double startSpeed;
};

USTRUCT()
struct FClientInfo
{
    GENERATED_BODY()
public:
    FUniqueNetIdRepl uniqueNetId;
    FString playerName;
    bool bIsSimActionComplete = false;
    bool bIsDeferredSimActionComplete = false;
    FString simStat = TEXT("DATA_SENT");
    ESimState state = ESimState::SA_DONE;
};

struct FSimIn : public FSimData
{
public:
    virtual ~FSimIn()
    {
    }
};

struct FSimOut : public FSimData
{
public:
    bool bIsSent = false;
};

struct FSimInitIn : public FSimIn
{
public:
    int32 clientNum = 1;
};

struct FSimInitOut : public FSimOut
{
public:
    virtual ~FSimInitOut()
    {
    }

    FString message;
    std::vector<std::string> sensor_topic;
};


struct FSimResetIn : public FSimIn
{
public:
    FString tadsimPath;
    FString configFilePath;
    double mapOriginLon = 0.f;
    double mapOriginLat = 0.f;
    double mapOriginAlt = 0.f;

    TArray<FEgoInitInfo> EgoInitInfoArry;
    FString egoName = TEXT("suv");
    FString egoCategory = TEXT("car");
    FString egoType = "transport/Ego";
    double startLon;
    double startLat;
    double startAlt;
    double startTheta;
    double startSpeed;
    double endLon;
    double endLat;
    double endAlt;
    TArray<FVector> egoPath;

    int32 mapIndex;
    FString mapDataBaseName;
    FString mapDataBasePath;

    FString sensorConfigPath;
    FString envConfigPath;

    FString mapName;
    FString mapPath;
    FString decryptFilePath;
    FString ModelPath;

    std::string sceneBuffer;

    FString SceneTrafficPath;
};

struct FSimResetOut : public FSimOut
{
public:
    virtual ~FSimResetOut()
    {
    }
    FString message;
};

struct FSimUpdateIn : public FSimIn
{
public:
    virtual ~FSimUpdateIn()
    {
    }

    int32 frameID = 0;
    TMap<FString, sim_msg::Location> egoData;
    TMap<FString, sim_msg::Location> egoContainerData;
    sim_msg::Location overrideEgoLocation;
    sim_msg::Traffic trafficData;
    sim_msg::Trajectory trajectoryData;
    sim_msg::PlanOutput planOutputData;
    sim_msg::ControlSim controlSimData;
    sim_msg::PlanStatus planStatusData;
    sim_msg::EnvironmentalConditions environmentData;
};

struct FSimUpdateOut : public FSimOut
{
public:
    FSimUpdateOut()
    {
        datatype = 1;
    }
    virtual ~FSimUpdateOut()
    {
    }

    int32 frameID = 0;
    sim_msg::Location egoData;
    sim_msg::Traffic trafficData;
    sim_msg::DisplayPose trafficPose;
    sim_msg::EgoMapData egoMapData;
    
    std::string topic_egoData = "LOCATION";
    std::string topic_trafficData = "TRAFFIC";
};

struct FSimSensorUpdateOut : public FSimOut
{
    // GENERATED_BODY()
public:
    FSimSensorUpdateOut()
    {
        datatype = 2;
    }
    virtual ~FSimSensorUpdateOut()
    {
    }

    // 具体传感器的序列化改为传感器内部进行
    sim_msg::SensorRaw sensorData;
};

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

    void SimOutput(const FSimData& _Data);

    void OnAllClientLevelLoaded();

    /**
     * Sim interface
     * Trigger simulator action to GameInstance from SimModule.
     */
    virtual void SimInput(const FSimData& _Data);

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

    SaveDataThread* GetSaveDataHandle() const;

    FString GetGameConfig(const TCHAR* Section, const TCHAR* Key);

    void SetAsynchronousMode(bool _Active);

    // if EgoGrouName is null, then find current possessed ego
    int64 GetEgoIDByGroupName(FString EgoGroupName = TEXT(""));

    void SendSimData();
    void ReceiveSimData();
    void SyncSimData();
    void OutputData();

    // store simdata
    TArray<TSharedPtr<FSimIn>> simInDataArry;
    TArray<TSharedPtr<FSimOut>> simOutDataArry;
    TSharedPtr<FSimIn> currentSimInData;
    TSharedPtr<FSimOut> currentSimOutData;
    TSharedPtr<FSimOut> currentSimSensorOutData;
    // update data flag
    bool bSimInDataRefreshed = false;

    FEvent* threadSuspendedEvent;

    bool bAllClientsLogin = false;

    FString ResetFaildStr;

    FString ModuleGroupName;

    bool bIsFrameSync = false;

    bool bAllowSync;

    FVector2D nHILpos{-1, -1};

protected:

    UPROPERTY()
    class UCatalogDataSource* CatalogDataSource;

    UPROPERTY()
    class URuntimeMeshLoader* RuntimeMeshLoader;

private:

    // Init from simulator
    void Sim_InitBeginLoadWorld();

    // Reset from simulator
    void Sim_ResetBeginLoadWorld();

    void ReadSceneFileAndConfig(FSimIn& _InData);

    int32 getMapIndex(const FString& mapname);

    bool GetMapInfo(int32 MapIndex, const FString& MapFileName, FMapInfo& MapInfo, FString& ErrorMessage);

private:

    int32 clientNum = 1;

    std::string ipAddress;

    std::string moduleName = "Display";

    std::string modeName = "FrameAsync";

    bool sendVilMsg = false;

    TMap<FString, int64> EgoName_ID_Mapping;

    // Init complete flag
    bool bInitActionComplete = false;
    // Reset complete flag
    bool bResetActionComplete = false;
    // Is all clients loaded world.
    bool bIsAllClientsLoadedWorld = false;
    
    bool asynchronousMode = true;
    bool syncOneFrame = false;
    bool NeedExit = false;
    int SyncModeWait = 0;
    FString mapPath_Lobby = TEXT("/Game/Basic");

    // Load hadmap flag
    bool bNeedToLoadHadmap = false;

    // Module Handle
    hadmapue4::HadmapManager* hadmapHandle = NULL;

    // All client configuration, include uniqueNetId, sensor config and so on.
    TArray<FClientInfo> clientConfigArry;

    TSharedPtr<DisplayNetworkManager> displayNetworkManager;

    FTSTicker::FDelegateHandle TickDelegateHandle;

    // save data thread
    TSharedPtr<SaveDataThread> savedataThreadHandle;

};