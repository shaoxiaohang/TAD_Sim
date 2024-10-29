#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Objects/Transports/TransportInterface.h"
#include "VehicleInterface.generated.h"

USTRUCT()
struct FVehicleConfig : public FTransportConfig
{
    GENERATED_BODY();

    UPROPERTY()
    TArray<FVector> wheelLocationArry;

    UPROPERTY()
    TArray<FRotator> wheelRotationArry;
};

USTRUCT()
struct FVehicleIn : public FTransportIn
{
    GENERATED_BODY();

    UPROPERTY()
    bool hasContainer = false;

    UPROPERTY()
    FVector locContainer = FVector(0);

    UPROPERTY()
    FRotator rotContainer = FRotator(0);

    UPROPERTY()
    double speedContainer = 0;

    UPROPERTY()
    TArray<FVector> wheelLocationArry;

    UPROPERTY()
    TArray<FRotator> wheelRotationArry;

    // driving simulator
    UPROPERTY()
    int8 drivingMode = 0;

    UPROPERTY()
    double steeringAngle = 0.f;

    UPROPERTY()
    double throttle = 0.f;

    UPROPERTY()
    double brake = 0.f;

    UPROPERTY()
    int8 gear = 0;

    UPROPERTY()
    int8 status = 0;
};

USTRUCT()
struct FVehicleOut : public FTransportOut
{
    GENERATED_BODY();

    UPROPERTY()
    TArray<FVector> wheelImpactPointArry;

    UPROPERTY()
    TArray<double> wheelImpactTanXArry;

    UPROPERTY()
    TArray<double> wheelImpactTanYArry;

    UPROPERTY()
    double friction;

    // driving simulator
    UPROPERTY()
    int8 drivingMode = 0;

    UPROPERTY()
    FVector locationManned;

    UPROPERTY()
    FRotator rotatorManned;

    UPROPERTY()
    FVector velocityManned;

    UPROPERTY()
    FVector acceleration;

    UPROPERTY()
    double speed = 0.f;

    UPROPERTY()
    int16 rmp = 0;

    UPROPERTY()
    bool bHasMannedControlData = false;

    UPROPERTY()
    FVector locPose;
    UPROPERTY()
    FRotator rotPose;
    UPROPERTY()
    bool bHasPose = false;
};

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UVehicleInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 *
 */
class DISPLAY_API IVehicleInterface
{
    GENERATED_BODY()

    // Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

};