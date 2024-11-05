#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SensorInterface.h"
#include "SensorActor.generated.h"

UCLASS()
class DISPLAY_API ASensorActor : public AActor, public ISensorInterface
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    ASensorActor();

    virtual bool Init(const FSensorConfig& Config);

    virtual void Update(const FSensorInput& Input, FSensorOutput& Output);

    virtual void Destroy(FString Reason);

    virtual ISimActorInterface* Install(const FSensorConfig& _Config);

    class UDisplayGameInstance* GetDisplayInstance();

protected:
    AActor* InstalledActor = NULL;

};