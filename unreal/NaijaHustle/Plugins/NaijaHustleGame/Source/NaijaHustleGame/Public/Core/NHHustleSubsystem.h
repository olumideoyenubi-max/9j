#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NHHustleSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FNHLedgerEntry
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Naija") float Minutes = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Naija") int32 Amount = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Naija") FString Why;
};

/** What survives between sessions (same fields as the browser demo's save, plus story flags) */
UCLASS()
class NAIJAHUSTLEGAME_API UNHSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) int32 Version = 2;
	UPROPERTY(SaveGame) int32 Cash = 0;
	UPROPERTY(SaveGame) int32 BoughtCash = 0;
	UPROPERTY(SaveGame) int32 Cred = 0;
	UPROPERTY(SaveGame) int32 Integrity = 0;
	UPROPERTY(SaveGame) float Minutes = 480.f;
	UPROPERTY(SaveGame) int32 Jobs = 0;
	UPROPERTY(SaveGame) TArray<FName> Done;
	UPROPERTY(SaveGame) TArray<FName> Flags;
	UPROPERTY(SaveGame) FName Outfit;
	UPROPERTY(SaveGame) TArray<FNHLedgerEntry> Ledger;
};

class ANHPlayerState;
class ANHGameState;

/**
 * Saving and loading. The player's hustle itself lives on their ANHPlayerState and the clock on the
 * ANHGameState (both replicated, see docs/MULTIPLAYER.md). This writes the host's progress to the local
 * "NaijaHustle" slot after every job; on servers the same calls will go to a database instead.
 */
UCLASS()
class NAIJAHUSTLEGAME_API UNHHustleSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UNHHustleSubsystem* Get(const UObject* WorldContext);

	/** Saves the host's progress and the clock (does nothing on a client) */
	void Save(const UObject* WorldContext);
	/** Loads the local save into this player and the clock; false if there's no save */
	bool LoadInto(ANHPlayerState* Player, ANHGameState* World);
	/** Deletes the save and puts the host back at the start */
	void ResetProgress(const UObject* WorldContext);

	/** "N5,000": the engine's default font has no naira sign, so the HUD writes "N" (as on a Lagos price tag) */
	static FString Naira(int32 Amount);
};
