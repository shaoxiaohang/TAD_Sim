#pragma once

#include "CoreMinimal.h"
#include "Managers/Manager.h"
#include "Objects/Transports/Vehicle/VehicleInterface.h"
#include "VehicleManager.generated.h"


class AVehiclePawn;

USTRUCT()
struct FVehicleManagerConfig : public FManagerConfig
{
    GENERATED_BODY()
public:
    UPROPERTY()
    TArray<FVehicleConfig> egoConfigArry;

    UPROPERTY()
    TArray<FVehicleConfig> trafficConfigArry;
};

USTRUCT()
struct FVehicleManagerIn : public FManagerIn
{
    GENERATED_BODY()
public:
    UPROPERTY()
    double timeStamp = 0.f;

    UPROPERTY()
    TArray<FVehicleIn> egoVehicleInputArry;

    UPROPERTY()
    TArray<FVehicleIn> trafficVehicleInputArry;
};

USTRUCT()
struct FVehicleManagerOut : public FManagerOut
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FVehicleOut> trafficOutArry;

    UPROPERTY()
    TArray<FVehicleOut> egoOutArry;
};

UCLASS()
class DISPLAY_API AVehicleManager : public AManager
{
    GENERATED_BODY()
public:
    AVehicleManager();

    TArray<ISimActorInterface*> egoArry;

    TArray<ISimActorInterface*> trafficArry;


public:
    virtual void Init(const FManagerConfig& Config);

    virtual void Update(const FManagerIn& Input, FManagerOut& Output);

public:

    ISimActorInterface* GetVehicle(ETrafficType _Type, int64 _Id);


    class UDisplayGameInstance* GetGameInstance();

protected:

    TSubclassOf<AVehiclePawn> vehicleClass;

    UPROPERTY()
    TMap<FString, TSubclassOf<AVehiclePawn> > vehicleClassMap;

};