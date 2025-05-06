// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class Display : ModuleRules
{
	public Display(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "RenderCore", "CoreUObject", "RHI",
		 "InputCore", "EnhancedInput","ImageWrapper","ProceduralMeshComponent", "OpenCV","OpenCVHelper","CinematicCamera","Networking", "Sockets", "AutoRoad","RuntimeMeshLoader"});

			PrivateIncludePaths.AddRange(
				new string[]
				{
					System.IO.Path.Combine(GetModuleDirectory("Renderer"), "Private"),
				}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Json",
				"JsonUtilities",
				"Slate",
				"SlateCore",
				"WorldXShaders",
				"Renderer",
				"UMG",
				"Eigen",
				"CustomMeshComponent"
			}
			);

    //Plugin module
    PublicDependencyModuleNames.AddRange(new string[]
    		{"HadMap","Protobuf","BoostLib","MyUDP", "CudaResource"});


    PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "SimMsg"));

    PublicIncludePaths.Add(ModuleDirectory);

    PublicDefinitions.Add("EIGEN_MPL2_ONLY");

    RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "../../Config", "DefaultAutoRoad.ini"));

	string UE_ROOT = Environment.GetEnvironmentVariable("UE5_ROOT");

	string OPENCV_PATH = Path.Combine(UE_ROOT, "Engine/Plugins/Runtime/OpenCV/Binaries/ThirdParty/Linux");
	
	RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)/ubuntu18_20/", "libopencv_world.so.405"),
		Path.Combine(OPENCV_PATH, "libopencv_world.so.405"));

    if (Target.Platform == UnrealTargetPlatform.Win64){
        PublicDependencyModuleNames.AddRange(new string[] { "RTXLidar" });
		}

    bEnableExceptions = true;

	}
}
