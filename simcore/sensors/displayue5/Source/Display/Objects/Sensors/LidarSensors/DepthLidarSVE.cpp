#include "DepthLidarSVE.h"

#include "Objects/Sensors/LidarSensors/TLidarBufferDepth.h"
#include "Runtime/Renderer/Private/SceneRendering.h"
#include "Utils/FRDGBuilderHelper.h"
#include "WorldXShaders/Private/DepthBasedLidarCSInstance.h"

FDepthMapBasedLidarSceneViewExtension::FDepthMapBasedLidarSceneViewExtension(
    ALidarBufferDepth* InDepthMapBasedLidar, bool UseSingleCapture)
    : DepthMapLidar(InDepthMapBasedLidar), bUseSingleCapture(UseSingleCapture)
{
}

FDepthMapBasedLidarSceneViewExtension::~FDepthMapBasedLidarSceneViewExtension()
{
}

void FDepthMapBasedLidarSceneViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
}
void FDepthMapBasedLidarSceneViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
}
void FDepthMapBasedLidarSceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FDepthMapBasedLidarSceneViewExtension::PostRenderViewFamily_RenderThread(
    FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
    if (bUseSingleCapture)
    {
        SingleCaptureLidar(GraphBuilder, InViewFamily);
    }
    else
    {
        MultipleCaptureLidar(GraphBuilder, InViewFamily);
    }
}

void FDepthMapBasedLidarSceneViewExtension::ResetParams()
{
    CameraIndex = 0;
}

void FDepthMapBasedLidarSceneViewExtension::SingleCaptureLidar(
    FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
    ALidarBufferDepth::ELidarPassType LidarPassType = ALidarBufferDepth::ELidarPassType::BasePass;
    if (CameraIndex == DepthMapLidar->CameraCount - 1)
    {
        LidarPassType = ALidarBufferDepth::ELidarPassType::All;
    }

    ALidarBufferDepth::FLidarPassParams PassParams = DepthMapLidar->CreateLidarPassParams(GraphBuilder, LidarPassType);

    // using actomic
    if (CameraIndex == 0)
    {
        // clear counter
        AddClearUAVPass(GraphBuilder, PassParams.LaserNumPerScanUAV, 0);
        AddClearUAVPass(GraphBuilder, PassParams.DetectionCountUAV, 0);
    }

    // rendet target
    auto RenderTargetInfoItem = DepthMapLidar->RenderTargets[0];
    FRDGTextureSRVDesc SRVDesc(GraphBuilder.RegisterExternalTexture(RenderTargetInfoItem.pooled_target));
    DepthMapLidar->AddLidarBasePass(GraphBuilder, CameraIndex, GraphBuilder.CreateSRV(SRVDesc), PassParams);

    // last capture
    if (CameraIndex == DepthMapLidar->CameraCount - 1)
    {
        DepthMapLidar->AddLidarPostPass(GraphBuilder, PassParams);
    }

    CameraIndex++;
}
void FDepthMapBasedLidarSceneViewExtension::MultipleCaptureLidar(
    FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
    ALidarBufferDepth::FLidarPassParams PassParams =
        DepthMapLidar->CreateLidarPassParams(GraphBuilder, ALidarBufferDepth::ELidarPassType::All);

    // clear counter
    AddClearUAVPass(GraphBuilder, PassParams.LaserNumPerScanUAV, 0);
    AddClearUAVPass(GraphBuilder, PassParams.DetectionCountUAV, 0);

    for (int i = 0; i < DepthMapLidar->CameraCount; i++)
    {
        // rendet target
        auto RenderTargetInfoItem = DepthMapLidar->RenderTargets[i];
        FRDGTextureSRVDesc SRVDesc(GraphBuilder.RegisterExternalTexture(RenderTargetInfoItem.pooled_target));
        DepthMapLidar->AddLidarBasePass(GraphBuilder, i, GraphBuilder.CreateSRV(SRVDesc), PassParams);
    }
    DepthMapLidar->AddLidarPostPass(GraphBuilder, PassParams);
}
