#include "DistortionUVMappingCSInstance.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

class FFisheyeUVMappingCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFisheyeUVMappingCS);

	SHADER_USE_PARAMETER_STRUCT(FFisheyeUVMappingCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int, Width)
		SHADER_PARAMETER(int, Height)
		SHADER_PARAMETER(int, ScaleFactor)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<FVector4f>, InTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InTextureSampler)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<unsigned int>, OutTexture)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVector2f>, UVMapping)
		SHADER_PARAMETER(int, CameraIndex)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<unsigned int>, UVCameraMapping)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		//We're using it here to add some preprocessor defines. That way we don't have to change both C++ and HLSL code when we change the value for NUM_THREADS_PER_GROUP_DIMENSION
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), 32);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), 32);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Z"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FFisheyeUVMappingCS, "/WorldXShaders/DistortionUVMappingCS.usf", "FisheyeMain", SF_Compute);

class FNormalUVMappingCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FNormalUVMappingCS);

	SHADER_USE_PARAMETER_STRUCT(FNormalUVMappingCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int, Width)
		SHADER_PARAMETER(int, Height)
		SHADER_PARAMETER(int, ScaleFactor)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<FVector4f>, InTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InTextureSampler)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<unsigned int>, OutTexture)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVector2f>, UVMapping)
		SHADER_PARAMETER(int, CameraIndex)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<unsigned int>, UVCameraMapping)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		//We're using it here to add some preprocessor defines. That way we don't have to change both C++ and HLSL code when we change the value for NUM_THREADS_PER_GROUP_DIMENSION
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), 32);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), 32);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Z"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FNormalUVMappingCS, "/WorldXShaders/DistortionUVMappingCS.usf", "NormalMain", SF_Compute);


void FDistortionUVMappingCSInstance::Dispatch(FRDGBuilder& GraphBuilder, const FFisheyeParameters& Parameters)
{
	FFisheyeUVMappingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFisheyeUVMappingCS::FParameters>();
	PassParameters->Width = Parameters.Width;
	PassParameters->Height = Parameters.Height;
	PassParameters->ScaleFactor = Parameters.ScaleFactor;
	PassParameters->InTexture = Parameters.InTexture;
	PassParameters->InTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->OutTexture = Parameters.OutTexture;
	PassParameters->UVMapping = Parameters.UVMapping;
	PassParameters->CameraIndex = Parameters.CameraIndex;
	PassParameters->UVCameraMapping = Parameters.UVCameraMapping;
	const TShaderMapRef<FFisheyeUVMappingCS> ComputeShaderMapRef(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	const FIntVector GroupCount = FIntVector(FMath::DivideAndRoundUp(Parameters.Width, 32), FMath::DivideAndRoundUp(Parameters.Height, 32), 1);
	FComputeShaderUtils::AddPass<FFisheyeUVMappingCS>(GraphBuilder, RDG_EVENT_NAME("FisheyeUVMapping"), ERDGPassFlags::Compute, ComputeShaderMapRef, PassParameters, GroupCount);
}

void FDistortionUVMappingCSInstance::Dispatch(FRDGBuilder& GraphBuilder, const FNormalParameters& Parameters)
{
	FNormalUVMappingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FNormalUVMappingCS::FParameters>();
	PassParameters->Width = Parameters.Width;
	PassParameters->Height = Parameters.Height;
	PassParameters->ScaleFactor = Parameters.ScaleFactor;
	PassParameters->InTexture = Parameters.InTexture;
	PassParameters->InTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->OutTexture = Parameters.OutTexture;
	PassParameters->UVMapping = Parameters.UVMapping;
	PassParameters->CameraIndex = Parameters.CameraIndex;
	PassParameters->UVCameraMapping = Parameters.UVCameraMapping;
	const TShaderMapRef<FNormalUVMappingCS> ComputeShaderMapRef(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	const FIntVector GroupCount = FIntVector(FMath::DivideAndRoundUp(Parameters.Width, 32), FMath::DivideAndRoundUp(Parameters.Height, 32), 1);
	FComputeShaderUtils::AddPass<FNormalUVMappingCS>(GraphBuilder, RDG_EVENT_NAME("NormalUVMapping"), ERDGPassFlags::Compute, ComputeShaderMapRef, PassParameters, GroupCount);
}

// void FDistortionUVMappingCSInstance::Dispatch(FRDGBuilder& GraphBuilder, const FNormalParameters& Parameters)
// {
// 	FNormalUVMappingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FNormalUVMappingCS::FParameters>();
// 	PassParameters->Width = Parameters.Width;
// 	PassParameters->Height = Parameters.Height;
// 	PassParameters->ScaleFactor = Parameters.ScaleFactor;
// 	PassParameters->InTexture = Parameters.InTexture;
// 	PassParameters->InTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
// 	PassParameters->OutTexture = Parameters.OutTexture;
// 	PassParameters->UVMapping = Parameters.UVMapping;
// 	const TShaderMapRef<FNormalUVMappingCS> ComputeShaderMapRef(GetGlobalShaderMap(GMaxRHIFeatureLevel));
// 	const FIntVector GroupCount = FIntVector(FMath::DivideAndRoundUp(Parameters.Width, 32), FMath::DivideAndRoundUp(Parameters.Height, 32), 1);
// 	FComputeShaderUtils::AddPass<FNormalUVMappingCS>(GraphBuilder, RDG_EVENT_NAME("NormalUVMapping"), ERDGPassFlags::Compute, ComputeShaderMapRef, PassParameters, GroupCount);
// }

