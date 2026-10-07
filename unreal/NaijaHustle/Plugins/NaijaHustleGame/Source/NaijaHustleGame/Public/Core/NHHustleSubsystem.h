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

/** What survives between sessions (same fields as the browser demo's save) */
UCLASS()
class NAIJAHUSTLEGAME_API UNHSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) int32 Version = 1;
	UPROPERTY(SaveGame) int32 Cash = 0;
	UPROPERTY(SaveGame) int32 Cred = 0;
	UPROPERTY(SaveGame) int32 Integrity = 0;
	UPROPERTY(SaveGame) float Minutes = 480.f;
	UPROPERTY(SaveGame) int32 Jobs = 0;
	UPROPERTY(SaveGame) TArray<FName> Done;
	UPROPERTY(SaveGame) FName Outfit;
	UPROPERTY(SaveGame) TArray<FNHLedgerEntry> Ledger;
};

/**
 * The player's hustle: money, cred, integrity, the in-game clock, finished missions and the wanted
 * level ("heat", 0–5 stars). Lives on the game instance, so it carries across levels; saved to the
 * "NaijaHustle" slot after every job.
 */
UCLASS()
class NAIJAHUSTLEGAME_API UNHHustleSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	static UNHHustleSubsystem* Get(const UObject* WorldContext);

	UPROPERTY(BlueprintReadOnly, Category = "Naija") int32 Cash = 5000;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") int32 Cred = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") int32 Integrity = 0;
	/** In-game minutes since the start of day 1 (8:00 = 480) */
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float Minutes = 480.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") int32 Jobs = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TArray<FName> Done;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FName Outfit = TEXT("fit_street_basic");
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TArray<FNHLedgerEntry> Ledger;

	/** Wanted level: 0..5, shown as stars rounded up */
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float Heat = 0.f;

	UFUNCTION(BlueprintCallable, Category = "Naija") void Earn(int32 Amount, const FString& Why);
	UFUNCTION(BlueprintCallable, Category = "Naija") void AddHeat(float Amount);
	UFUNCTION(BlueprintCallable, Category = "Naija") void ClearHeat();
	UFUNCTION(BlueprintPure, Category = "Naija") int32 Stars() const;
	/** Unseen by any unit, the heat drops a star every SecondsPerStar */
	void TickHeat(float DeltaSeconds, bool bSeen);
	void TickClock(float DeltaSeconds, float Scale = 1.f);

	UFUNCTION(BlueprintPure, Category = "Naija") bool IsDone(FName Mission) const { return Done.Contains(Mission); }
	UFUNCTION(BlueprintPure, Category = "Naija") FString ClockText() const;
	UFUNCTION(BlueprintPure, Category = "Naija") int32 Day() const { return 1 + FMath::FloorToInt(Minutes / 1440.f); }
	UFUNCTION(BlueprintPure, Category = "Naija") float HourOfDay() const { return FMath::Fmod(Minutes, 1440.f) / 60.f; }

	UFUNCTION(BlueprintCallable, Category = "Naija") void Save();
	UFUNCTION(BlueprintCallable, Category = "Naija") bool Load();
	/** Back to a fresh start (and deletes the save) */
	UFUNCTION(BlueprintCallable, Category = "Naija") void ResetProgress();

	/** Fires on every Earn, for the HUD's cash change */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEarn, int32 /*Amount*/, const FString& /*Why*/);
	FOnEarn OnEarn;

	static FString Naira(int32 Amount);

private:
	float HeatTimer = 0.f;
	float SecondsPerStar = 18.f;
};
