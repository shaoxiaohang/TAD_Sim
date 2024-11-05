#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Display/Objects/SimActorInterface.h"
#include "TransportInterface.h"
#include "TransportPawn.generated.h"

UCLASS(Abstract, config = game /*, perObjectConfig*/)
class DISPLAY_API ATransportPawn : public APawn, public ISimActorInterface, public ITransportInterface
{
    GENERATED_BODY()

public:
    // Sets default values for this pawn's properties
    ATransportPawn();

    // Called to bind functionality to input
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    /**
     * Called when an instance of this class is placed (in editor) or spawned.
     * @param    Transform            The transform the actor was constructed at.
     */
    virtual void OnConstruction(const FTransform& Transform);

    virtual void ApplyCatalogOffset(const FVector OffSet);

    UFUNCTION(BlueprintCallable)
    FVector BP_GetVelocity() const;

    UFUNCTION(BlueprintCallable)
    float BP_GetTimeStamp() const;

    FString GetDefaultCameraName() const
    {
        return defaultCameraName;
    }

    virtual void SwitchCamera(FString _Name);

    void SwitchCamera();

    bool InstallCamera(const FString& _Name, class UCameraComponent* _Camera);

    bool SetDefaultCamera(const FString& _Name);

public:
    /* ~ ISimActorInterface~ */

    // Get the configuration from SimActor
    virtual const FSimActorConfig* GetConfig() const;
    // Init SimActor
    virtual void Init(const FSimActorConfig& _Config);
    // Update SimActor
    virtual void Update(const FSimActorInput& _Input, FSimActorOutput& _Output);
    // Destroy the SimActor
    virtual void Destroy();
    // Get current timestamp
    virtual double GetTimeStamp() const;


protected:

    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    virtual void SetupCameras();

public:

    UPROPERTY(EditAnyWhere)
    bool bDisableCatalog = false;

protected:

    // Basic root component
    class UBasicInfoComp* basicInfoComp = NULL;

    // Sim move component
    class USimMoveComponent* simMoveComponent = NULL;

    // Skeleton mesh component
    UPROPERTY(BlueprintReadWrite, VisibleDefaultsOnly, meta = (AllowPrivateAccess = "true"))
    class USkeletalMeshComponent* meshComp = NULL;

    bool isEgoSnap = true;
    ETrafficType trafficType = ETrafficType::ST_TRAFFIC;

    // Camera component
    class UCameraMasterComponent* cameraMasterComp = NULL;

    UPROPERTY(config)
    FString defaultCameraName = TEXT("Camera_BirdView");

    // Camera for view
    UPROPERTY(VisibleDefaultsOnly, meta = (AllowPrivateAccess = "true"))
    class UCameraComponent* camera_BirdView = NULL;
    UPROPERTY(VisibleDefaultsOnly, meta = (AllowPrivateAccess = "true"))
    class UCameraComponent* camera_Driver = NULL;
    UPROPERTY(VisibleDefaultsOnly, meta = (AllowPrivateAccess = "true"))
    class UCameraComponent* camera_Roof = NULL;
    UPROPERTY(VisibleDefaultsOnly, meta = (AllowPrivateAccess = "true"))
    class UCameraComponent* camera_Free = NULL;
    UPROPERTY(VisibleDefaultsOnly, meta = (AllowPrivateAccess = "true"))
    class USpringArmComponent* springArm_Free = NULL;
    UPROPERTY(VisibleDefaultsOnly, meta = (AllowPrivateAccess = "true"))
    class USpringArmComponent* springArm_Bird = NULL;
};