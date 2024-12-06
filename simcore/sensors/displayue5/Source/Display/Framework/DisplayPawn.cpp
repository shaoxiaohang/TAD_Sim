// Fill out your copyright notice in the Description page of Project Settings.

#include "DisplayPawn.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Components/SphereComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerInput.h"

FName ADisplayPawn::MovementComponentName(TEXT("MovementComponent0"));
FName ADisplayPawn::CollisionComponentName(TEXT("CollisionComponent0"));
FName ADisplayPawn::MeshComponentName(TEXT("MeshComponent0"));

ADisplayPawn::ADisplayPawn()
{
    SetCanBeDamaged(true);
    SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
    bReplicates = true;
    NetPriority = 3.0f;

    BaseEyeHeight = 0.0f;
    bCollideWhenPlacing = false;
    SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    CollisionComponent = CreateDefaultSubobject<USphereComponent>(ADisplayPawn::CollisionComponentName);
    CollisionComponent->InitSphereRadius(35.0f);
    CollisionComponent->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);

    CollisionComponent->CanCharacterStepUpOn = ECB_No;
    CollisionComponent->SetShouldUpdatePhysicsVolume(true);
    CollisionComponent->SetCanEverAffectNavigation(false);
    CollisionComponent->bDynamicObstacle = true;

    RootComponent = CollisionComponent;

    MovementComponent =
        CreateDefaultSubobject<UPawnMovementComponent, UFloatingPawnMovement>(ADisplayPawn::MovementComponentName);
    MovementComponent->UpdatedComponent = CollisionComponent;

    // Structure to hold one-time initialization
    struct FConstructorStatics
    {
        ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh;
        FConstructorStatics() : SphereMesh(TEXT("/Engine/EngineMeshes/Sphere"))
        {
        }
    };

    static FConstructorStatics ConstructorStatics;

    MeshComponent = CreateOptionalDefaultSubobject<UStaticMeshComponent>(ADisplayPawn::MeshComponentName);
    if (MeshComponent)
    {
        MeshComponent->SetStaticMesh(ConstructorStatics.SphereMesh.Object);
        MeshComponent->AlwaysLoadOnClient = true;
        MeshComponent->AlwaysLoadOnServer = true;
        MeshComponent->bOwnerNoSee = true;
        MeshComponent->bCastDynamicShadow = true;
        MeshComponent->bAffectDynamicIndirectLighting = false;
        MeshComponent->bAffectDistanceFieldLighting = false;
        MeshComponent->PrimaryComponentTick.TickGroup = TG_PrePhysics;
        MeshComponent->SetupAttachment(RootComponent);
        MeshComponent->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
        const float Scale =
            CollisionComponent->GetUnscaledSphereRadius() /
            160.f;    // @TODO: hardcoding known size of EngineMeshes.Sphere. Should use a unit sphere instead.
        MeshComponent->SetRelativeScale3D(FVector(Scale));
        MeshComponent->SetGenerateOverlapEvents(false);
        MeshComponent->SetCanEverAffectNavigation(false);

        MeshComponent->SetHiddenInGame(true);
    }

    // This is the default pawn class, we want to have it be able to move out of the box.
    bAddDefaultMovementBindings = true;

    BaseTurnRate = 45.f;
    BaseLookUpRate = 45.f;
}

void ADisplayPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    check(PlayerInputComponent);

    if (bAddDefaultMovementBindings)
    {
        // InitializeDefaultPawnInputBindings();

        PlayerInputComponent->BindAxis("MoveForward", this, &ADisplayPawn::MoveForward);
        PlayerInputComponent->BindAxis("MoveRight", this, &ADisplayPawn::MoveRight);
        PlayerInputComponent->BindAxis("MoveUp", this, &ADisplayPawn::MoveUp_World);
        PlayerInputComponent->BindAxis("Turn", this, &ADisplayPawn::AddControllerYawInput);
        // PlayerInputComponent->BindAxis("DefaultPawn_TurnRate", this, &ADisplayPawn::TurnAtRate);
        PlayerInputComponent->BindAxis("LookUp", this, &ADisplayPawn::AddControllerPitchInput);
        // PlayerInputComponent->BindAxis("DefaultPawn_LookUpRate", this, &ADisplayPawn::LookUpAtRate);
        PlayerInputComponent->BindAction(
            TEXT("IncreaseSpeed"), EInputEvent::IE_Released, this, &ADisplayPawn::increaseSpeed);
        PlayerInputComponent->BindAction(
            TEXT("ReduceSpeed"), EInputEvent::IE_Released, this, &ADisplayPawn::reduceSpeed);
    }
}

void ADisplayPawn::UpdateNavigationRelevance()
{
    if (CollisionComponent)
    {
        CollisionComponent->SetCanEverAffectNavigation(bCanAffectNavigationGeneration);
    }
}

void ADisplayPawn::MoveRight(float Val)
{
    if (Val != 0.f)
    {
        if (Controller)
        {
            FRotator const ControlSpaceRot = Controller->GetControlRotation();

            // transform to world space and add it
            AddMovementInput(FRotationMatrix(ControlSpaceRot).GetScaledAxis(EAxis::Y), Val * speedScale);
        }
    }
}

void ADisplayPawn::MoveForward(float Val)
{
    if (Val != 0.f)
    {
        if (Controller)
        {
            FRotator const ControlSpaceRot = Controller->GetControlRotation();

            // transform to world space and add it
            AddMovementInput(FRotationMatrix(ControlSpaceRot).GetScaledAxis(EAxis::X), Val * speedScale);
        }
    }
}

void ADisplayPawn::MoveUp_World(float Val)
{
    if (Val != 0.f)
    {
        AddMovementInput(FVector::UpVector, Val * speedScale);
    }
}

void ADisplayPawn::TurnAtRate(float Rate)
{
    // calculate delta for this frame from the rate information
    AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds() * CustomTimeDilation);
}

void ADisplayPawn::LookUpAtRate(float Rate)
{
    // calculate delta for this frame from the rate information
    AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds() * CustomTimeDilation);
}

void ADisplayPawn::increaseSpeed()
{
    speedScale += 0.5f;
}

void ADisplayPawn::reduceSpeed()
{
    if (speedScale > 0.5f)
    {
        speedScale -= 0.5f;
    }
}

void ADisplayPawn::BeginPlay()
{
    Super::BeginPlay();
    AddActorWorldOffset(start_location_offset);
    AddActorWorldRotation(start_rotation_offset);
}

UPawnMovementComponent* ADisplayPawn::GetMovementComponent() const
{
    return MovementComponent;
}
