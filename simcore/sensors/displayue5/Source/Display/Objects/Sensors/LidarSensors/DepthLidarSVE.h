// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"

class ALidarBufferDepth;

/**
 * 
 */
class DISPLAY_API FDepthMapBasedLidarSceneViewExtension : public ISceneViewExtension
{
public:
	FDepthMapBasedLidarSceneViewExtension(ALidarBufferDepth* InDepthMapBasedLidar, bool UseSingleCapture);
	virtual ~FDepthMapBasedLidarSceneViewExtension() override;

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;

	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;

	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;

	virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;

	void ResetParams();
	
protected:

	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override
	{
		return true;
	}

private:

	void SingleCaptureLidar(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily);

	void MultipleCaptureLidar(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily);
	
	int CameraIndex = 0;

	ALidarBufferDepth* DepthMapLidar;
	bool bUseSingleCapture;
};
