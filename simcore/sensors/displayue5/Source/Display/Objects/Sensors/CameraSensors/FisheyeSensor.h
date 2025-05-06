#pragma once

#include "Camera/CameraTypes.h"
#include "CoreMinimal.h"
#include "Materials/Material.h"
#include "Objects/Sensors/SensorActor.h"

#include "FisheyeSensor.generated.h"

class USceneCaptureComponent2D;
class FFisheyeSceneViewExtension;

USTRUCT()
struct FFisheyeConfig : public FSensorConfig
{
    GENERATED_BODY()
public:
    UPROPERTY()
    TArray<double> intrinsic_Matrix;
    UPROPERTY()
    TArray<double> distortion_Parameters;
    UPROPERTY()
    double res_Horizontal = 0;
    UPROPERTY()
    double res_Vertical = 0;
    UPROPERTY()
    double vignette_Intensity = 0.4;
    UPROPERTY()
    double noise_Intensity = 0;
    UPROPERTY()
    double motionBlur_Amount = 0;
    UPROPERTY()
    double blur_Intensity = 0;
    UPROPERTY()
    int color_gray = 0;
    UPROPERTY()
    double LensFlares = 0;

    UPROPERTY()
    double Exquisite = 0;
    UPROPERTY()
    double Bloom = 0.1;
    UPROPERTY()
    int Exposure = 0;
    UPROPERTY()
    double Compensation = 1;
    UPROPERTY()
    double ShutterSpeed = 60;
    UPROPERTY()
    double ISO = 100;
    UPROPERTY()
    double Aperture = 4;
    UPROPERTY()
    double ColorTemperature = 6500;
    UPROPERTY()
    double WhiteHint = 0;
    UPROPERTY()
    double Transmittance = 98;
    UPROPERTY()
    FString PostProcessMaterial;
    UPROPERTY()
    bool bIsRGB = true;
    UPROPERTY()
    bool bCaptureEveryFrame = false;
    UPROPERTY()
    bool bMultiCapture = false;
};

USTRUCT()
struct FFisheyeInput : public FSensorInput
{
    GENERATED_BODY()
public:
};

USTRUCT()
struct FFisheyeOutput : public FSensorOutput
{
    GENERATED_BODY()
public:
    std::vector<uint8> buffer;
};

UCLASS()
class DISPLAY_API AFisheyeSensor : public ASensorActor
{
    GENERATED_BODY()

public:
    AFisheyeSensor();

    struct FCameraPatch
    {
        FIntVector2 LeftTop;
        FIntVector2 RightBottom;
    };

    struct FCameraLayout
    {
        FQuat Direction;
        FVector2f LocalFovStart;
        FVector2f LocalFovEnd;
        FMatrix ProjectionMatrix;
        FVector4d Intrinsics;    // cx, cy, fx, fy
    };

    virtual void Tick(float DeltaSeconds) override;

    virtual bool Init(const FSensorConfig& _Config);

    virtual void Update(const FSensorInput& Input, FSensorOutput& Output);

    virtual ISimActorInterface* Install(const FSensorConfig& _Config);

    FVector2D imageRes = FVector2D(500, 500);

    bool bIsRGB = true;

	static void SaveDirectionMapToTxt(const TArray<FVector>& DirectionMap, const int& Width, const int& Height);


protected:
    void InitSceneCaptureComponent();

    void SetupDistortionMap();

	void CreateOtherCaptureComponents();

    void IgnoreActor(AActor* actor);

    bool PublishRGB(FSensorOutput& Output, const std::vector<uint8>& BitData, double timeStamp_ego);

    bool PublishDepth(FSensorOutput& Output, FColor* Pixels, double timeStamp_ego);

    bool PublishDepth2(FSensorOutput& Output, const std::vector<uint8>& BitData, double timeStamp_ego);

    bool PublishSemantic(FSensorOutput& Output, const std::vector<uint8>& BitData, double timeStamp_ego);

    bool PublishNormal(FSensorOutput& Output, const std::vector<uint8>& BitData, double timeStamp_ego);

protected:
    int id;

    FFisheyeConfig sensorConfig;

    double frequency = 10;

    TArray<FCameraLayout> CameraLayouts;

    bool bUseMultipleCapture = false;

    bool bPublicMsg = false;

    UTextureRenderTarget2D* CaptureRenderTarget = nullptr;

    USceneCaptureComponent2D* CaptureComponent2D = nullptr;

    TArray<USceneCaptureComponent2D*> CaptureComponents;

    // distorted uv --> undistorted uv
    TArray<FVector2f> UVMapping;
    TArray<unsigned int> UVCameraMapping;

    friend class FFisheyeSceneViewExtension;

    TSharedPtr<FFisheyeSceneViewExtension> SceneViewExtension;

    float SourceImageScaleFactor = 1;

    bool bCaptureEveryFrame = false;
    bool bEnablePostProcessingEffects = false;

    float Fx = 0;
    float Fy = 0;
    float Cx = 0;
    float Cy = 0;

    float K1 = 0;
    float K2 = 0;
    float K3 = 0;
    float K4 = 0;

    float Fov_H = 0;
    float Fov_V = 0;

    int32 ImageQuality = 85;

    double timeStamp = -10000;

    UMaterialInstanceDynamic* PostProcessMaterial = nullptr;

    FString CameraPostProcess;

    float fov_range_ = 50;
};