#pragma once

#include "CoreMinimal.h"
#include "Managers/Manager.h"
#include "Objects/Sensors/CameraSensors/CameraSensor.h"
#include "Objects/Sensors/LidarSensors/TLidarSensor.h"
#include "Objects/Sensors/CameraSensors/FisheyeSensor.h"
#include "SensorManager.generated.h"


USTRUCT()
struct FSensorManagerConfig : public FManagerConfig
{
    GENERATED_BODY();

public:
    TArray<FCameraConfig> cameraArry;
    TArray<FCameraConfig> semanticArry;
    TArray<FCameraConfig> normalArry;
    TArray<FCameraConfig> depthArry;
    TArray<FFisheyeConfig> fisheyeArry;
    TArray<FLidarConfig> lidarArry;
};

USTRUCT()
struct FSensorManagerIn : public FManagerIn
{
    GENERATED_BODY();
};

USTRUCT()
struct FSensorManagerOut : public FManagerOut
{
    GENERATED_BODY();

public:
    TArray<FSensorOutput> outArray;
};


UCLASS()
class DISPLAY_API ASensorManager : public AManager
{
    GENERATED_BODY()
public:
    ASensorManager();

    virtual void Init(const FManagerConfig& Config);

    virtual void Update(const FManagerIn& Input, FManagerOut& Output);

    static FSensorManagerConfig ParseSensorString(const std::string& buffer, int64 EgoId);

    static void CoordinateTransform_RightHandToLeftHand(FVector& _Location, FRotator& _Rotation);

protected:
    TMap<ISimActorInterface*, TMap<FString, ISensorInterface*>> sensorMap;

};