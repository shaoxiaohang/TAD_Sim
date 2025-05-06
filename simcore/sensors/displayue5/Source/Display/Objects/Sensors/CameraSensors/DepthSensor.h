#pragma once

#include "CoreMinimal.h"
#include "CameraSensor.h"
#include "DepthSensor.generated.h"

/**
 *
 */
UCLASS()
class DISPLAY_API ADepthSensor : public ACameraSensor
{
    GENERATED_BODY()
public:
    ADepthSensor();
    ~ADepthSensor();

    virtual bool Init(const FSensorConfig& _Config);
    
    virtual void Update(const FSensorInput& _Input, FSensorOutput& _Output) override;

};
