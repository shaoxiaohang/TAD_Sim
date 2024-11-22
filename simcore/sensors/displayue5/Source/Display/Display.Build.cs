// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class Display : ModuleRules
{
	public Display(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine",
		 "InputCore", "EnhancedInput","ProceduralMeshComponent","CinematicCamera","Networking", "Sockets", "AutoRoad","RuntimeMeshLoader"});

		PrivateDependencyModuleNames.AddRange(new string[] { "UMG"  });

    //Plugin module
    PublicDependencyModuleNames.AddRange(new string[]
    		{"HadMap","Protobuf","BoostLib","MyUDP", "CudaResource"});


    PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "SimMsg"));

    PublicIncludePaths.Add(ModuleDirectory);

    RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "../../Config", "DefaultAutoRoad.ini"));
	
    if (Target.Platform == UnrealTargetPlatform.Win64){
        PublicDependencyModuleNames.AddRange(new string[] { "RTXLidar" });
		}

    bEnableExceptions = true;

	}
}
