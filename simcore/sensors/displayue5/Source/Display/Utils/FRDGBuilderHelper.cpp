// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/FRDGBuilderHelper.h"

#include "RenderGraphBuilder.h"

FRDGBufferUAVRef FRDGBuilderHelper::RegisterUAVBuffer(FRDGBuilder& GraphBuilder, const TRefCountPtr<FRDGPooledBuffer>& ExternalPooledBuffer)
{
	const FRDGBufferRef ExternalBuffer = GraphBuilder.RegisterExternalBuffer(
		ExternalPooledBuffer, ERDGBufferFlags::None);

	const FRDGBufferUAVDesc BufferDesc(ExternalBuffer);
	return GraphBuilder.CreateUAV(BufferDesc, ERDGUnorderedAccessViewFlags::None);
}

FRDGBufferSRVRef FRDGBuilderHelper::RegisterSRVBuffer(FRDGBuilder& GraphBuilder, const TRefCountPtr<FRDGPooledBuffer>& ExternalPooledBuffer)
{
	const FRDGBufferRef ExternalBuffer = GraphBuilder.RegisterExternalBuffer(
		ExternalPooledBuffer, ERDGBufferFlags::None);
	const FRDGBufferSRVDesc BufferDesc(ExternalBuffer);
	return GraphBuilder.CreateSRV(BufferDesc);
}