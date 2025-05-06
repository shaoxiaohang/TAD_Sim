// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Objects/Sensors/CameraSensors/FisheyeSensor.h"
#include "Camera/CameraTypes.h"
#include "Materials/Material.h"
#include "Runtime/ImageWrapper/Public/IImageWrapper.h"
#include "SharedMemoryWriter.h"
#include "FisheyeSensorNew.generated.h"

/**
 *
 */
UCLASS()
class DISPLAY_API AFisheyeSensorNew : public ASensorActor
{
    GENERATED_BODY()
public:
    AFisheyeSensorNew();
    ~AFisheyeSensorNew();

    virtual bool Init(const FSensorConfig& _Config);
    virtual ISimActorInterface* Install(const FSensorConfig& _Config);
    virtual void Update(const FSensorInput& _Input, FSensorOutput& _Output);

    void IgnoreActor(AActor* actor);

protected:
    TArray<class ADrawBatch*> labelTypeArry;
    TArray<FName> onlyShowTypeArry;
    TArray<FName> ignoreTypeArry;

    bool bHasFilterListSet = false;

public:
    UMaterialInstanceDynamic* cameraPostProcess;
    UPROPERTY(/*BlueprintReadOnly*/)
    FFisheyeConfig sensorConfig;

    class USceneCaptureComponent2D* captureComponent2D = NULL;
    class UTextureRenderTarget2D* renderTarget2D = NULL;

    class USceneCaptureComponentCube* captureComponentCube = NULL;
    class UTextureRenderTargetCube* renderTargetCube = NULL;

    // CaptureParameter
    FString imageName = TEXT("Fisheye");
    EImageFormat imageFormat = EImageFormat::JPEG;
    int32 imageQuality = 85;
    FFisheyeOutput dataBuf;
    //
    double timeStamp = -10000;
    double lastTimeStamp = 0;
    double targetGamma = 2.2;
    TSharedPtr<class SharedMemoryWriter> sharedWriter;
    TSharedPtr<class SharedMemoryWriter> sharedWriterGpu;
    bool public_msg = false;

    void SetPostProcessSettings(FPostProcessSettings& PostProcessSettings, float screen_scale = 1);
};
