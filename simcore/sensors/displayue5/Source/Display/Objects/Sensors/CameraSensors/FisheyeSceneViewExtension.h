#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"

class FRHIGPUBufferReadback;
class AFisheyeSensor;

class DISPLAY_API FFisheyeSceneViewExtension : public ISceneViewExtension
{
public:
    FFisheyeSceneViewExtension(AFisheyeSensor* SceneCaptureCamera);
    virtual ~FFisheyeSceneViewExtension() override;

    virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;

    virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;

    virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;

    virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;

    TArray<uint8>& GetReadbackData();

private:
    int Width;
    int Height;
    EPixelFormat PixelFormat;
    float SourceImageScaleFactor;

    TSharedPtr<FRHIGPUBufferReadback> ImageDataBufferReadBack;
    TRefCountPtr<FRDGPooledBuffer> ImageDataRDGPooledBuffer;
    TArray<uint8> ImageData;
    int DataNumBytes = 0;

    TRefCountPtr<FRDGPooledBuffer> UVMappingPooledBuffer;
    TRefCountPtr<FRDGPooledBuffer> UVCameraMappingPooledBuffer;

    TRefCountPtr<FRDGPooledBuffer> CameraPatchesPooledBuffer;

    int CameraIndex = 0;

    bool bIsRGB = true;
};
