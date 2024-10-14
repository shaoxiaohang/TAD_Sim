#include "DisplayPlayerController.h"

ADisplayPlayerController::ADisplayPlayerController()
{
    // Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
    bShowMouseCursor = bShowMouseConfig;
}
