#include "DisplayPlayerController.h"
#include "Managers/TransportManager.h"
#include "DisplayGameStateBase.h"


DEFINE_LOG_CATEGORY_STATIC(SimLogPlayerController, Log, All);

ADisplayPlayerController::ADisplayPlayerController()
{
    // Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
    bShowMouseCursor = bShowMouseConfig;
}

void ADisplayPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    check(InputComponent);
}

void ADisplayPlayerController::SwitchPawnToEgo()
{
    TWeakObjectPtr<ATransportManager> TM =
        GetWorld()->GetGameState<ADisplayGameStateBase>()->syncSystem->transportManager;
    if (TM.IsValid() && TM->vehicleManager)
    {
        ISimActorInterface* SimActor = TM->vehicleManager->GetVehicle(ETrafficType::ST_Ego, 0);
        if (SimActor)
        {
            AVehiclePawn* EgoPawn = Cast<AVehiclePawn>(SimActor);
            if (EgoPawn)
            {
                this->Possess(EgoPawn);
                EgoPawn->SwitchCamera(EgoPawn->GetDefaultCameraName());
                return;
            }
        }
    }
    UE_LOG(SimLogPlayerController, Warning, TEXT("Switch Pawn To Ego Failed!"));
}

void ADisplayPlayerController::SwitchPawnToGhost()
{

}