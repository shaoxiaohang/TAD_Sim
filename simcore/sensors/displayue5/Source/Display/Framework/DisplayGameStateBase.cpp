#include "DisplayGameStateBase.h"
#include "DisplayPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogDebugGameState, Log, All);

FLocalUpdateOut ASyncSystem::SyncSimActors(const FLocalData& _Data)
{
    if (_Data.name == TEXT("RESET"))
    {
        resetIn = *static_cast<const FLocalResetIn*>(&_Data);
        SpawnAndInitAllManagers();
    }
    if (_Data.name == TEXT("UPDATE"))
    {
        updateIn = *static_cast<const FLocalUpdateIn*>(&_Data);
        updateOut = UpdateAllManagers(updateIn);
    }
    if (_Data.name == TEXT("OUTPUT_SENSOR"))
    {
        updateIn.sensorManager.timeStamp = _Data.timeStamp;
        updateIn.sensorManager.timeStamp_ego = _Data.timeStamp_ego;
        updateIn.sensorManager.timeStamp_tail = _Data.timeStamp_tail;
        SensorManager->Update(updateIn.sensorManager, updateOut.sensorManager);
        updateOut.message = TEXT("SUCCESS");
    }
    return updateOut;
}

bool ASyncSystem::SpawnAndInitAllManagers()
{
    bool AllManagerReady = true;

    /* Map */

    /* Transports */
    transportManager = GetWorld()->SpawnActor<ATransportManager>();
    if (transportManager.IsValid())
    {
        transportManager->Init(resetIn.transportManager);
    }
    else
    {
        AllManagerReady = false;
        UE_LOG(LogTemp, Error, TEXT("Can not spawn TransportManager!"));
    }

    /* Sensors */
    SensorManager = GetWorld()->SpawnActor<ASensorManager>();
    if (SensorManager)
    {
        SensorManager->Init(resetIn.sensorManager);
    }
    else
    {
        AllManagerReady = false;
        UE_LOG(LogTemp, Error, TEXT("Can not spawn SensorManager!"));
    }

    OnAllManagersInit();
    return AllManagerReady;
}

FLocalUpdateOut ASyncSystem::UpdateAllManagers(const FLocalUpdateIn& _In)
{
    FLocalUpdateOut UpdateOut = FLocalUpdateOut();

    transportManager->Update(_In.transportManager, UpdateOut.transportManager);

    UpdateOut.message = TEXT("SUCCESS");

    return UpdateOut;
}

void ASyncSystem::OnAllManagersInit()
{
    bool SwitchCameraToEgo = false;
    if (transportManager.IsValid() && transportManager->vehicleManager)
    {
        if (transportManager->vehicleManager->egoArry.Num() > 0)
        {
            ATransportPawn* Ego = Cast<ATransportPawn>(transportManager->vehicleManager->egoArry[0]);
            if (Ego)
            {
                GetWorld()->GetFirstPlayerController<ADisplayPlayerController>()->SwitchPawnToEgo();
                SwitchCameraToEgo = true;
            }
        }
    }
    if (!SwitchCameraToEgo)
    {
        GetWorld()->GetFirstPlayerController<ADisplayPlayerController>()->SwitchPawnToGhost();
    }
}

ADisplayGameStateBase::ADisplayGameStateBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void ADisplayGameStateBase::SimInput(const FLocalData& _Data)
{
    if (_Data.name == TEXT("RESET"))
    {
        resetIn = *static_cast<const FLocalResetIn*>(&_Data);
        Multicast_Reset(resetIn);
    }
    else if (_Data.name == TEXT("UPDATE"))
    {
        updateIn = *static_cast<const FLocalUpdateIn*>(&_Data);
        Multicast_Update(updateIn);
    }
    else if (_Data.name == TEXT("OUTPUT_SENSOR"))
    {
        FLocalUpdateIn InData;
        InData.timeStamp = _Data.timeStamp;
        InData.name = _Data.name;
        InData.timeStamp_ego = _Data.timeStamp_ego;
        InData.timeStamp_tail = _Data.timeStamp_tail;
        Multicast_Update(InData);
    }
}

void ADisplayGameStateBase::Multicast_Reset_Implementation(FLocalResetIn _ResetData)
{
    UE_LOG(LogDebugGameState, Log, TEXT("Multicast_Reset"));

    if (GetGameInstance()->IsDedicatedServerInstance())
    {
        UE_LOG(LogDebugGameState, Log, TEXT("Execute Multicast_Reset Server."));
    }
    else
    {
        resetIn = _ResetData;
        if (!syncSystem)
        {
            syncSystem = GetWorld()->SpawnActor<ASyncSystem>();
        }
        syncSystem->SyncSimActors(resetIn);
    }
}

bool ADisplayGameStateBase::Multicast_Reset_Validate(FLocalResetIn _ResetData)
{
    // TODO: check value is legal
    // ..
    return true;
}

void ADisplayGameStateBase::Multicast_Update_Implementation(FLocalUpdateIn _UpdateData)
{
    if (GetGameInstance()->IsDedicatedServerInstance())
    {
        UE_LOG(LogDebugGameState, Log, TEXT("Execute Multicast_Update Server."));
    }
    else
    {
        if (_UpdateData.name == TEXT("UPDATE"))
        {
            updateIn = _UpdateData;
            FLocalUpdateOut UpdateOut = syncSystem->SyncSimActors(updateIn);
            UpdateOut.timeStamp = _UpdateData.timeStamp;
            UpdateOut.name = TEXT("UPDATE");
            GetWorld()->GetFirstPlayerController<ADisplayPlayerController>()->Server_SimUpdateOutput(UpdateOut);
        }
        if (_UpdateData.name == TEXT("OUTPUT_SENSOR"))
        {
            FLocalUpdateOut UpdateOut = syncSystem->SyncSimActors(_UpdateData);
            UpdateOut.name = TEXT("OUTPUT_SENSOR");
            UpdateOut.timeStamp = _UpdateData.timeStamp;
            UpdateOut.timeStamp_ego = _UpdateData.timeStamp_ego;
            UpdateOut.timeStamp_tail = _UpdateData.timeStamp_tail;
            GetWorld()->GetFirstPlayerController<ADisplayPlayerController>()->Server_SimUpdateOutput(UpdateOut);
        }
    }
}


bool ADisplayGameStateBase::Multicast_Update_Validate(FLocalUpdateIn _UpdateData)
{
    // TODO: check value is legal
    // ..
    return true;
}