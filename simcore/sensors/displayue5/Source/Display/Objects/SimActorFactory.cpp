#include "SimActorFactory.h"
#include "Engine/World.h"
#include "SimActorInterface.h"

// Sets default values
ASimActorFactory::ASimActorFactory()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = false;
}
