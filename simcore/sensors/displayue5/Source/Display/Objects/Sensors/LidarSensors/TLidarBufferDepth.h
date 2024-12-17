// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CudaLidarModel.h"
#include "LidarSensorDef.h"
#include "Networking.h"
#include "Objects/Sensors/SensorActor.h"
#include "RenderGraphResources.h"
#include "lidar/Lidar.h"
#include "lidar/LidarModel.h"

#include <map>
#include <vector>

class USceneCaptureComponent2D;
class FDepthMapBasedLidarSceneViewExtension;
class UMaterialInstanceDynamic;
class UTextureRenderTarget2D;
class FRHIGPUBufferReadback;

struct LidarScan
{
    int scan_offset;
    float scan_azimuth;
    unsigned int scan_sequence;
};

struct LidarDetection
{
    float x;
    float y;
    float z;
    int label;
    float norinter;
    float intensity;
};

struct DepthLidarBuffer : public LidarBuffer
{
    int detection_count;
    int scan_count;
    std::vector<LidarDetection> detections;
    std::vector<LidarScan> scans;
};

class ALidarBufferDepth : public LidarBufferFun
{
public:
    virtual LidarBufferMethod Method()
    {
        return BUFMETHOD_DEPTH;
    }
    virtual bool Init(const FLidarConfig& config, std::shared_ptr<lidar::TraditionalLidar> _LidarSensor,
        AActor* _actor = 0, class LidarModel* lmodel = 0);
    virtual TSharedPtr<LidarBuffer> GetTBuffer(const FTLidarMeasurement& measure);

    virtual bool GetPoints(const LidarBuffer* rawbuf, const FTLidarMeasurement& measure,
        lidar::TraditionalLidar::lidar_ptset& lidarBuffer);

    void PrepareParallelRays(std::shared_ptr<lidar::TraditionalLidar> _LidarSensor);

    void CreateSenceCaptureComponents();

    void CreateTextureRenderTargets();

    void SetupComponents();

    void SetupBuffers();

    struct FImageSpaceLaserRay
    {
        FVector3f direction;
        int row;
        int column;
        int scan_id;
        int laser_id;
        float azimuth;
    };

    struct RenderTargetSRVInfo
    {
        FString debug_name;
        UTextureRenderTarget2D* render_target;
        TRefCountPtr<IPooledRenderTarget> pooled_target;
    };

    bool OutputExtraInfo() const
    {
        return bOutputExtraInfo;
    }

private:
    enum ELidarPassType
    {
        BasePass = 0x01,
        PostPass = 0x02,
        All = 0x03
    };

    struct FLidarPassParams
    {
        // base pass
        FRDGBufferSRVRef ImageSpaceLaserRaysSRV;
        FRDGBufferUAVRef RawLidarBufferUAV;
        FRDGBufferUAVRef LaserNumPerScanUAV;
        FRDGBufferUAVRef DetectionCountUAV;

        // post pass
        FRDGBufferSRVRef ScanAzimuthSRV;
        FRDGBufferUAVRef ScanBufferUAV;
        FRDGBufferUAVRef ScanOffsetUAV;
        FRDGBufferUAVRef ReorderedBufferUAV;
    };

    FLidarPassParams CreateLidarPassParams(FRDGBuilder& GraphBuilder, ELidarPassType ParamsType);

    void AddLidarBasePass(FRDGBuilder& GraphBuilder, const int& CameraIndex, const FRDGTextureSRVRef& RenderTargetSRV,
        const FLidarPassParams& PassParams);

    void AddLidarBasePassExtra(FRDGBuilder& GraphBuilder, const int& CameraIndex,
        const FRDGTextureSRVRef& RenderTargetSRV, const FRDGTextureSRVRef& RenderTargetSRVExtra,
        const FLidarPassParams& PassParams);

    void AddLidarPostPass(FRDGBuilder& GraphBuilder, const FLidarPassParams& PassParams);

    void DisableShowFlags(FEngineShowFlags& ShowFlags);

    void EnableShowFlags(FEngineShowFlags& ShowFlags);

    void FetchReadbackBuffer(TSharedPtr<DepthLidarBuffer> buffer);

    bool ReadLidarData_RenderThreadSVE(TSharedPtr<DepthLidarBuffer> buffer);

    bool ReadLidarData_RenderThreadSingleCaptureSVE(TSharedPtr<DepthLidarBuffer> buffer);

    RenderTargetSRVInfo CreateRenderTargetInfo(const FString& DebugName);

    USceneCaptureComponent2D* CreateSceneCaptureComponent(const FString& DebugName, float Yaw);

private:
    FVector2f AzimuthRange;

    int ScanCount = 0;
    unsigned int ScanSequenceCount;

    int MaxPointNum = 0;

    int CameraCount = 24;

    int SceneCaptureCount;

    bool bUseSingleCapture = true;
    bool bUseEnableFlags = true;

    FVector2f FovRangeVerticalPerCamera = FVector2f(-25, 25);

    float FovHorizonPerCamera;

    float HorizonSampleCount;

    float DegreeHorizonPerPixel = 0.01;

    float DegreeVerticalPerPixel = 0.02;

    int ImageWidthPerCamera;

    int ImageHeightPerCamera;

    // calc ray direction
    double FocalLengthX;
    double FocalLengthY;

    unsigned int Channels = 0;

    float Range = 0;

    TArray<float> ScanAzimuth;

    TArray<FVector3f> LaserRays;
    TArray<FImageSpaceLaserRay> ImageSpaceLaserRays;

    TArray<USceneCaptureComponent2D*> SceneCaptures;
    TArray<UMaterialInstanceDynamic*> MaterialInstanceDynamics;
    bool bOutputExtraInfo = false;

    TArray<USceneCaptureComponent2D*> ExtraSceneCaptures;
    TArray<UMaterialInstanceDynamic*> ExtraMaterialInstanceDynamics;

    TSharedPtr<FDepthMapBasedLidarSceneViewExtension> SceneViewExtension;

    friend class FDepthMapBasedLidarSceneViewExtension;

    TArray<RenderTargetSRVInfo> RenderTargets;
    TArray<RenderTargetSRVInfo> ExtraRenderTargets;

    // read back
    TSharedPtr<FRHIGPUBufferReadback> LidarReadbackDetectionCount;
    TSharedPtr<FRHIGPUBufferReadback> LidarReadbackDetection;
    TSharedPtr<FRHIGPUBufferReadback> LidarReadbackScan;

    TRefCountPtr<FRDGPooledBuffer> RawLidarBufferRDGPooledBuffer;
    TRefCountPtr<FRDGPooledBuffer> LaserNumPerScanRDGPooledBuffer;
    TRefCountPtr<FRDGPooledBuffer> DetectionCountRDGPooledBuffer;
    TRefCountPtr<FRDGPooledBuffer> ScanBufferRDGPooledBuffer;
    TRefCountPtr<FRDGPooledBuffer> ScanOffsetRDGPooledBuffer;
    TRefCountPtr<FRDGPooledBuffer> ReorderedBufferRDGPooledBuffer;
    TRefCountPtr<FRDGPooledBuffer> ScanAzimuthRDGPooledBuffer;
    TRefCountPtr<FRDGPooledBuffer> ImageSpaceLaserRaysRDGPooledBuffer;
};
