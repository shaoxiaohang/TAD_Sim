#pragma once

#include "CoreMinimal.h"
#include "Managers/Manager.h"
#include "SensorManager.generated.h"


USTRUCT()
struct FSensorManagerConfig : public FManagerConfig
{
    GENERATED_BODY();

public:
    TArray<FCameraConfig> cameraArry;
    // TArray<FCameraConfig> semanticArry;
    // TArray<FLidarConfig> lidarArry;
    // TArray<FUltrasonicConfig> ultrasonicArry;
    // TArray<FCameraConfig> depthArry;
    // TArray<FFisheyeConfig> fisheyeArry;
    // TArray<FCameraConfig> ringArry;
};


UCLASS()
class DISPLAY_API ASensorManager : public AManager
{
    GENERATED_BODY()
public:
    ASensorManager();

    virtual void Init(const FManagerConfig& Config);

    static FSensorManagerConfig ParseSensorString(const std::string& buffer, int64 EgoId);


    static void CoordinateTransform_RightHandToLeftHand(FVector& _Location, FRotator& _Rotation);

};