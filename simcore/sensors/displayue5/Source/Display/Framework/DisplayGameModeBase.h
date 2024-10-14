#pragma once

#include "CoreMinimal.h"
#include "DisplayGameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "DisplayGameModeBase.generated.h"


UCLASS()
class DISPLAY_API ADisplayGameModeBase : public AGameModeBase
{
    GENERATED_BODY()
public:
    ADisplayGameModeBase();

public:
    /* Engine Interface */

    /**
     * Accept or reject a player attempting to join the server.  Fails login if you set the ErrorMessage to a non-empty
     * string. PreLogin is called before Login.  Significant game time may pass before Login is called
     *
     * @param    Options                    The URL options (e.g. name/spectator) the player has passed
     * @param    Address                    The network address of the player
     * @param    UniqueId                The unique id the player has passed to the server
     * @param    ErrorMessage            When set to a non-empty value, the player will be rejected using the error
     * message set
     */
    virtual void PreLogin(
        const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage);

    /**
     * Called to login new players by creating a player controller, overridable by the game
     *
     * Sets up basic properties of the player (name, unique id, registers with backend, etc) and should not be used to
     * do more complicated game logic.  The player controller is not fully initialized within this function as far as
     * networking is concerned. Save "game logic" for PostLogin which is called shortly afterward.
     *
     * @param NewPlayer pointer to the UPlayer object that represents this player (either local or remote)
     * @param RemoteRole the remote role this controller has
     * @param Portal desired portal location specified by the client
     * @param Options game options passed in by the client at login
     * @param UniqueId platform specific unique identifier for the logging in player
     * @param ErrorMessage [out] error message, if any, why this login will be failing
     *
     * If login is successful, returns a new PlayerController to associate with this player. Login fails if ErrorMessage
     * string is set.
     *
     * @return a new player controller for the logged in player, NULL if login failed for any reason
     */
    virtual APlayerController* Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal,
        const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage);

    /** Called after a successful login.  This is the first place it is safe to call replicated functions on the
     * PlayerController. */
    virtual void PostLogin(APlayerController* NewPlayer);

    /** Called when a Controller with a PlayerState leaves the game or is destroyed */
    virtual void Logout(AController* Exiting);

    /**
     * Initialize the game.
     * The GameMode's InitGame() event is called before any other functions (including PreInitializeComponents() )
     * and is used by the GameMode to initialize parameters and spawn its helper classes.
     * @warning: this is called before actors' PreInitializeComponents.
     */
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage);

    /**
     *    Function called every frame on this Actor. Override this function to implement custom logic to be executed
     * every frame. Note that Tick is disabled by default, and you will need to check PrimaryActorTick.bCanEverTick is
     * set to true to enable it.
     *
     *    @param    DeltaSeconds    Game time elapsed during last frame modified by the time dilation
     */
    virtual void Tick(float DeltaSeconds);

public:
    // Check all client keep connect
    bool CheckAllClientKeepConnect();

    // Check all login client loaded server current world.
    bool CheckAllClientLoadedCurrentWorld();

protected:
    /* Engine Interface */
    /** Overridable native event for when play begins for this actor. */
    virtual void BeginPlay();

protected:

    int64 id_controlled = 0;

};