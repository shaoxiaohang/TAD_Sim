#pragma once

#include "CoreMinimal.h"

#include "GameFramework/GameStateBase.h"

#include "DisplayGameStateBase.generated.h"

UCLASS()
class DISPLAY_API ADisplayGameStateBase : public AGameStateBase
{
    GENERATED_BODY()
public:
    /** Constructor for AActor that takes an ObjectInitializer for backward compatibility */
    ADisplayGameStateBase(const FObjectInitializer& ObjectInitializer);


};