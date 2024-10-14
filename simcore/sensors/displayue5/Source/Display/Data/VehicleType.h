#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "VehicleType.generated.h"

class AVehiclePawn;
class USkeletalMesh;

UENUM(BlueprintType)
enum class ELightType : uint8
{
    LT_POINT UMETA(DisplayName = "PointLight"),
    LT_SPOT UMETA(DisplayName = "SpotLight"),
    LT_RECT UMETA(DisplayName = "RectLight"),
    LT_DIRECT UMETA(DisplayName = "DirectLight")
};

USTRUCT(BlueprintType)
struct FVehicleType : public FTableRowBase
{
    GENERATED_BODY()
public:
    FVehicleType(){};

    UPROPERTY(EditDefaultsOnly, Category = "Vehicle Type")
    // TypeName
    FName typeName;

    //UPROPERTY(EditDefaultsOnly, Category = "Vehicle Type")
    // BP class, if use BP template
    //TAssetSubclassOf<AVehiclePawn> blueprintClass;

    UPROPERTY(EditDefaultsOnly, Category = "Vehicle Type")
    // vehicle`s main skeletalmesh
    TAssetPtr<USkeletalMesh> vehicleMesh;

    UPROPERTY(EditDefaultsOnly, Category = "Vehicle Type")
    // vehicle`s car paint material
    TAssetPtr<UMaterial> carpaintMaterial;

    UPROPERTY(EditDefaultsOnly, Category = "Vehicle Type")
    // vehicle`s car paint material
    FColor carpaintColor;
};