#pragma once

#include "CoreMinimal.h"

class WORLDXSHADERS_API FTexture2DCopyCSInstance
{
public:
	//Get the instance
	static FTexture2DCopyCSInstance* Get()
	{
		if (!Instance)
			Instance = new FTexture2DCopyCSInstance();
		return Instance;
	}

	struct FCopyBGRAParameters
	{
		int Width;
		int Height;
		FRDGTextureSRVRef InTexture;
		FRDGBufferUAVRef OutRawTextureUint;
	};

	struct FCopyBGRParameters
	{
		int Width;
		int Height;
		FRDGTextureSRVRef InTexture;
		FRDGBufferUAVRef OutRawTextureByte;
	};
	
	void DispatchCopyBGRA(FRDGBuilder& GraphBuilder, const FCopyBGRAParameters& Parameters);
	void DispatchCopyBGR(FRDGBuilder& GraphBuilder, const FCopyBGRParameters& Parameters);
	
private:

	FTexture2DCopyCSInstance() = default;
	
	static FTexture2DCopyCSInstance* Instance;

};