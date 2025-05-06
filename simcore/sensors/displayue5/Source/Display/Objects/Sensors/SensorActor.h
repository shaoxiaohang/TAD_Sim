#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SensorInterface.h"
#include "SensorActor.generated.h"

UCLASS()
class DISPLAY_API ASensorActor : public AActor, public ISensorInterface
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    ASensorActor();

    virtual bool Init(const FSensorConfig& Config);

    virtual void Update(const FSensorInput& Input, FSensorOutput& Output);

    virtual void Destroy(FString Reason);

    virtual ISimActorInterface* Install(const FSensorConfig& _Config);

    class UDisplayGameInstance* GetDisplayInstance();

	static void UseUnlitShowFlags(FEngineShowFlags& ShowFlags)
	{
		ShowFlags = FEngineShowFlags(ESFIM_All0);
		ShowFlags.SetRendering(true);
		ShowFlags.SetMaterials(true);
		// ShowFlags.SetBones(true);
		ShowFlags.SetSkeletalMeshes(true);
		ShowFlags.SetStaticMeshes(true);
		ShowFlags.SetInstancedStaticMeshes(true);
		ShowFlags.SetInstancedFoliage(true);
		ShowFlags.SetInstancedGrass(true);
		// ShowFlags.SetParticles(true);
		// ShowFlags.SetNiagara(true);
		ShowFlags.SetLandscape(true);
		ShowFlags.SetBrushes(true);
		ShowFlags.SetPostProcessMaterial(true);
		ShowFlags.SetPostProcessing(true);
		ShowFlags.SetNaniteMeshes(true);
		ShowFlags.SetNaniteStreamingGeometry(true);
		ShowFlags.SetTonemapper(false);
		ShowFlags.SetEyeAdaptation(false);
	}

	static void UseLitShowFlags(FEngineShowFlags& ShowFlags, bool enableAA = true)
	{
		ShowFlags = FEngineShowFlags(ESFIM_All0);


		ShowFlags.SetRendering(true);

		//Assets
		ShowFlags.SetMaterials(true);
		ShowFlags.SetSkeletalMeshes(true);
		ShowFlags.SetStaticMeshes(true);
		ShowFlags.SetInstancedStaticMeshes(true);
		ShowFlags.SetNaniteMeshes(true);
		ShowFlags.SetNaniteStreamingGeometry(true);
		ShowFlags.SetLandscape(true);
		// ShowFlags.SetBones(true);
		ShowFlags.SetInstancedFoliage(true);
		ShowFlags.SetInstancedGrass(true);
		ShowFlags.SetParticles(true);
		ShowFlags.SetNiagara(true);
		ShowFlags.SetDecals(true);
		// ShowFlags.SetBrushes(true);

		// Weather
		ShowFlags.SetCloud(true);
		ShowFlags.SetAtmosphere(true);
		ShowFlags.SetFog(true);
		ShowFlags.SetVolumetricFog(true);

		// Lighting
		ShowFlags.SetLighting(true);
		ShowFlags.SetDeferredLighting(true);
		ShowFlags.SetDirectLighting(true);
		ShowFlags.SetDirectionalLights(true);
		ShowFlags.SetSkyLighting(true);
		ShowFlags.SetPointLights(true);
		ShowFlags.SetSpotLights(true);
		ShowFlags.SetRectLights(true);
		ShowFlags.SetTranslucency(true);
		ShowFlags.SetLightFunctions(true);

		ShowFlags.SetGlobalIllumination(true);
		ShowFlags.SetLumenDetailTraces(true);
		ShowFlags.SetLumenShortRangeAmbientOcclusion(true);
		ShowFlags.SetLumenGlobalIllumination(true);
		//ShowFlags.SetGlobalIllumination(true);
		ShowFlags.SetLumenReflections(true);
		ShowFlags.SetLumenScreenTraces(true);
		ShowFlags.SetLumenSecondaryBounces(true);
		ShowFlags.SetLumenFarFieldTraces(true);
		ShowFlags.SetLumenGlobalTraces(true);
		

		ShowFlags.SetDynamicShadows(true);
		//ShowFlags.SetVirtualShadowMapCaching(true);
		ShowFlags.SetScreenSpaceReflections(true);
		ShowFlags.SetAmbientOcclusion(true);
		ShowFlags.SetDistanceFieldAO(true);

		// Post Processing
		ShowFlags.SetPostProcessMaterial(true);
		ShowFlags.SetPostProcessing(true);
		ShowFlags.SetAntiAliasing(true);
		ShowFlags.SetTemporalAA(enableAA);
		ShowFlags.SetToneCurve(true);
		ShowFlags.SetTonemapper(true);
		ShowFlags.SetColorGrading(true);
		ShowFlags.SetEyeAdaptation(true);
		ShowFlags.SetCameraInterpolation(true);
		ShowFlags.SetMotionBlur(false);
		ShowFlags.SetLensFlares(true);
		ShowFlags.SetLightShafts(true);
		ShowFlags.SetDepthOfField(true);

		// ShowFlags.SetCameraImperfections(true);
		// ShowFlags.SetVignette(true);
		// ShowFlags.SetGrain(true);
		// ShowFlags.SetSeparateTranslucency(true);
		// ShowFlags.SetIndirectLightingCache(true);
		// ShowFlags.SetPostProcessMaterial(true);
	}

protected:
    AActor* InstalledActor = NULL;

};