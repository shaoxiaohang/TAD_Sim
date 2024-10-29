#include "DisplayGameModeBase.h"
#include "DisplayPlayerController.h"
#include "DisplayGameStateBase.h"
#include "DisplayPlayerState.h"
#include "DisplayHUD.h"
#include "DisplayGameSession.h"
#include "MapGeneratedActor.h"
#include "Modules/OpenDriveFunctionLibrary.h"

DEFINE_LOG_CATEGORY(LogSimGameMode);

ADisplayGameModeBase::ADisplayGameModeBase()
{
    // Allow tick every frame
    PrimaryActorTick.bCanEverTick = true;

    // Turn on Seamless Travel
    bUseSeamlessTravel = true;

    PlayerControllerClass = ADisplayPlayerController::StaticClass();
    //DefaultPawnClass = APawn::StaticClass();
    GameStateClass = ADisplayGameStateBase::StaticClass();
    PlayerStateClass = ADisplayPlayerState::StaticClass();
    HUDClass = ADisplayHUD::StaticClass();
    GameSessionClass = ADisplayGameSession::StaticClass();
};

void ADisplayGameModeBase::PreLogin(
    const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
}

APlayerController* ADisplayGameModeBase::Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal,
    const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    return Super::Login(NewPlayer, InRemoteRole, Portal, Options, UniqueId, ErrorMessage);
}

void ADisplayGameModeBase::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    UE_LOG(LogSimSystem, Log, TEXT("Player connected! PlayerName: %s"),
        *(NewPlayer->GetPlayerState<ADisplayPlayerState>()->GetPlayerName()));
    UE_LOG(LogSimSystem, Log, TEXT("PlayerNum: %d"), GetGameState<ADisplayGameStateBase>()->PlayerArray.Num());
    id_controlled = Cast<ADisplayPlayerController>(NewPlayer)->id_controlled;

    check(GetGameInstance<UDisplayGameInstance>());

    // Register client info to simulator in GameInstance.
    GetGameInstance<UDisplayGameInstance>()->RegisterClientToSim(NewPlayer);
}

void ADisplayGameModeBase::Logout(AController* Exiting)
{
    Super::Logout(Exiting);

    // TODO: Stop simulator, send error message to coordinator.

    check(GetGameInstance<UDisplayGameInstance>());

    // Register client info to simulator in GameInstance.
    GetGameInstance<UDisplayGameInstance>()->UnregisterClientFromSim(Exiting);
}

void ADisplayGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
}

void ADisplayGameModeBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
}

void ADisplayGameModeBase::BeginPlay()
{
    Super::BeginPlay();
    // Check all client loaded world, send event to GameInstance.
    if (GetGameInstance<UDisplayGameInstance>()->bAllClientsLogin && CheckAllClientLoadedCurrentWorld())
    {
        GetGameInstance<UDisplayGameInstance>()->OnAllClientLevelLoaded();
    }
}

void ADisplayGameModeBase::SimInput(const FSimData& _Data)
{
    if (_Data.name == TEXT("RESET"))
    {
        FLocalResetIn ResetInData;
        ConvertData_SimToLocal(_Data, ResetInData);
        GetGameState<ADisplayGameStateBase>()->SimInput(ResetInData);
    }
    else if (_Data.name == TEXT("UPDATE"))
    {
        FLocalUpdateIn UpdateInData;
        ConvertData_SimToLocal(_Data, UpdateInData);
        GetGameState<ADisplayGameStateBase>()->SimInput(UpdateInData);
    }
}

bool ADisplayGameModeBase::CheckAllClientKeepConnect()
{
    check(GetGameInstance<UDisplayGameInstance>());
    check(GetGameState<ADisplayGameStateBase>());
    bool IsAllConnect = true;
    TArray<FClientInfo> ClientConfigArry = GetGameInstance<UDisplayGameInstance>()->GetAllClientConfig();
    for (size_t i = 0; i < ClientConfigArry.Num(); i++)
    {
        bool IsInArray = false;
        for (auto& Elem : GetGameState<ADisplayGameStateBase>()->PlayerArray)
        {
            if (ClientConfigArry[i].uniqueNetId == Elem->GetUniqueId())
            {
                IsInArray = true;
            }
        }
        if (!IsInArray)
        {
            IsAllConnect = false;
            UE_LOG(LogSimSystem, Warning, TEXT("Client %s Is Disconnect!(%d/%d)"), *ClientConfigArry[i].playerName,
                i + 1, ClientConfigArry.Num());
        }
    }
    return IsAllConnect;
}

bool ADisplayGameModeBase::CheckAllClientLoadedCurrentWorld()
{
    check(GetGameInstance<UDisplayGameInstance>());
    bool IsAllLoaded = true;
    TArray<FClientInfo> ClientConfigArry = GetGameInstance<UDisplayGameInstance>()->GetAllClientConfig();
    for (size_t i = 0; i < ClientConfigArry.Num(); i++)
    {
        APlayerController* PC =
            GetPlayerControllerFromNetId(GetWorld(), *ClientConfigArry[i].uniqueNetId.GetUniqueNetId().Get());
        if (PC)
        {
            if (!PC->HasClientLoadedCurrentWorld())
            {
                IsAllLoaded = false;
                UE_LOG(LogSimSystem, Warning, TEXT("Client %s Has Not Loaded!(%d/%d)"), *ClientConfigArry[i].playerName,
                    i + 1, ClientConfigArry.Num());
            }
        }
        else
        {
            IsAllLoaded = false;
            UE_LOG(LogSimSystem, Warning,
                TEXT("Client %s Has Not Loaded!(%d/%d) Can Not Get PlayerController By It`s UniqueNetId."),
                *ClientConfigArry[i].playerName, i + 1, ClientConfigArry.Num());
        }
    }
    return IsAllLoaded;
}

void ADisplayGameModeBase::ConvertData_SimToLocal(const FSimData& _SimData, FLocalData& _LocalData)
{
    if (_SimData.name == TEXT("RESET"))
    {
        const FSimResetIn* ResetInPtr = static_cast<const FSimResetIn*>(&_SimData);
        FLocalResetIn NewLocalResetIn;   
        if (ResetInPtr->sceneBuffer.empty())
        {
            UE_LOG(LogSimSystem, Error, TEXT("Can not get scene Buffer"));
        }
        else
        {
            int64 EgoID = GetGameInstance<UDisplayGameInstance>()->ModuleGroupName.IsEmpty()
                              ? -1
                              : GetGameInstance<UDisplayGameInstance>()->GetEgoIDByGroupName();
            NewLocalResetIn.sensorManager = ASensorManager::ParseSensorString(ResetInPtr->sceneBuffer, EgoID);
            GetGameInstance<UDisplayGameInstance>()->GetCatalogDataSource()->LoadSceneBuffer(ResetInPtr->sceneBuffer);
        }
        UClass* MapGeneratedActorClass =
            LoadClass<AMapGeneratedActor>(NULL, TEXT("Blueprint'/AutoRoad/HadmapActor.HadmapActor_C'"));
        AMapGeneratedActor* HadMapActor = nullptr;
        if (MapGeneratedActorClass)
        {
            HadMapActor =
                GetWorld()->SpawnActor<AMapGeneratedActor>(MapGeneratedActorClass, FVector(0, 0, 0), FRotator(0, 0, 0));
        }
        else
        {
            UE_LOG(LogSimSystem, Error, TEXT("AMapGeneratedActor class load fail"));
        }
        if (HadMapActor)
        {
            bool bArtLevel = false;
            if (ResetInPtr->mapIndex == 0)
            {
                bool bOriginInEgo = false;
                GConfig->GetBool(TEXT("AutoRoad"), TEXT("bOriginInEgo"), bOriginInEgo, GGameIni);
                FVector NewOrigin = UOpenDriveFunctionLibrary::GetMapCenterLonlat(ResetInPtr->mapDataBasePath);
                HadMapActor->ShowObjectOnly = false;

                if (bOriginInEgo)
                {
                    UE_LOG(LogSimSystem, Log, TEXT("AutoRoad bOriginInEgo true"));

                    HadMapActor->RefX = ResetInPtr->startLon;
                    HadMapActor->RefY = ResetInPtr->startLat;
                    HadMapActor->RefZ = NewOrigin.Z - 0.5f;
                }
                else
                {
                    UE_LOG(LogSimSystem, Log, TEXT("AutoRoad bOriginInEgo false"));

                    HadMapActor->RefX = NewOrigin.X;
                    HadMapActor->RefY = NewOrigin.Y;
                    HadMapActor->RefZ = NewOrigin.Z - 0.5f;
                }
                UE_LOG(LogSimSystem, Error, TEXT("Auto Road Origin is %f, %f, %f"), HadMapActor->RefX,
                    HadMapActor->RefY, HadMapActor->RefZ);
            }
            else
            {
                bArtLevel = true;
                // 沿用Game.Ini中的经纬度并只渲染放置物
                HadMapActor->ShowObjectOnly = true;
                HadMapActor->RefX = ResetInPtr->mapOriginLon;
                HadMapActor->RefY = ResetInPtr->mapOriginLat;
                HadMapActor->RefZ = ResetInPtr->mapOriginAlt;
            }
            const TMap<FString, TPair<FString, FVector>>& MapModelData =
                GetGameInstance<UDisplayGameInstance>()->GetCatalogDataSource()->GetMapModelData();
            HadMapActor->DrawMap(ResetInPtr->mapDataBasePath, ResetInPtr->decryptFilePath, bArtLevel, MapModelData,
                ResetInPtr->ModelPath);
        }

        FLocalResetIn* LocalResetInPtr = static_cast<FLocalResetIn*>(&_LocalData);
        NewLocalResetIn.name = _SimData.name;
        /*~ Ego ~*/
        for (const auto& Elem : ResetInPtr->EgoInitInfoArry)
        {
            FVehicleConfig EgoConfig;
            EgoConfig.id = Elem.EgoID;
            EgoConfig.timeStamp = ResetInPtr->timeStamp;
            EgoConfig.trafficType = ETrafficType::ST_Ego;

            EgoConfig.type = FCString::Atoi(*(Elem.egoType.Replace(TEXT("transport/Type"), TEXT(""))));
            EgoConfig.typeName = GetTypeIdDef(Elem.egoType);
            EgoConfig.Name = Elem.egoName;

            UE_LOG(LogSimSystem, Warning, TEXT("egoType %s egoName %s"), *Elem.egoType, *Elem.egoName);

            double x = Elem.startLon;
            double y = Elem.startLat;
            double z = Elem.startAlt;
            hadmapue4::HadmapManager::Get()->LonLatToLocal(x, y, z, EgoConfig.startLocation);
            // Rotation
            FRotator egoVehicleRotation(ForceInit);
            EgoConfig.startRotation = FRotator(0, -Elem.startTheta * 180 / PI - 90, 0);
            // Velocity
            FVector Velocity = EgoConfig.startRotation.Vector() * Elem.startSpeed;
            EgoConfig.initVelocity = Velocity;
        
            // Snap
            if (!GConfig->GetBool(TEXT("Mode"), TEXT("SnapGround"), EgoConfig.isEgoSnap, GGameIni))
            {
                UE_LOG(LogSimSystem, Error, TEXT("Can not get ego snamp mode"));
            }

            NewLocalResetIn.transportManager.vehicleManagerConfig.egoConfigArry.Add(EgoConfig);

            *LocalResetInPtr = NewLocalResetIn;
        }
    }

    if (_SimData.name == TEXT("UPDATE"))
    {
        const FSimUpdateIn* UpdateInPtr = static_cast<const FSimUpdateIn*>(&_SimData);
        FLocalUpdateIn* LocalUpdateInPtr = static_cast<FLocalUpdateIn*>(&_LocalData);

        FLocalUpdateIn NewLocalUpdateIn;
        NewLocalUpdateIn.timeStamp = UpdateInPtr->timeStamp;
        NewLocalUpdateIn.name = UpdateInPtr->name;
        /*~ Ego ~*/
        for (const auto& Elem : UpdateInPtr->egoData)
        {
            const FString& GroupName = Elem.Key;
            const sim_msg::Location& Location = Elem.Value;

            FVehicleIn EgoInput;
            EgoInput.id = GetGameInstance<UDisplayGameInstance>()->GetEgoIDByGroupName(GroupName);
            EgoInput.timeStamp = UpdateInPtr->timeStamp;
            EgoInput.timeStamp0 = Elem.Value.t() * 1000;
            EgoInput.trafficType = ETrafficType::ST_Ego;
            EgoInput.typeName = TEXT("");

            // Location
            hadmapue4::HadmapManager::Get()->LonLatToLocal(
                Location.position().x(), Location.position().y(), 
                Location.position().z(), EgoInput.location);

            UE_LOG(LogSimSystem, Display, TEXT("Update ego lon %f lat %f alt %f x %f y %f z %f"),
            Location.position().x(), Location.position().y(), Location.position().z(), 
            EgoInput.location.X,EgoInput.location.Y,EgoInput.location.Z );

            // Rotation
            FRotator egoVehicleRotation(ForceInit);
            EgoInput.rotation.Roll = (float) (Location.rpy().x() * 180 / PI);
            EgoInput.rotation.Pitch = (float) (-Location.rpy().y() * 180 / PI);
            EgoInput.rotation.Yaw = (float) (-Location.rpy().z() * 180 / PI - 90);
            // Velocity
            FVector Velocity = FVector(Location.velocity().x(), -Location.velocity().y(),
             Location.velocity().z());
            EgoInput.velocity = Velocity;

            NewLocalUpdateIn.transportManager.vehicleManagerIn.egoVehicleInputArry.Add(EgoInput);
        }

        /*~ Traffic ~*/
        for (auto& Elem : UpdateInPtr->trafficData.cars())
        {
            FVehicleIn TrafficInput;
            TrafficInput.id = Elem.id();
            TrafficInput.timeStamp = UpdateInPtr->timeStamp;
            TrafficInput.timeStamp0 = Elem.t();
            TrafficInput.sizeLWH = FVector(Elem.length(), Elem.width(), Elem.height()) * 100;
            TrafficInput.trafficType = ETrafficType::ST_TRAFFIC;
            TrafficInput.type = Elem.type();
            TrafficInput.typeName = GetTypeIdDef(Elem.type(), TEXT("transport"));


            // Location
            double x = Elem.x();
            double y = Elem.y();
            double z = Elem.z();
            hadmapue4::HadmapManager::Get()->LonLatToLocal(x, y, z, TrafficInput.location);

            UE_LOG(LogSimSystem, Display, TEXT("Update traffic vehicle %s %f %f %f"), 
            *TrafficInput.typeName,  TrafficInput.location.X,TrafficInput.location.Y,TrafficInput.location.Z );

            // Rotation
            TrafficInput.rotation = FRotator(0, -Elem.heading() * 180 / PI - 90, 0);
            // Velocity
            TrafficInput.velocity = TrafficInput.rotation.Vector().GetSafeNormal() * Elem.v();
            // Add
            NewLocalUpdateIn.transportManager.vehicleManagerIn.
                trafficVehicleInputArry.Add(TrafficInput);
        }
        // Updat global time
        NewLocalUpdateIn.transportManager.vehicleManagerIn.timeStamp = UpdateInPtr->timeStamp;

        *LocalUpdateInPtr = NewLocalUpdateIn;
    }
}   

FString ADisplayGameModeBase::GetTypeIdDef(const FString& _TypeName)
{
    FString TypeDef;
    if (GConfig->GetString(TEXT("TypeDef"), *_TypeName, TypeDef, GGameIni))
    {
        return TypeDef;
    }
    return FString();
}

FString ADisplayGameModeBase::GetTypeIdDef(int32 _Id, FString _Type)
{
    FString KeyName = _Type + TEXT("/Type") + FString::FromInt(_Id);
    FString TypeDef;
    if (GConfig->GetString(TEXT("TypeDef"), *KeyName, TypeDef, GGameIni))
    {
        return TypeDef;
    }
    return FString();
}