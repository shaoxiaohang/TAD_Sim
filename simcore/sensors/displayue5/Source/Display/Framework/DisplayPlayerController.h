// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SimInterface.h"
#include "Framework/DisplayGameModeBase.h"
#include "Framework/DisplayGameStateBase.h"
#include "DisplayPlayerController.generated.h"

class UOutlineWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnEgoViewChange, const FName& CameraName);

/**
 *
 */
UCLASS()
class DISPLAY_API ADisplayPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    ADisplayPlayerController();

public:
    FLocalResetIn resetIn;

    FLocalResetOut resetOut;

    FLocalUpdateIn updateIn;

    FLocalUpdateOut updateOut;

    UFUNCTION()
    void OnSimResetInput();

    UFUNCTION()
    void OnSimUpdateInput();

    virtual FString ConsoleCommand(const FString& Command, bool bWriteToLog = true);

public:

    // SensorManager
    ASensorManager* SensorManager = NULL;

    UUserWidget* SwitchEgoWidget = nullptr;
    class UDrivingWidget* driving_widget = nullptr;

    class UDrivingWidget* GetDrivingWidget();

    UUserWidget* WanderInfoWidget = nullptr;

public:

    UFUNCTION(Server, Reliable, WithValidation)
    virtual void Server_SimResetOutput(FLocalResetOut _ReturnData);

    UFUNCTION(Server, Reliable, WithValidation)
    virtual void Server_SimUpdateOutput(FLocalUpdateOut _OutData);

    /** Overridable function called whenever this actor is being removed from a level */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

public:
    // Called every frame
    virtual void Tick(float DeltaTime) override;

    // Wrapper OnPossess function
    void PossessActor(AActor* _ActorToPossess);

    UFUNCTION(BlueprintCallable)
    void SetPossessEgoName(const FString& VehicleName);

    UFUNCTION(Exec)
    void PossessVehicleExec(int64 _Id);

    // UFUNCTION(Exec)
    // void DebugGate(bool bEnable);

protected:
    /** Overridable native event for when play begins for this actor. */
    virtual void BeginPlay();

    /** Allows the PlayerController to set up custom input bindings. */
    virtual void SetupInputComponent();

private:
    UPROPERTY(Config)
    bool bShowMouseConfig = true;

    UPROPERTY(Config)
    bool bEnableGhost = false;

    UPROPERTY(Config)
    bool bEnableGod = false;

    class ADisplayPawn* ghostPawn = NULL;
    class AGodPawn* godPawn = NULL;

    bool bEnableConsole = false;

public:
    void ToggleUIVisibility();

    // Switch possess to ego vehicle
    void SwitchPawnToEgo();
    // Switch possess to ghost pawn
    void SwitchPawnToGhost();
    // Switch possess to god pawn
    void SwitchPawnToGod();

    void SwitchPawnToEgoOrGhost();

public:
    void ToggleDriveMode();
    int32 GetDrivingMode() const
    {
        return modeDrive;
    }
    void SetDrivingMode(int32 _Mode)
    {
        modeDrive = _Mode;
    }

protected:
    UPROPERTY(Config)
    int32 modeDrive = 0;

    UPROPERTY(Config)
    bool bAllowSwitchDriveMode = 0;

public:
    UPROPERTY(Config)
    int32 id_controlled = 0;

    FOnEgoViewChange OnEgoViewChange;
};
