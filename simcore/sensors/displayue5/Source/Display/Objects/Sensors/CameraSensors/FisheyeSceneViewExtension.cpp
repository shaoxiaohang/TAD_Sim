#include "FisheyeSceneViewExtension.h"

#include "Engine/TextureRenderTarget2D.h"
#include "FisheyeSensor.h"
#include "RHIGPUReadback.h"
#include "RenderGraphUtils.h"
#include "Runtime/Renderer/Private/SceneRendering.h"
#include "Utils/FRDGBuilderHelper.h"
#include "WorldXShaders/Private/DistortionUVMappingCSInstance.h"
#include "WorldXShaders/Private/FullscreenTexturePassInstance.h"

FFisheyeSceneViewExtension::FFisheyeSceneViewExtension(AFisheyeSensor* SceneCaptureCamera)
{
    // Description width height may differ with CaptureRenderTarget when SourceImageScaleFactor is not 1
    Width = SceneCaptureCamera->imageRes.X;
    Height = SceneCaptureCamera->imageRes.Y;
    PixelFormat = SceneCaptureCamera->CaptureRenderTarget->GetFormat();
    SourceImageScaleFactor = SceneCaptureCamera->SourceImageScaleFactor;
    bIsRGB = SceneCaptureCamera->bIsRGB;
    UE_LOG(LogTemp, Display, TEXT("RDGTextureRef Width: %d Height: %d PixelFormat: %s"), Width, Height,
        GPixelFormats[PixelFormat].Name);
    FlushRenderingCommands();
    ImageDataBufferReadBack = MakeShared<FRHIGPUBufferReadback>(TEXT("Fisheye.Reackback.OutputImage"));
    DataNumBytes = Width * Height * GPixelFormats[PixelFormat].BlockBytes;
    ENQUEUE_RENDER_COMMAND(FImageDataPooledBufferSetup)
    ([&](FRHICommandListImmediate& RHICommandList) {
        ImageDataRDGPooledBuffer =
            AllocatePooledBuffer(FRDGBufferDesc::CreateByteAddressDesc(DataNumBytes), TEXT("Fisheye.OutputImage"));
        {
            const int BytesNum = SceneCaptureCamera->UVMapping.Num() * sizeof(FVector2f);
            UVMappingPooledBuffer =
                AllocatePooledBuffer(FRDGBufferDesc::CreateByteAddressDesc(BytesNum), TEXT("Fisheye.UVMapping"));
            void* TempBuffer = RHICommandList.LockBuffer(UVMappingPooledBuffer->GetRHI(), 0, BytesNum, RLM_WriteOnly);
            FMemory::Memcpy(TempBuffer, SceneCaptureCamera->UVMapping.GetData(), BytesNum);
            RHICommandList.UnlockBuffer(UVMappingPooledBuffer->GetRHI());
        }

        {
            const int BytesNum = SceneCaptureCamera->UVCameraMapping.Num() * sizeof(unsigned int);
            UVCameraMappingPooledBuffer = AllocatePooledBuffer(
                FRDGBufferDesc::CreateByteAddressDesc(BytesNum), TEXT("Fisheye.FishEye.UVCameraMapping"));
            void* TempBuffer =
                RHICommandList.LockBuffer(UVCameraMappingPooledBuffer->GetRHI(), 0, BytesNum, RLM_WriteOnly);
            FMemory::Memcpy(TempBuffer, SceneCaptureCamera->UVCameraMapping.GetData(), BytesNum);
            RHICommandList.UnlockBuffer(UVCameraMappingPooledBuffer->GetRHI());
        }
    });
    FlushRenderingCommands();
    ImageData.Reserve(DataNumBytes);
}

FFisheyeSceneViewExtension::~FFisheyeSceneViewExtension()
{
}
void FFisheyeSceneViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
}
void FFisheyeSceneViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
}
void FFisheyeSceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FFisheyeSceneViewExtension::PostRenderViewFamily_RenderThread(
    FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
    const FViewFamilyInfo* ViewFamilyInfo = reinterpret_cast<FViewFamilyInfo*>(&InViewFamily);
    const FRDGTextureRef RDGTextureRef = ViewFamilyInfo->RenderTarget->GetRenderTargetTexture(GraphBuilder);
    const FRDGBufferUAVRef ImageDataUAV = FRDGBuilderHelper::RegisterUAVBuffer(GraphBuilder, ImageDataRDGPooledBuffer);
    const FRDGBufferSRVRef UVMappingSrv = FRDGBuilderHelper::RegisterSRVBuffer(GraphBuilder, UVMappingPooledBuffer);
    const FRDGBufferSRVRef UVCameraMappingSRV =
        FRDGBuilderHelper::RegisterSRVBuffer(GraphBuilder, UVCameraMappingPooledBuffer);

    FRDGTextureSRVRef MainTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc(RDGTextureRef));

    // debug visualize pass

    // {
    // 	int ViewportWidth = 800;
    // 	int ViewportHeight = ViewportWidth * Height / Width;
    // 	int specator = 100;
    // 	int StartX = 10 +  (ViewportWidth + specator) * CameraIndex;
    // 	int StartY = 10 + ViewportHeight;
    // 	FullscreenTexturePassInstance::FParameters Parameters;
    // 	Parameters.InTexture = MainTexture;
    // 	Parameters.ViewPort = FIntRect(StartX, StartY ,  StartX + ViewportWidth, StartY + ViewportHeight);
    // 	FullscreenTexturePassInstance::Get()->AddPass(GraphBuilder, Parameters);
    // }

    if (CameraIndex == 0)
    {
        AddClearUAVPass(GraphBuilder, ImageDataUAV, 0);
    }

    if(bIsRGB)
    {
        // distort
        FDistortionUVMappingCSInstance::FFisheyeParameters Parameters;
        Parameters.Width = Width;
        Parameters.Height = Height;
        Parameters.ScaleFactor = SourceImageScaleFactor;
        Parameters.InTexture = MainTexture;
        Parameters.OutTexture = ImageDataUAV;
        Parameters.UVMapping = UVMappingSrv;
        Parameters.CameraIndex = CameraIndex;
        Parameters.UVCameraMapping = UVCameraMappingSRV;
        FDistortionUVMappingCSInstance::Dispatch(GraphBuilder, Parameters);
        //UE_LOG(LogTemp, Display, TEXT("FisheyeSceneViewExtension: RGB"));
    }
    else
    {
        // distort
        FDistortionUVMappingCSInstance::FNormalParameters Parameters;
        Parameters.Width = Width;
        Parameters.Height = Height;
        Parameters.ScaleFactor = SourceImageScaleFactor;
        Parameters.InTexture = MainTexture;
        Parameters.OutTexture = ImageDataUAV;
        Parameters.UVMapping = UVMappingSrv;
        Parameters.CameraIndex = CameraIndex;
        Parameters.UVCameraMapping = UVCameraMappingSRV;
        FDistortionUVMappingCSInstance::Dispatch(GraphBuilder, Parameters);
        //UE_LOG(LogTemp, Display, TEXT("FisheyeSceneViewExtension: Normal"));
    }


    // {
    //     // distort
    //     FDistortionUVMappingCSInstance::FNormalParameters Parameters;
    //     Parameters.Width = Width;
    //     Parameters.Height = Height;
    //     Parameters.ScaleFactor = SourceImageScaleFactor;
    //     Parameters.InTexture = MainTexture;
    //     Parameters.OutTexture = ImageDataUAV;
    //     Parameters.UVMapping = UVMappingSrv;
    //     FDistortionUVMappingCSInstance::Dispatch(GraphBuilder, Parameters);
    // }

    if (CameraIndex == 4)
    {
        AddEnqueueCopyPass(GraphBuilder, ImageDataBufferReadBack.Get(), ImageDataUAV->GetParent(), DataNumBytes);
    }

    CameraIndex++;
}

TArray<uint8>& FFisheyeSceneViewExtension::GetReadbackData()
{
    ENQUEUE_RENDER_COMMAND(FGetUAV_RenderThread)
    ([&](FRHICommandListImmediate& RHICommandList) {
        RHICommandList.BlockUntilGPUIdle();
        ImageData.SetNum(0);

        if (ImageDataBufferReadBack->IsReady())
        {
            ImageData.SetNum(DataNumBytes);
            const void* Data = ImageDataBufferReadBack->Lock(DataNumBytes);
            FMemory::Memcpy(ImageData.GetData(), Data, DataNumBytes);
            ImageDataBufferReadBack->Unlock();
        }
    });

    FlushRenderingCommands();
    CameraIndex = 0;
    return ImageData;
}
