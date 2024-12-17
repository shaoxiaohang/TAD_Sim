#pragma once

#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h"

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"
//
// class FDepthBasedMetaLidarCS : public FGlobalShader
// {
// public:
//     DECLARE_GLOBAL_SHADER(FDepthBasedMetaLidarCS);
//
//     SHADER_USE_PARAMETER_STRUCT(FDepthBasedMetaLidarCS, FGlobalShader);
//
//     BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
// 		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<FVector4f>, InTexture)
// 		SHADER_PARAMETER(FVector2f, Dimensions)
// 		SHADER_PARAMETER(FVector4f, Params0)
// 		SHADER_PARAMETER(FVector4f, Params1)
// 		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, RawHitBuffer)
// 		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<int>, LaserNumPerScan)
// 	END_SHADER_PARAMETER_STRUCT()
//     
// public:
//
//     static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
// 	{
// 		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
// 	}
//
//     static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
// 	{
// 		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
//
// 		//We're using it here to add some preprocessor defines. That way we don't have to change both C++ and HLSL code when we change the value for NUM_THREADS_PER_GROUP_DIMENSION
// 		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), 32);
// 		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), 32);
// 		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Z"), 1);
// 	}
// };


class FDepthBasedLidarRawHitCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FDepthBasedLidarRawHitCS);

    SHADER_USE_PARAMETER_STRUCT(FDepthBasedLidarRawHitCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int, ChannelCount)
		SHADER_PARAMETER(int, HorizonCount)
		SHADER_PARAMETER(int, CameraIndex)
		SHADER_PARAMETER(float, CosAzimuth)
		SHADER_PARAMETER(float, SinAzimuth)
		SHADER_PARAMETER(float, Range)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, RawHitBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<int>, LaserNumPerScan)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<FVector4f>, InTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<FVector4f>, InTextureExtra)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float>, ImageSpaceLaserRays)
	END_SHADER_PARAMETER_STRUCT()
    
public:

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

    static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		//We're using it here to add some preprocessor defines. That way we don't have to change both C++ and HLSL code when we change the value for NUM_THREADS_PER_GROUP_DIMENSION
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), 32);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), 32);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Z"), 1);
	}
};


class FDepthBasedLidarScanCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FDepthBasedLidarScanCS);

	SHADER_USE_PARAMETER_STRUCT(FDepthBasedLidarScanCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int, ScanCount)
		SHADER_PARAMETER(int, ScanSequenceOffset)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<int>, DetectionCount)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<int>, ScanOffsetUAV)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float>, ScanAzimuth)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<int>, LaserNumPerScan)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, ScanBufferUAV)
	END_SHADER_PARAMETER_STRUCT()
    
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		//We're using it here to add some preprocessor defines. That way we don't have to change both C++ and HLSL code when we change the value for NUM_THREADS_PER_GROUP_DIMENSION
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), 32);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), 1);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Z"), 1);
	}
};

class FDepthBasedLidarReorderCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FDepthBasedLidarReorderCS);

	SHADER_USE_PARAMETER_STRUCT(FDepthBasedLidarReorderCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int, ScanCount)
		SHADER_PARAMETER(int, ChannelCount)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<int>, LaserNumPerScan)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<int>, ScanOffsetUAV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, RawHitBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, ReorderedLidarBuffer)
	END_SHADER_PARAMETER_STRUCT()
    
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		//We're using it here to add some preprocessor defines. That way we don't have to change both C++ and HLSL code when we change the value for NUM_THREADS_PER_GROUP_DIMENSION
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), 32);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), 32);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Z"), 1);
	}
};
