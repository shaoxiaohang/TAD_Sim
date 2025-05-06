#include "NormalCamera.h"
#include "HAL/PlatformFileManager.h"
#include "Framework/SaveDataThread.h"
#include "Runtime/ImageWrapper/Public/IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Framework/DisplayGameInstance.h"
#include "Runtime/Engine/Classes/Components/SceneCaptureComponent2D.h"


ANormalCamera::ANormalCamera()
{
    str_PostProcess = TEXT("Material'/Game/SensorSim/Camera/Material/Mat_Camera_Capture_normal.Mat_Camera_Capture_normal'");

    imageName = TEXT("Normal");
    imageFormat = EImageFormat::PNG;
}

bool ANormalCamera::Init(const FSensorConfig& _Config)
{
    if (!ACameraSensor::Init(_Config))
        return false;
    UseUnlitShowFlags(captureComponent->ShowFlags);
    return true;
}

ANormalCamera::~ANormalCamera()
{
}