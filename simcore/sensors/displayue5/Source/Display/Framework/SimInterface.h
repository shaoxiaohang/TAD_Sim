#pragma once

#include "CoreMinimal.h"
#include <string>
#include "UObject/Interface.h"

#include "SimInterface.generated.h"

UENUM(BlueprintType)
enum class ESimState : uint8
{
    SA_DONE,
    SA_INIT,
    SA_RESET,
    SA_UPDATE
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


struct FSimData
{
public:
    virtual ~FSimData()
    {
    }
    int datatype = 0;

    double timeStamp = 0.f;
    ESimState state = ESimState::SA_DONE;
    FString name;
    int bIsConsumed = 0;
    double timeStamp_ego = 0.f;
    double timeStamp_tail = 0.f;
};

struct FSimIn : public FSimData
{
public:
    virtual ~FSimIn()
    {
    }
};

struct FSimInitIn : public FSimIn
{
public:
    int32 clientNum = 1;
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