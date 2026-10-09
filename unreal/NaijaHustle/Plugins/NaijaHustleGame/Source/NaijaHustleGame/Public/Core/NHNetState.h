#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "NHNetState.generated.h"

/**
 * The world's shared state, replicated to everybody: the in-game clock and the lighting preset.
 *
 * The server copies them here from where the single-player game keeps them (UNHHustleSubsystem, ANHLightingRig); a
 * client copies them back out into its own subsystem and rig, so the HUD's clock and the sky follow the server's
 * without the code that reads them changing. First step of docs/NETWORK_AUDIT.md.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ANHGameState();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** In-game minutes since the start of day 1 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Naija") float Minutes = 480.f;
	/** ENHLightingPreset, or 255 when the level has no lighting rig */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Naija") uint8 LightingPreset = 255;
};

/**
 * One player's hustle, owned by the server: money, cred, integrity and jobs done (replicated to that player only)
 * and the wanted level (to everybody). Only the server changes them, through Earn, AddHeat and ClearHeat, and every
 * change of money is logged (lines starting MONEY) with who, how much, why and the balance after.
 *
 * The host's own player state mirrors UNHHustleSubsystem, which stays the single-player store and save: so in single
 * player nothing behaves differently. A joining player gets a wallet of their own, starting at the start cash. On a
 * client, the local player's values are copied into that client's subsystem for the HUD to read.
 *
 * Still to do (docs/NETWORK_AUDIT.md): the game's 31 Earn and AddHeat call sites go through the subsystem and so
 * still mean "the host's player". Each system moves over to the player concerned as it is made network-safe.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ANHPlayerState();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Naija") int32 Cash = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Naija") int32 Cred = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Naija") int32 Integrity = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Naija") int32 Jobs = 0;
	/** Wanted level, 0..5 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Naija") float Heat = 0.f;

	/** Server only: pays this player (or takes, if negative). False, and nothing changes, anywhere else. */
	bool Earn(int32 Amount, const FString& Why);
	/** Server only */
	bool AddHeat(float Amount);
	bool ClearHeat();

	/** True for the player whose money lives in the server's own UNHHustleSubsystem and save: the host */
	bool IsHostWallet() const;

private:
	float LastSaid = -10.f;
};
