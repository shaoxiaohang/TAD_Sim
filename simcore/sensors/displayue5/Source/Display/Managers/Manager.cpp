#include "Manager.h"
#include "Framework/DisplayGameModeBase.h"


TArray<ISimActorInterface*> AManager::registeredSimActorArry;

AManager::AManager()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AManager::Init(const FManagerConfig& Config)
{
}

void AManager::Update(const FManagerIn& Input, FManagerOut& Output)
{
}

bool AManager::RegisterSimActor(ISimActorInterface* _Actor)
{
    if (!_Actor)
    {
        return false;
    }

    if (registeredSimActorArry.Contains(_Actor))
    {
        return false;
    }
    else
    {
        registeredSimActorArry.Add(_Actor);
        return true;
    }
}