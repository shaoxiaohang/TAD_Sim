#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CameraMasterComponent.generated.h"

class UCameraComponent;
class USpringArmComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DISPLAY_API UCameraMasterComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UCameraMasterComponent();

protected:

    TMap<FString, UCameraComponent*> cameraMap;

    TArray<FString> cameraNameOrderArry;

    FString currentCameraName;

    void SwitchCamera(FString _CameraName);

public:

    bool RegisterCamera(FString _Name, UCameraComponent* _Camera);

    void SwitchCameraByName(FString _CameraName = FString(TEXT("")));

    UCameraComponent* GetCurrentCamera();

    FORCEINLINE FString GetCurrentCameraName();

    bool SetCurrentCamera(FString _Name);

    void SwitchCamera();
};