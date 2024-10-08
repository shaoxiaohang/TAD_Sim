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
		 "InputCore", "EnhancedInput", "AutoRoad","RuntimeMeshLoader"});

		PrivateDependencyModuleNames.AddRange(new string[] {  });

    //Plugin module
    PublicDependencyModuleNames.AddRange(new string[]
    		{"HadMap","Protobuf","BoostLib","MyUDP"});


    RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "../../Config", "DefaultAutoRoad.ini"));
	
    if (Target.Platform == UnrealTargetPlatform.Win64){
        PublicDependencyModuleNames.AddRange(new string[] { "RTXLidar" });
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
