#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimActorInterface.h"
#include "Engine/World.h"
#include "SimActorFactory.generated.h"

class ISimActorInterface;
class UWorld;
struct FSimActorConfig;

UCLASS()
class DISPLAY_API ASimActorFactory : public AActor
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    ASimActorFactory();

public:
    // Spawn SimActor and init it.
    template <class T = AActor>
    static T* SpawnSimActor(UWorld* _World, UClass* _Class, const FSimActorConfig& _InitParam)
    {
        if (!_World)
        {
            return NULL;
        }
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        T* TActor = _World->SpawnActor<T>(_Class, Params);
        if (!TActor)
        {
            return NULL;
        }
        ISimActorInterface* SimActor = Cast<ISimActorInterface>(TActor);
        if (!SimActor)
        {
            return NULL;
        }
        SimActor->Init(_InitParam);
        UE_LOG(LogTemp, Display, TEXT("SimActor Spawn!  Name %s TypeName %s "), 
          *_InitParam.Name,*_InitParam.typeName );
        return TActor;
    }

};