#include "SensorActor.h"
#include "Framework/DisplayGameInstance.h"

ASensorActor::ASensorActor()
{
}

bool ASensorActor::Init(const FSensorConfig& _Config)
{
    UE_LOG(LogTemp, Log, TEXT("Init %s! Id: %d"), *_Config.typeName, _Config.id);
    return true;
}

UDisplayGameInstance* ASensorActor::GetDisplayInstance()
{
    if (!GetWorld())
    {
        return nullptr;
    }
    UGameInstance* GI = GetWorld()->GetGameInstance();
    UDisplayGameInstance* DGI = NULL;
    if (GI)
    {
        DGI = Cast<UDisplayGameInstance>(GI);
    }
    return DGI;
}