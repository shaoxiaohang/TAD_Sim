#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "DisplayPlayerController.generated.h"

UCLASS()
class DISPLAY_API ADisplayPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    ADisplayPlayerController();

private:
    UPROPERTY(Config)
    bool bShowMouseConfig = true;

public:
    UPROPERTY(Config)
    int32 id_controlled = 0;

};