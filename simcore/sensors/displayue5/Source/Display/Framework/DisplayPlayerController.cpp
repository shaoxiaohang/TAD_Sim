// Fill out your copyright notice in the Description page of Project Settings.

#include "DisplayPlayerController.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DisplayGameInstance.h"
#include "DisplayGameModeBase.h"
#include "DisplayGameStateBase.h"
#include "DisplayPawn.h"
#include "DisplayPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GodPawn.h"
#include "Managers/TransportManager.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "UI/DrivingWidget.h"


DEFINE_LOG_CATEGORY_STATIC(SimLogPlayerController, Log, All);

ADisplayPlayerController::ADisplayPlayerController()
{
    // Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
    bShowMouseCursor = bShowMouseConfig;
}

void ADisplayPlayerController::OnSimResetInput()
{
}

void ADisplayPlayerController::OnSimUpdateInput()
{
}

FString ADisplayPlayerController::ConsoleCommand(const FString& Command, bool bWriteToLog)
{
    //#if WITH_EDITOR
    return Super::ConsoleCommand(Command, bWriteToLog);
    // #else
    //     if (Command == TEXT("EnableConsoleCommand"))
    //     {
    //         bEnableConsole = true;
    //     }
    //     if (bEnableConsole)
    //     {
    //         return Super::ConsoleCommand(Command, bWriteToLog);
    //     }
    //     return FString();
    // #endif
}

void ADisplayPlayerController::Server_SimResetOutput_Implementation(FLocalResetOut _ReturnData)
{
    if (GetGameInstance<UDisplayGameInstance>()
            ->ModuleGroupName.IsEmpty())    // Display配置在全局算法，则创建选择主车UI
    {
        if (!SwitchEgoWidget)
        {
            UClass* MyWidgetClass =
                LoadClass<UUserWidget>(NULL, TEXT("WidgetBlueprint'/Game/UI/WBP_SwitchEgo.WBP_SwitchEgo_C'"));
            if (MyWidgetClass)
            {
                SwitchEgoWidget = CreateWidget<UUserWidget>(this, MyWidgetClass);
                if (SwitchEgoWidget)
                {
                    SwitchEgoWidget->AddToViewport(1);
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("DisplayPlayerController: Cant load /Game/UI/WBP_SwitchEgo UWidgetBlueprint asset!"));
            }
        }
    }
    PossessVehicleExec(GetGameInstance<UDisplayGameInstance>()->GetEgoIDByGroupName());

    GetWorld()->GetAuthGameMode<ADisplayGameModeBase>()->SimOutput(
        _ReturnData, GetPlayerState<ADisplayPlayerState>()->GetUniqueId());
}

bool ADisplayPlayerController::Server_SimResetOutput_Validate(FLocalResetOut _ReturnData)
{
    return true;
}

void ADisplayPlayerController::Server_SimUpdateOutput_Implementation(FLocalUpdateOut _OutData)
{
    GetWorld()->GetAuthGameMode<ADisplayGameModeBase>()->SimOutput(
        _OutData, GetPlayerState<ADisplayPlayerState>()->GetUniqueId());
}

bool ADisplayPlayerController::Server_SimUpdateOutput_Validate(FLocalUpdateOut _OutData)
{
    return true;
}

UDrivingWidget* ADisplayPlayerController::GetDrivingWidget()
{
    if (!driving_widget)
    {
        /* Driving UI */
        UClass* DrivingWidgetClass =
            LoadClass<UDrivingWidget>(NULL, TEXT("WidgetBlueprint'/Game/UI/WBP_RearView.WBP_RearView_C'"));
        if (DrivingWidgetClass)
        {
            driving_widget = CreateWidget<UDrivingWidget>(this, DrivingWidgetClass);
            if (driving_widget)
            {
                driving_widget->AddToViewport(1);
                driving_widget->SetVisibility(ESlateVisibility::Hidden);
                // UE_LOG(LogTemp, Error, TEXT("DisplayPlayerController: Create
                // WidgetBlueprint'/Game/UI/WBP_RearView.WBP_RearView'!"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error,
                TEXT("DisplayPlayerController: Cant load WidgetBlueprint'/Game/UI/WBP_RearView.WBP_RearView'!"));
        }
    }
    return driving_widget;
}

void ADisplayPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    if (driving_widget)
    {
        driving_widget->SetVisibility(ESlateVisibility::Collapsed);
        driving_widget->RemoveFromParent();
        driving_widget = nullptr;
    }
    if (SwitchEgoWidget)
    {
        SwitchEgoWidget->SetVisibility(ESlateVisibility::Collapsed);
        SwitchEgoWidget->RemoveFromParent();
        SwitchEgoWidget = nullptr;
    }
    // CollectGarbage(EObjectFlags::RF_NoFlags);
}

void ADisplayPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ADisplayPlayerController::PossessActor(AActor* _ActorToPossess)
{
    if (!_ActorToPossess)
    {
        UE_LOG(SimLogPlayerController, Warning, TEXT("The Actor To Possess Is Null!"));
        return;
    }
    APawn* Pawn = Cast<APawn>(_ActorToPossess);
    if (Pawn)
    {
        OnPossess(Pawn);
    }
    else
    {
        UE_LOG(SimLogPlayerController, Warning, TEXT("The Actor To Possess Is Not A Pawn!"));
    }
}

void ADisplayPlayerController::SetPossessEgoName(const FString& VehicleName)
{
    PossessVehicleExec(GetGameInstance<UDisplayGameInstance>()->GetEgoIDByGroupName(VehicleName));
}

void ADisplayPlayerController::PossessVehicleExec(int64 _Id)
{
    TWeakObjectPtr<ATransportManager> TM =
        GetWorld()->GetGameState<ADisplayGameStateBase>()->syncSystem->transportManager;
    if (TM.IsValid() && TM->vehicleManager)
    {
        ISimActorInterface* SimActor = TM->vehicleManager->GetVehicle(ETrafficType::ST_Ego, _Id);
        if (SimActor)
        {
            id_controlled = _Id;
            OnPossess(Cast<APawn>(SimActor));
            if (AVehiclePawn* EgoVehicle = Cast<AVehiclePawn>(SimActor))
            {
                EgoVehicle->SwitchCamera(EgoVehicle->GetDefaultCameraName());
            }
            UE_LOG(SimLogPlayerController, Log, TEXT("possess pawn(name:%s, id:%ld)"),
                *(Cast<APawn>(SimActor)->GetName()), _Id);
            return;
        }
        else
        {
            SimActor = TM->vehicleManager->GetVehicle(ETrafficType::ST_TRAFFIC, _Id);
            if (SimActor)
            {
                id_controlled = _Id;
                OnPossess(Cast<APawn>(SimActor));
                UE_LOG(SimLogPlayerController, Log, TEXT("possess pawn(name:%s, id:%ld)"),
                    *(Cast<APawn>(SimActor)->GetName()), _Id);
                return;
            }
        }

        UE_LOG(SimLogPlayerController, Log, TEXT("Cant possess target vehicle!(id: %ld)"), _Id);
    }
}

// void ADisplayPlayerController::DebugGate(bool bEnable)
// {
//     TArray<AActor*> FoundActors;
//     UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGate::StaticClass(), FoundActors);
//     for (auto Elem : FoundActors)
//     {
//         if (AGate* Gate = Cast<AGate>(Elem))
//         {
//             Gate->ShowDebug(bEnable);
//         }
//     }
// }

void ADisplayPlayerController::BeginPlay()
{
    Super::BeginPlay();

    /* Driving UI */
    if (!driving_widget || !driving_widget->IsValidLowLevel())
    {
        UClass* DrivingWidgetClass =
            LoadClass<UDrivingWidget>(NULL, TEXT("WidgetBlueprint'/Game/UI/WBP_RearView.WBP_RearView_C'"));
        if (DrivingWidgetClass)
        {
            driving_widget = CreateWidget<UDrivingWidget>(this, DrivingWidgetClass);
            if (driving_widget)
            {
                driving_widget->AddToViewport(1);
                driving_widget->SetVisibility(ESlateVisibility::Hidden);
            }
        }
        else
        {
            UE_LOG(LogTemp, Error,
                TEXT("DisplayPlayerController: Cant load WidgetBlueprint'/Game/UI/WBP_RearView.WBP_RearView'!"));
        }
    }

    // Create pawn
    ghostPawn = GetWorld()->SpawnActor<ADisplayPawn>();
    godPawn = GetWorld()->SpawnActor<AGodPawn>();
}

void ADisplayPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    check(InputComponent);
    // InputComponent->BindAction(TEXT(""))

    InputComponent->BindAction(
        TEXT("ToggleUIVisibility"), EInputEvent::IE_Released, this, &ADisplayPlayerController::ToggleUIVisibility);
    InputComponent->BindAction(
        TEXT("Ghost"), EInputEvent::IE_Released, this, &ADisplayPlayerController::SwitchPawnToEgoOrGhost);
    InputComponent->BindAction(TEXT("God"), EInputEvent::IE_Released, this, &ADisplayPlayerController::SwitchPawnToGod);
    InputComponent->BindAction(TEXT("Ego"), EInputEvent::IE_Released, this, &ADisplayPlayerController::SwitchPawnToEgo);
    InputComponent->BindAction(
        TEXT("SwitchDrivingMode"), IE_Released, this, &ADisplayPlayerController::ToggleDriveMode);
}

void ADisplayPlayerController::SwitchPawnToEgo()
{
    TWeakObjectPtr<ATransportManager> TM =
        GetWorld()->GetGameState<ADisplayGameStateBase>()->syncSystem->transportManager;
    if (TM.IsValid() && TM->vehicleManager)
    {
        ISimActorInterface* SimActor = TM->vehicleManager->GetVehicle(ETrafficType::ST_Ego, 0);
        if (SimActor)
        {
            AVehiclePawn* EgoPawn = Cast<AVehiclePawn>(SimActor);
            if (EgoPawn)
            {
                this->Possess(EgoPawn);
                EgoPawn->SwitchCamera(EgoPawn->GetDefaultCameraName());
                return;
            }
        }
    }
    UE_LOG(SimLogPlayerController, Warning, TEXT("Switch Pawn To Ego Failed!"));
}

void ADisplayPlayerController::SwitchPawnToGhost()
{
    if (bEnableGhost && ghostPawn)
    {
        if (this->GetPawn() != ghostPawn)
        {
            // Snap to current camera view
            float Yaw = GetPawn()->GetActorRotation().Yaw;
            float Pitch = GetPawn()->GetActorRotation().Pitch;
            ghostPawn->SetActorLocation(this->PlayerCameraManager->GetCameraLocation());
            ghostPawn->SetActorRotation(FRotator(Pitch, Yaw, 0.f));
            ghostPawn->SetActorEnableCollision(false);
            this->Possess(ghostPawn);
        }
        else
        {
            UE_LOG(SimLogPlayerController, Log, TEXT("Switch Pawn Cancel, Current Pawn Is Ghost!"));
        }
        return;
    }
    UE_LOG(SimLogPlayerController, Warning, TEXT("Switch Pawn To Ghost Failed!"));
}

void ADisplayPlayerController::SwitchPawnToGod()
{
    if (bEnableGod && godPawn)
    {
        if (this->GetPawn() != godPawn)
        {
            // Move godpawn to above egovehicle
            TWeakObjectPtr<ATransportManager> TM =
                GetWorld()->GetGameState<ADisplayGameStateBase>()->syncSystem->transportManager;
            if (TM.IsValid() && TM->vehicleManager)
            {
                ISimActorInterface* SimActor = TM->vehicleManager->GetVehicle(ETrafficType::ST_Ego, 0);
                if (SimActor)
                {
                    APawn* EgoPawn = Cast<APawn>(SimActor);
                    if (EgoPawn)
                    {
                        godPawn->SetActorLocationAndRotation(
                            EgoPawn->GetActorLocation() + FVector(0, 0, 100000), FRotator(-90, 0, 0));
                    }
                }
            }
            this->Possess(godPawn);
        }
        else
        {
            UE_LOG(SimLogPlayerController, Log, TEXT("Switch Pawn Cancel, Current Pawn Is God!"));
        }
        return;
    }
    UE_LOG(SimLogPlayerController, Warning, TEXT("Switch Pawn To God Failed!"));
}

void ADisplayPlayerController::SwitchPawnToEgoOrGhost()
{
    if (GetGameInstance<UDisplayGameInstance>()->bAllowSync)
    {
        UClass* WanderInfoClass =
            LoadClass<UUserWidget>(NULL, TEXT("WidgetBlueprint'/Game/UI/WBP_WanderInfo.WBP_WanderInfo_C'"));
        if (WanderInfoClass)
        {
            if (WanderInfoWidget)
            {
                WanderInfoWidget->RemoveFromParent();
                WanderInfoWidget = nullptr;
            }

            WanderInfoWidget = CreateWidget<UUserWidget>(this, WanderInfoClass);
            if (WanderInfoWidget)
            {
                WanderInfoWidget->AddToViewport(1);
            }
        }
        return;
    }

    if (this->GetPawn() != ghostPawn)
    {
        SwitchPawnToGhost();
    }
    else
    {
        SwitchPawnToEgo();
    }
}

void ADisplayPlayerController::ToggleDriveMode()
{
    if (!bAllowSwitchDriveMode)
    {
        UE_LOG(SimLogPlayerController, Log, TEXT("Switch drive mode feature has been disabled"));
        return;
    }

    if (modeDrive == 1)
    {
        modeDrive = 0;
        UE_LOG(SimLogPlayerController, Log, TEXT("drive mode is auto"));
    }
    else if (modeDrive == 0)
    {
        modeDrive = 1;
        UE_LOG(SimLogPlayerController, Log, TEXT("drive mode is manned"));
    }
}

void ADisplayPlayerController::ToggleUIVisibility()
{
}
