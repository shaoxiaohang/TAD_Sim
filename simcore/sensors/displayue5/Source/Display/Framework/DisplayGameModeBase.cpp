#include "DisplayGameModeBase.h"
#include "DisplayPlayerController.h"
#include "DisplayGameStateBase.h"
#include "DisplayPlayerState.h"
#include "DisplayHUD.h"
#include "DisplayGameSession.h"

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