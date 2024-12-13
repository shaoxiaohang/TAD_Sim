// Fill out your copyright notice in the Description page of Project Settings.


#include "FullscreenTexturePassInstance.h"

#include "PixelShaderUtils.h"

class FullscreenTexturePass : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FullscreenTexturePass);

	SHADER_USE_PARAMETER_STRUCT(FullscreenTexturePass, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<FVector4f>, InTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FullscreenTexturePass, "/WorldXShaders/FullscreenTexture.usf", "FullscreenTexturePS", SF_Pixel);

FullscreenTexturePassInstance* FullscreenTexturePassInstance::Instance = nullptr;

void FullscreenTexturePassInstance::AddPass(FRDGBuilder& GraphBuilder, const FParameters& Parameters)
{
	auto* PassParameters = GraphBuilder.AllocParameters<FullscreenTexturePass::FParameters>();
	PassParameters->InTexture = Parameters.InTexture;
	PassParameters->TextureSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	const auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	const TShaderMapRef<FullscreenTexturePass> PixelShader(ShaderMap);

	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		ShaderMap,
		RDG_EVENT_NAME("FullscreenTexture"),
		PixelShader,
		PassParameters,
		Parameters.ViewPort);
}
