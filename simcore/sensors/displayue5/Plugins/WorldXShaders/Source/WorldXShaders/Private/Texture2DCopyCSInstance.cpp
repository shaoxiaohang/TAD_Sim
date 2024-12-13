#include "Texture2DCopyCSInstance.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

class FTexture2DCopyRGBACS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTexture2DCopyRGBACS);

	SHADER_USE_PARAMETER_STRUCT(FTexture2DCopyRGBACS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int, Width)
		SHADER_PARAMETER(int, Height)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<FVector4f>, InTexture)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<unsigned int>, OutRawTextureUint)
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
IMPLEMENT_GLOBAL_SHADER(FTexture2DCopyRGBACS, "/WorldXShaders/Texture2DCopyCS.usf", "CopyRGBAMain", SF_Compute);


class FTexture2DCopyBGRCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTexture2DCopyBGRCS);

	SHADER_USE_PARAMETER_STRUCT(FTexture2DCopyBGRCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int, Width)
		SHADER_PARAMETER(int, Height)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<FVector4f>, InTexture)
		// SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, OutRawTextureByte)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<unsigned int>, OutRawTextureByte)
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
IMPLEMENT_GLOBAL_SHADER(FTexture2DCopyBGRCS, "/WorldXShaders/Texture2DCopyCS.usf", "CopyRGBMain", SF_Compute);


FTexture2DCopyCSInstance* FTexture2DCopyCSInstance::Instance = nullptr;
void FTexture2DCopyCSInstance::DispatchCopyBGRA(FRDGBuilder& GraphBuilder, const FCopyBGRAParameters& Parameters)
{
	FTexture2DCopyRGBACS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTexture2DCopyRGBACS::FParameters>();
	PassParameters->Width = Parameters.Width;
	PassParameters->Height = Parameters.Height;
	PassParameters->InTexture = Parameters.InTexture;
	PassParameters->OutRawTextureUint = Parameters.OutRawTextureUint;

	const TShaderMapRef<FTexture2DCopyRGBACS> ComputeShaderMapRef(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	const FIntVector GroupCount = FIntVector(FMath::DivideAndRoundUp(Parameters.Width, 32), FMath::DivideAndRoundUp(Parameters.Height, 32), 1);

	FComputeShaderUtils::AddPass<FTexture2DCopyRGBACS>(GraphBuilder, RDG_EVENT_NAME("TextureCopy BGRA"), ERDGPassFlags::Compute, ComputeShaderMapRef, PassParameters, GroupCount);
}

void FTexture2DCopyCSInstance::DispatchCopyBGR(FRDGBuilder& GraphBuilder, const FCopyBGRParameters& Parameters)
{
	FTexture2DCopyBGRCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTexture2DCopyBGRCS::FParameters>();
	PassParameters->Width = Parameters.Width;
	PassParameters->Height = Parameters.Height;
	PassParameters->InTexture = Parameters.InTexture;
	PassParameters->OutRawTextureByte = Parameters.OutRawTextureByte;

	const TShaderMapRef<FTexture2DCopyBGRCS> ComputeShaderMapRef(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	const FIntVector GroupCount = FIntVector(FMath::DivideAndRoundUp(Parameters.Width, 32), FMath::DivideAndRoundUp(Parameters.Height, 32), 1);

	FComputeShaderUtils::AddPass<FTexture2DCopyBGRCS>(GraphBuilder, RDG_EVENT_NAME("TextureCopy BGR"), ERDGPassFlags::Compute, ComputeShaderMapRef, PassParameters, GroupCount);
}
