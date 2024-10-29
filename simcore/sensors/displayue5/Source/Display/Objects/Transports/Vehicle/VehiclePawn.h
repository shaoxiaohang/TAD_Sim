#pragma once

#include "CoreMinimal.h"
#include "Objects/Transports/TransportPawn.h"
#include "VehicleInterface.h"
#include "Data/CatalogDataSource.h"
#include "VehiclePawn.generated.h"



UCLASS(config = game /*, perObjectConfig*/)
class DISPLAY_API AVehiclePawn : public ATransportPawn, public IVehicleInterface
{
    GENERATED_BODY()
public:
    AVehiclePawn();

public:
    /* ~ Simulator Interface ~ */

    // Init SimActor
    virtual void Init(const FSimActorConfig& _Config);


protected:
    UPROPERTY(config)
    int32 uuid = 0;

protected:

    // save init config
    FVehicleConfig vehicleConfig;

};