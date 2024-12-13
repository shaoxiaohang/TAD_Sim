// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class DISPLAY_API FRDGBuilderHelper
{
public:
	static FRDGBufferUAVRef RegisterUAVBuffer(FRDGBuilder& GraphBuilder, const TRefCountPtr<FRDGPooledBuffer>& ExternalPooledBuffer);
	
	static FRDGBufferSRVRef RegisterSRVBuffer(FRDGBuilder& GraphBuilder, const TRefCountPtr<FRDGPooledBuffer>& ExternalPooledBuffer);

};
