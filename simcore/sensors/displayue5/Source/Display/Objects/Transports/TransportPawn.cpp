#include "TransportPawn.h"
#include "Display/Components/BasicInfoComp.h"
#include "Components/SimMoveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CameraMasterComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/SpringArmComponent.h"

ATransportPawn::ATransportPawn()
{
    // Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;

    basicInfoComp = CreateDefaultSubobject<UBasicInfoComp>(FName(TEXT("BasicInfo")));
    RootComponent = basicInfoComp;

    simMoveComponent = CreateDefaultSubobject<USimMoveComponent>(FName(TEXT("SimMove")));

    meshComp = CreateDefaultSubobject<USkeletalMeshComponent>(FName(TEXT("Mesh")));
    meshComp->SetupAttachment(RootComponent);
    meshComp->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    meshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    meshComp->SetCollisionProfileName(FName(TEXT("SimVehicle")));

    cameraMasterComp = CreateDefaultSubobject<UCameraMasterComponent>(FName(TEXT("CameraMaster")));

    springArm_Bird = CreateDefaultSubobject<USpringArmComponent>(FName(TEXT("SpringArm_Bird")));
    springArm_Bird->SetupAttachment(meshComp, TEXT("FreeView"));
    springArm_Bird->SetRelativeRotation(FRotator(-89.f, 0.f, 0.f));
    springArm_Bird->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
    springArm_Bird->TargetArmLength = 2300;
    springArm_Bird->bDoCollisionTest = false;
    springArm_Bird->ProbeChannel = ECollisionChannel::ECC_GameTraceChannel2;

    camera_BirdView = CreateDefaultSubobject<UCameraComponent>(FName(TEXT("Camera_BirdView")));
    camera_BirdView->SetupAttachment(springArm_Bird);

    camera_Driver = CreateDefaultSubobject<UCameraComponent>(FName(TEXT("Camera_Driver")));
    camera_Driver->SetupAttachment(meshComp, TEXT("DriverView"));

    camera_Roof = CreateDefaultSubobject<UCameraComponent>(FName(TEXT("Camera_Roof")));
    camera_Roof->SetupAttachment(meshComp, TEXT("RoofView"));

    springArm_Free = CreateDefaultSubobject<USpringArmComponent>(FName(TEXT("SpringArm_Free")));
    springArm_Free->SetupAttachment(meshComp, TEXT("FreeView"));
    springArm_Free->TargetArmLength = 500;
    springArm_Free->bDoCollisionTest = false;
    springArm_Free->ProbeChannel = ECollisionChannel::ECC_GameTraceChannel2;

    camera_Free = CreateDefaultSubobject<UCameraComponent>(FName(TEXT("Camera_Free")));
    camera_Free->SetupAttachment(springArm_Free);

    SetReplicates(false);
}

void ATransportPawn::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    // Setup cameras of transport
    SetupCameras();
}

const FSimActorConfig* ATransportPawn::GetConfig() const
{
    check(basicInfoComp);
    return basicInfoComp->config.Get();
}

void ATransportPawn::BeginPlay()
{
    Super::BeginPlay();
}

void ATransportPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindAction("SwitchCamera", IE_Released, this, &ATransportPawn::SwitchCamera);
}

void ATransportPawn::SetupCameras()
{
    if (camera_BirdView)
    {
        cameraMasterComp->RegisterCamera(TEXT("Camera_BirdView"), camera_BirdView);
    }
    if (camera_Driver)
    {
        cameraMasterComp->RegisterCamera(TEXT("Camera_Driver"), camera_Driver);
    }
    if (camera_Roof)
    {
        cameraMasterComp->RegisterCamera(TEXT("Camera_Roof"), camera_Roof);
    }
    if (camera_Free && springArm_Free)
    {
        cameraMasterComp->RegisterCamera(TEXT("Camera_Free"), camera_Free);
    }
}

void ATransportPawn::SwitchCamera(FString _Name)
{
    if (cameraMasterComp)
    {
        cameraMasterComp->SwitchCameraByName(_Name);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Switch Camera Failed! cameraMasterComp Is Null!"))
    }
}

void ATransportPawn::SwitchCamera()
{
    SwitchCamera(FString());
}

void ATransportPawn::Init(const FSimActorConfig& _Config)
{
    const FTransportConfig* TConfig = Cast_Sim<const FTransportConfig>(_Config);
    check(TConfig);

    isEgoSnap = TConfig->isEgoSnap;
    trafficType = TConfig->trafficType;
    FTransform NewTransform;

    if (isEgoSnap)
    {
        if (simMoveComponent)
        {
            simMoveComponent->GetSnapGroundTransform(
                NewTransform, TConfig->startLocation, TConfig->startRotation, true);
        }
    }
    else
    {
        NewTransform = FTransform(TConfig->startRotation, TConfig->startLocation);
    }
    SetActorTransform(NewTransform);
    UE_LOG(LogTemp, Warning, TEXT("Transport init origin Loc: %s(Name: %s)"), *TConfig->startLocation.ToString(),
        *TConfig->typeName);
    UE_LOG(LogTemp, Warning, TEXT("Transport init Loc: %s(Name: %s)"), *this->GetActorLocation().ToString(),
        *TConfig->typeName);

    basicInfoComp->Init(*TConfig);
}

void ATransportPawn::Update(const FSimActorInput& _Input, FSimActorOutput& _Output)
{
    const FTransportIn* TransportIn = Cast_Sim<const FTransportIn>(_Input);
    FTransportOut* TransportOut = Cast_Sim<FTransportOut>(_Output);
    check(TransportIn);

    FTransform NewTransform;
    if (isEgoSnap)
    {
        FVector RaycastStart = TransportIn->location;
        RaycastStart.Z = this->GetActorLocation().Z;
        simMoveComponent->GetSnapGroundTransform(NewTransform, 
        RaycastStart, TransportIn->rotation);
    }
    else
    {
        NewTransform = FTransform(TransportIn->rotation, TransportIn->location);
    }

    SetActorTransform(NewTransform);

    basicInfoComp->Update(_Input);
}

void ATransportPawn::Destroy()
{
    Super::Destroy();
}

double ATransportPawn::GetTimeStamp() const
{
    if (basicInfoComp)
    {
        return basicInfoComp->timeStamp;
    }
    return -1;
}

void ATransportPawn::ApplyCatalogOffset(const FVector OffSet)
{
    if (bDisableCatalog)
    {
        return;
    }

    FVector LocalOffset = OffSet;
    LocalOffset.Y = -LocalOffset.Y;
    meshComp->SetRelativeLocation(LocalOffset * 100.f);
}

FVector ATransportPawn::BP_GetVelocity() const
{
    return basicInfoComp ? basicInfoComp->velocity : FVector();
}

float ATransportPawn::BP_GetTimeStamp() const
{
    return basicInfoComp ? basicInfoComp->timeStamp : 0;
}
