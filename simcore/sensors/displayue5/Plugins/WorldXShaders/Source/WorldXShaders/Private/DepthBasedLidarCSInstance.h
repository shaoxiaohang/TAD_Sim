#pragma once
#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h"
#include "DepthBasedLidarCSDeclaration.h"
#include "RenderGraphResources.h"
#include "RenderGraphBuilder.h"

/// <summary>
/// A singleton Shader Manager for our Shader Type
/// </summary>
class WORLDXSHADERS_API FDepthBasedLidarCSInstance
{
public:
	struct FRawHitParameters
	{
		int ChannelCount;
		int HorizonCount;
		int CameraIndex;
		float CosAzimuth;
		float SinAzimuth;
		float Range;
		FRDGBufferUAVRef RawHitBuffer;
		FRDGBufferUAVRef LaserNumPerScan;
		FRDGTextureSRVRef InTexture;
		FRDGTextureSRVRef InTextureExtra;
		FRDGBufferSRVRef ImageSpaceLaserRays;
	};

	struct FScanParameters
	{
		int ScanCount;
		int ScanSequenceOffset;
		FRDGBufferUAVRef DetectionCount;
		FRDGBufferUAVRef ScanOffsetUAV;
		FRDGBufferSRVRef ScanAzimuth;
		FRDGBufferUAVRef LaserNumPerScan;
		FRDGBufferUAVRef ScanBuffer;
	};

	struct FReorderParameters
	{
		int ScanCount;
		int ChannelCount;
		FRDGBufferUAVRef LaserNumPerScan;
		FRDGBufferUAVRef ScanOffsetUAV;
		FRDGBufferUAVRef RawHitBuffer;
		FRDGBufferUAVRef ReorderedLidarBuffer;
	};


	//Get the instance
	static FDepthBasedLidarCSInstance* Get()
	{
		if (!instance)
			instance = new FDepthBasedLidarCSInstance();
		return instance;
	};
	
	// meta depth based points
	// void DispatchMetaLidar(FRHICommandListImmediate& RHICmdList, FDepthBasedLidarCSParameters& parameters);

	// // sample points
	// void DispatchLidar(FRHICommandListImmediate& RHICmdList, FDepthBasedLidarCSParameters& parameters);
	
	void GraphBuilderDispatchLidar(FRDGBuilder& GraphBuilder, FRawHitParameters& parameters);
	void GraphBuilderDispatchLidar(FRDGBuilder& GraphBuilder, FScanParameters& parameters);
	void GraphBuilderDispatchLidar(FRDGBuilder& GraphBuilder, FReorderParameters& parameters);
private:
	//Private constructor to prevent client from instanciating
	FDepthBasedLidarCSInstance() = default;

	//The singleton instance
	static FDepthBasedLidarCSInstance* instance;
};
