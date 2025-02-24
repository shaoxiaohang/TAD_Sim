#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/MeshComponent.h"
#include "FrustumMeshComponent.generated.h"

class FPrimitiveSceneProxy;
class UMaterial;
class UMaterialInstanceDynamic;


UCLASS()
class DISPLAY_API UFrustumMeshComponent : public UMeshComponent {
  GENERATED_UCLASS_BODY()

public:

  void SetDrawingParameters(float start_dis, float end_dis, float fov_x, float ratio, bool wireframe);

  void BuildMeshData();

public:

  UPROPERTY()
  UMaterial* wireframe_material_= nullptr;

  UPROPERTY()
  UMaterialInstanceDynamic* wireframe_material_dynamic_ = nullptr;

  float frustum_start_distance_;
  float frustum_end_distance_;
  float fov_x_;
  float aspect_ratio_;

  TArray<FVector> frusum_points_;

  bool wireframe_;

private:

  virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

  virtual int32 GetNumMaterials() const override;

  virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

  friend class FFrustumMeshSceneProxy;
};


