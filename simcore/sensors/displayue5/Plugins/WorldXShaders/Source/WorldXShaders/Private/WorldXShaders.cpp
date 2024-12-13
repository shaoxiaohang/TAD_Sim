// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldXShaders.h"

#include "AssetRegistry/AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "FWorldXShadersModule"

void FWorldXShadersModule::StartupModule()
{
    // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FString ShaderDirectory = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("WorldXShaders/Shaders/Private"));
	AddShaderSourceDirectoryMapping("/WorldXShaders", ShaderDirectory);
}

void FWorldXShadersModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FWorldXShadersModule, WorldXShaders)
