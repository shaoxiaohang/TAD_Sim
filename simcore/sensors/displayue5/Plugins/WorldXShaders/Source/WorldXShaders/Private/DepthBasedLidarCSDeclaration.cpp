#include "DepthBasedLidarCSDeclaration.h"

// IMPLEMENT_GLOBAL_SHADER(FDepthBasedMetaLidarCS, "/WorldXShaders/DepthBasedMetaLidarCS.usf", "MainComputeShader", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FDepthBasedLidarRawHitCS, "/WorldXShaders/DepthBasedLidarCS.usf", "RawHitMain", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FDepthBasedLidarScanCS, "/WorldXShaders/DepthBasedLidarCS.usf", "ScanMain", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FDepthBasedLidarReorderCS, "/WorldXShaders/DepthBasedLidarCS.usf", "ReorderMain", SF_Compute);
