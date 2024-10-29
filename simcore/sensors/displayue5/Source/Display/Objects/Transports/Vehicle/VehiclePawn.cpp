#include "VehiclePawn.h"

DEFINE_LOG_CATEGORY_STATIC(SimLogVehicle, Log, All);


AVehiclePawn::AVehiclePawn()
{

}

void AVehiclePawn::Init(const FSimActorConfig& _Config)
{
    ATransportPawn::Init(_Config);

    const FVehicleConfig* VehicleConfig = Cast_Sim<const FVehicleConfig>(_Config);
    check(VehicleConfig);
    vehicleConfig = *VehicleConfig;

    UE_LOG(LogTemp, Warning, TEXT("Name: %s, uuid: %d"), *VehicleConfig->typeName, uuid);

    // Register SimActor
    AManager::RegisterSimActor(this);

    UE_LOG(SimLogVehicle, Warning, TEXT("Vehicle Pawn Inited, Id: %d"), basicInfoComp->id);

}