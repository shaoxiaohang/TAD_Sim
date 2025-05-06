#include "SemanticCamera.h"
#include "HAL/PlatformFileManager.h"
#include "Framework/SaveDataThread.h"
#include "Runtime/ImageWrapper/Public/IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Framework/DisplayGameInstance.h"
#include "Runtime/Engine/Classes/Components/SceneCaptureComponent2D.h"


ASemanticCamera::ASemanticCamera()
{
    str_PostProcess = TEXT("Material'/Game/SensorSim/Camera/Material/Mat_Camera_Stencil.Mat_Camera_Stencil'");

    imageName = TEXT("Semantic");
    imageFormat = EImageFormat::PNG;
}

bool ASemanticCamera::Init(const FSensorConfig& _Config)
{
    if (!ACameraSensor::Init(_Config))
        return false;
    UseUnlitShowFlags(captureComponent->ShowFlags);
    return true;
}

ASemanticCamera::~ASemanticCamera()
{
}