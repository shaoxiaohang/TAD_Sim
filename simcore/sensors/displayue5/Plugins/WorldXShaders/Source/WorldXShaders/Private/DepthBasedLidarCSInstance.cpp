#include "DepthBasedLidarCSInstance.h"

FDepthBasedLidarCSInstance* FDepthBasedLidarCSInstance::instance = nullptr;

// void FDepthBasedLidarCSInstance::DispatchMetaLidar(FRHICommandListImmediate& RHICmdList,
// FDepthBasedLidarCSParameters& parameters)
// {
//     // compute shader
//     FDepthBasedMetaLidarCS::FParameters PassParameters;
//     PassParameters.InTexture = parameters.InTexture;
//     PassParameters.Dimensions = parameters.Dimensions;
//     PassParameters.Params0 = parameters.Params0;
//     PassParameters.Params1 = parameters.Params1;
//     PassParameters.OutputBuffer = parameters.OutputStructureBuffer;
//     PassParameters.OutputBufferCounter = parameters.OutputCountStructureBuffer;

//     //Get a reference to our shader type from global shader map
//     TShaderMapRef<FDepthBasedMetaLidarCS> depthBasedLidarCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
//     int width = parameters.Dimensions.X;
//     int height = parameters.Dimensions.Y;
//     // FIntVector threadRange(FIntVector(FMath::DivideAndRoundUp(height, 32), FMath::DivideAndRoundUp(height, 32),
//     1)); FIntVector groupCount =  FIntVector(FMath::DivideAndRoundUp(width, 32), FMath::DivideAndRoundUp(height, 32),
//     1);

//     //Dispatch the compute shader
//     FComputeShaderUtils::Dispatch(RHICmdList,
//                                     depthBasedLidarCS,
//                                     PassParameters,
//                                     groupCount);
// }

// void FDepthBasedLidarCSInstance::DispatchLidar(FRHICommandListImmediate& RHICmdList, FDepthBasedLidarCSParameters&
// parameters)
// {
//     // compute shader
//     FDepthBasedLidarCS::FParameters PassParameters;
//     PassParameters.InTexture = parameters.InTexture;
//     PassParameters.LidarCorrection = parameters.LidarCorrection;
//     PassParameters.Dimensions = parameters.Dimensions;
//     PassParameters.Params0 = parameters.Params0;
//     PassParameters.Params1 = parameters.Params1;
//     PassParameters.OutputBuffer = parameters.OutputStructureBuffer;
//     PassParameters.OutputBufferCounter = parameters.OutputCountStructureBuffer;

//     //Get a reference to our shader type from global shader map
//     TShaderMapRef<FDepthBasedLidarCS> depthBasedLidarCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
//     int horizon_count = parameters.Params1.Y;
//     int channel = parameters.Params1.X;

//     // FIntVector threadRange(FIntVector(FMath::DivideAndRoundUp(horizon_count, 32), FMath::DivideAndRoundUp(height,
//     32), 1)); FIntVector groupCount = FIntVector(FMath::DivideAndRoundUp(horizon_count, 32),
//     FMath::DivideAndRoundUp(channel, 32), 1);

//     //Dispatch the compute shader
//     FComputeShaderUtils::Dispatch(RHICmdList,
//                                     depthBasedLidarCS,
//                                     PassParameters,
//                                     groupCount);
// }

void FDepthBasedLidarCSInstance::GraphBuilderDispatchLidar(FRDGBuilder& GraphBuilder, FRawHitParameters& parameters)
{
    FDepthBasedLidarRawHitCS::FParameters* PassParameters;
    PassParameters = GraphBuilder.AllocParameters<FDepthBasedLidarRawHitCS::FParameters>();
    PassParameters->ChannelCount = parameters.ChannelCount;
    PassParameters->HorizonCount = parameters.HorizonCount;
    PassParameters->CameraIndex = parameters.CameraIndex;
    PassParameters->CosAzimuth = parameters.CosAzimuth;
    PassParameters->SinAzimuth = parameters.SinAzimuth;
    PassParameters->Range = parameters.Range;
    PassParameters->RawHitBuffer = parameters.RawHitBuffer;
    PassParameters->LaserNumPerScan = parameters.LaserNumPerScan;
    PassParameters->InTexture = parameters.InTexture;
		PassParameters->InTextureExtra = parameters.InTextureExtra;
    PassParameters->ImageSpaceLaserRays = parameters.ImageSpaceLaserRays;

    TShaderMapRef<FDepthBasedLidarRawHitCS> depthBasedLidarCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));

    // FIntVector threadRange(FIntVector(FMath::DivideAndRoundUp(horizon_count, 32), FMath::DivideAndRoundUp(height,
    // 32), 1));
    FIntVector groupCount = FIntVector(
        FMath::DivideAndRoundUp(parameters.HorizonCount, 32), FMath::DivideAndRoundUp(parameters.ChannelCount, 32), 1);

    FComputeShaderUtils::AddPass<FDepthBasedLidarRawHitCS>(GraphBuilder, RDG_EVENT_NAME("Lidar RawHit Pass"),
        ERDGPassFlags::Compute, depthBasedLidarCS, PassParameters, groupCount);
}

void FDepthBasedLidarCSInstance::GraphBuilderDispatchLidar(FRDGBuilder& GraphBuilder, FScanParameters& parameters)
{
    FDepthBasedLidarScanCS::FParameters* PassParameters;
    PassParameters = GraphBuilder.AllocParameters<FDepthBasedLidarScanCS::FParameters>();
    PassParameters->ScanCount = parameters.ScanCount;
    PassParameters->DetectionCount = parameters.DetectionCount;
    PassParameters->ScanSequenceOffset = parameters.ScanSequenceOffset;
    PassParameters->ScanOffsetUAV = parameters.ScanOffsetUAV;
    PassParameters->ScanAzimuth = parameters.ScanAzimuth;
    PassParameters->LaserNumPerScan = parameters.LaserNumPerScan;
    PassParameters->ScanBufferUAV = parameters.ScanBuffer;

    TShaderMapRef<FDepthBasedLidarScanCS> depthBasedLidarCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));

    FIntVector groupCount = FIntVector(FMath::DivideAndRoundUp(parameters.ScanCount, 32), 1, 1);

    FComputeShaderUtils::AddPass<FDepthBasedLidarScanCS>(GraphBuilder, RDG_EVENT_NAME("Lidar Scan Pass"),
        ERDGPassFlags::Compute, depthBasedLidarCS, PassParameters, groupCount);
}

void FDepthBasedLidarCSInstance::GraphBuilderDispatchLidar(FRDGBuilder& GraphBuilder, FReorderParameters& parameters)
{
    FDepthBasedLidarReorderCS::FParameters* PassParameters;
    PassParameters = GraphBuilder.AllocParameters<FDepthBasedLidarReorderCS::FParameters>();
    PassParameters->ScanCount = parameters.ScanCount;
    PassParameters->ChannelCount = parameters.ChannelCount;
    PassParameters->LaserNumPerScan = parameters.LaserNumPerScan;
    PassParameters->ScanOffsetUAV = parameters.ScanOffsetUAV;
    PassParameters->RawHitBuffer = parameters.RawHitBuffer;
    PassParameters->ReorderedLidarBuffer = parameters.ReorderedLidarBuffer;

    TShaderMapRef<FDepthBasedLidarReorderCS> depthBasedLidarCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));

    FIntVector groupCount = FIntVector(
        FMath::DivideAndRoundUp(parameters.ScanCount, 32), FMath::DivideAndRoundUp(parameters.ChannelCount, 32), 1);

    FComputeShaderUtils::AddPass<FDepthBasedLidarReorderCS>(GraphBuilder, RDG_EVENT_NAME("Lidar Reorder Pass"),
        ERDGPassFlags::Compute, depthBasedLidarCS, PassParameters, groupCount);
}
