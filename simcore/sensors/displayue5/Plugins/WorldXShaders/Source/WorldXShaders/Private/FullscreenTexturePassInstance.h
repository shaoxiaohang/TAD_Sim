// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class WORLDXSHADERS_API FullscreenTexturePassInstance
{
public:
	//Get the instance
	static FullscreenTexturePassInstance* Get()
	{
		if (!Instance)
			Instance = new FullscreenTexturePassInstance();
		return Instance;
	}
	
	struct FParameters
	{
		FIntRect ViewPort;
		FRDGTextureSRVRef InTexture;
	};

	void AddPass(FRDGBuilder& GraphBuilder, const FParameters& Parameters);

private:
	static FullscreenTexturePassInstance* Instance;
};
