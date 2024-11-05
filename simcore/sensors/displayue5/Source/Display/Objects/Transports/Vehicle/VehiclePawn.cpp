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

void AVehiclePawn::Update(const FSimActorInput& _Input, FSimActorOutput& _Output)
{
    ATransportPawn::Update(_Input, _Output);

    const FVehicleIn* VehicleIn = Cast_Sim<const FVehicleIn>(_Input);
    FVehicleOut* VehicleOut = Cast_Sim<FVehicleOut>(_Output);
    *(FSimActorInput*) VehicleOut = _Input;
    check(VehicleIn);

    VehicleOut->locPose = this->GetActorLocation();
    VehicleOut->rotPose = this->GetActorRotation();
    VehicleOut->bHasPose = true;
    if (VehicleOut->typeName.IsEmpty())
    {
        VehicleOut->type = vehicleConfig.type;
        VehicleOut->typeName = vehicleConfig.typeName;
    }
    VehicleOut->sizeLWH = GetComponentsBoundingBox().GetSize();
}