#pragma once

#include "CoreMinimal.h"

class WORLDXSHADERS_API FDistortionUVMappingCSInstance
{
public:
	struct FFisheyeParameters
	{
		int Width;
		int Height;
		int ScaleFactor;
		FRDGTextureSRVRef InTexture;
		FRDGBufferUAVRef OutTexture;
		FRDGBufferSRVRef UVMapping;
		int CameraIndex;
		FRDGBufferSRVRef UVCameraMapping;

	};
	static  void Dispatch(FRDGBuilder& GraphBuilder, const FFisheyeParameters& Parameters);

	// struct FNormalParameters
	// {
	// 	int Width;
	// 	int Height;
	// 	int ScaleFactor;
	// 	FRDGTextureSRVRef InTexture;
	// 	FRDGBufferUAVRef OutTexture;
	// 	FRDGBufferSRVRef UVMapping;
	// };
	// static void Dispatch(FRDGBuilder& GraphBuilder, const FNormalParameters& Parameters);

	struct FNormalParameters
	{
		int Width;
		int Height;
		int ScaleFactor;
		FRDGTextureSRVRef InTexture;
		FRDGBufferUAVRef OutTexture;
		FRDGBufferSRVRef UVMapping;
		int CameraIndex;
		FRDGBufferSRVRef UVCameraMapping;
	};
	static void Dispatch(FRDGBuilder& GraphBuilder, const FNormalParameters& Parameters);
};