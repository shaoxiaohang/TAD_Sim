#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TrajectoryComponent.generated.h"

UENUM()
enum class ERenderModel : uint8
{
    SPLINEMESH,
    PROCEDURALMESH
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), Config = Game)
class DISPLAY_API UTrajectoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UTrajectoryComponent();

};