#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MannedControlComponent.generated.h"

UENUM()
enum class EDriveMode : uint8
{
    Manned,
    Auto
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), config = game)
class DISPLAY_API UMannedControlComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UMannedControlComponent();

protected:

    UPROPERTY()
    EDriveMode mode = EDriveMode::Auto;

};