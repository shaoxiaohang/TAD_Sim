#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "DisplayPlayerController.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnEgoViewChange, const FName& CameraName);

UCLASS()
class DISPLAY_API ADisplayPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    ADisplayPlayerController();

    // Switch possess to ego vehicle
    void SwitchPawnToEgo();
    // Switch possess to ghost pawn
    void SwitchPawnToGhost();


    UFUNCTION(Server, Reliable, WithValidation)
    virtual void Server_SimUpdateOutput(FLocalUpdateOut _OutData);

protected:

    /** Allows the PlayerController to set up custom input bindings. */
    virtual void SetupInputComponent();

private:
    UPROPERTY(Config)
    bool bShowMouseConfig = true;

public:
    UPROPERTY(Config)
    int32 id_controlled = 0;

    FOnEgoViewChange OnEgoViewChange;
};