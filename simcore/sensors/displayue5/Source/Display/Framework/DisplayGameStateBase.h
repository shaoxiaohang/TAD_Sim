#pragma once

#include "CoreMinimal.h"

#include "GameFramework/GameStateBase.h"
#include "Framework/DisplayGameModeBase.h"
#include "Managers/SensorManager.h"
#include "Managers/TransportManager.h"

#include "DisplayGameStateBase.generated.h"


UCLASS()
class DISPLAY_API ASyncSystem : public AActor
{
    GENERATED_BODY()

public:
    FLocalUpdateOut SyncSimActors(const FLocalData& _Data);

public:
    // TransportManager
    TWeakObjectPtr<ATransportManager> transportManager = NULL;

    // SensorManager
    ASensorManager* SensorManager = NULL;

protected:
    /* Spawn all private managers */
    bool SpawnAndInitAllManagers();

    FLocalUpdateOut UpdateAllManagers(const FLocalUpdateIn& _In);

    void OnAllManagersInit();


protected:
    FLocalResetIn resetIn;
    FLocalUpdateIn updateIn;
    FLocalUpdateOut updateOut;
};

UCLASS()
class DISPLAY_API ADisplayGameStateBase : public AGameStateBase
{
    GENERATED_BODY()
public:
    /** Constructor for AActor that takes an ObjectInitializer for backward compatibility */
    ADisplayGameStateBase(const FObjectInitializer& ObjectInitializer);

public:
    /* Interface */

    // Gameinstance input simdata to gamestat
    virtual void SimInput(const FLocalData& _Data);

public:

    ASyncSystem* syncSystem = NULL;

protected:
    /**
     * Called by GameMode, run reset event.
     * NetMulticast function, create all public managers on server and client.
     */
    UFUNCTION(NetMulticast, Reliable, WithValidation)
    virtual void Multicast_Reset(FLocalResetIn _ResetData);

    /**
     * Called by GameMode, run update event.
     * NetMulticast function, update all public managers on server and client.
     */
    UFUNCTION(NetMulticast, Reliable, WithValidation)
    virtual void Multicast_Update(FLocalUpdateIn _UpdateData);

protected:
    FLocalResetIn resetIn;
    FLocalUpdateIn updateIn;
    FLocalUpdateOut updateOut;

};