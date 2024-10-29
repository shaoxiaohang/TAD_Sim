#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Display/Objects/SimActorInterface.h"
#include "TransportInterface.generated.h"


UENUM(BlueprintType)
enum class EDrivingMode : uint8
{
    DM_AUTOPILOT,
    DM_MANNED,
    DM_MANNED_VR
};

UENUM()
enum class ETrafficType : uint8
{
    ST_TRAFFIC,
    ST_Ego,
    ST_NONE
};

USTRUCT()
struct FTrajectoryPoint
{
    GENERATED_BODY()
public:
    UPROPERTY()
    FVector location = FVector(0);
    UPROPERTY()
    FRotator rotator = FRotator(0);
    UPROPERTY()
    double kappa = 0.0;
    UPROPERTY()
    double speed = 0.0;
    UPROPERTY()
    double accel = 0.0;
    UPROPERTY()
    double timestamp = 0.0;
    UPROPERTY()
    double distance = 0.0;
};

USTRUCT()
struct FTrajectory
{
    GENERATED_BODY()
public:
    UPROPERTY()
    TArray<FTrajectoryPoint> data;
};

USTRUCT()
struct FTransportConfig : public FSimActorConfig
{
    GENERATED_BODY()
public:
    UPROPERTY()
    ETrafficType trafficType = ETrafficType::ST_TRAFFIC;
    UPROPERTY()
    bool isEgoSnap = true;
    UPROPERTY()
    UStaticMesh* CustomMesh = nullptr;
};

USTRUCT()
struct FTransportIn : public FSimActorInput
{
    GENERATED_BODY()
public:
    UPROPERTY()
    ETrafficType trafficType = ETrafficType::ST_TRAFFIC;
    FTrajectory trajectory;
};

USTRUCT()
struct FTransportOut : public FSimActorOutput
{
    GENERATED_BODY()
public:
    UPROPERTY()
    ETrafficType trafficType = ETrafficType::ST_TRAFFIC;
};

UINTERFACE(MinimalAPI)
class UTransportInterface : public UInterface
{
    GENERATED_BODY()
};

class DISPLAY_API ITransportInterface
{
    GENERATED_BODY()

};