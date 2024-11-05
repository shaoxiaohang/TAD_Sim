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

void ASensorActor::Update(const FSensorInput& _Input, FSensorOutput& _Output)
{
}

void ASensorActor::Destroy(FString _Reason)
{
    UE_LOG(LogTemp, Log, TEXT("Destroy: %s"), *_Reason);
    Super::Destroy();
}

ISimActorInterface* ASensorActor::Install(const FSensorConfig& _Config)
{
    configBase = _Config;
    check(GetWorld());
    if (_Config.targetType != ETrafficType::ST_NONE)
    {
        ISimActorInterface* SimActor = nullptr;
        if (GetWorld() && GetWorld()->GetGameState<ADisplayGameStateBase>())
        {
            auto gamestate = GetWorld()->GetGameState<ADisplayGameStateBase>();
            if (gamestate->syncSystem && gamestate->syncSystem->transportManager.IsValid() &&
                gamestate->syncSystem->transportManager->vehicleManager)
            {
                SimActor = gamestate->syncSystem->transportManager->vehicleManager->GetVehicle(
                    _Config.targetType, _Config.targetId);
            }
        }
        if (SimActor)
        {
            AActor* Actor = Cast<AActor>(SimActor);
            if (Actor)
            {
                UE_LOG(LogTemp, Log, TEXT("Install C0"));
                this->AttachToComponent(
                    Actor->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
                this->SetActorRelativeLocation(_Config.installLocation);
                this->SetActorRelativeRotation(_Config.installRotation);
                this->SetOwner(Actor);
                InstalledActor = Actor;
                return SimActor;
            }
        }
    }
    else
    {
        this->SetActorLocation(_Config.installLocation);
        this->SetActorRotation(_Config.installRotation);
        InstalledActor = nullptr;
    }

    return nullptr;
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

