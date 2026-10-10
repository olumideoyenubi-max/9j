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

/** A car kept in the garage of a home: the player's own, brought back from wherever it was left */
USTRUCT()
struct FNHGarageCar
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) int32 Serial = 0;
	UPROPERTY(SaveGame) FName Type;
	UPROPERTY(SaveGame) FLinearColor Paint = FLinearColor::Black;
	UPROPERTY(SaveGame) FName Home;
	UPROPERTY(SaveGame) float Fuel = 1.f;
	UPROPERTY(SaveGame) bool bWrecked = false;
};

/** One of the cast (Data/characters.json) as the player left them: where, facing which way, and how full their ability is */
USTRUCT()
struct FNHLeadSpot
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FName Id;
	/** The level it was in: a place in the small city means nothing in the real one */
	UPROPERTY(SaveGame) FName Level;
	UPROPERTY(SaveGame) FVector At = FVector::ZeroVector;
	UPROPERTY(SaveGame) float Yaw = 0.f;
	UPROPERTY(SaveGame) bool bPlaced = false;
	UPROPERTY(SaveGame) float Meter = 0.f;
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
	UPROPERTY(SaveGame) int64 Bank = 0;
	UPROPERTY(SaveGame) FName Persona;
	UPROPERTY(SaveGame) TArray<FName> Owned;
	UPROPERTY(SaveGame) FName Home;
	UPROPERTY(SaveGame) TArray<FNHGarageCar> Cars;
	UPROPERTY(SaveGame) int32 NextCar = 1;
	UPROPERTY(SaveGame) FName Lead;
	UPROPERTY(SaveGame) TArray<FNHLeadSpot> LeadSpots;
	UPROPERTY(SaveGame) TMap<FName, int32> Flags;
	UPROPERTY(SaveGame) TArray<FName> Bought;
	UPROPERTY(SaveGame) TMap<FName, int32> Medals;
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

	/**
	 * Money in the bank, apart from the cash in the pocket: 64 bits, because the big men and women of NHEstate have
	 * hundreds of billions and the pocket's 32 bits stop at two. Pay takes from the pocket first, then from here.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Naija") int64 Bank = 0;
	/** Who is being played (Data/estate.json "people"), or none: the conductor the game starts with */
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FName Persona;
	/** Land, houses and businesses bought (Data/estate.json "places"), and the one that is home */
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TArray<FName> Owned;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FName Home;
	/** The cars in the garages of the homes owned (ANHEstate keeps them), and the number the next one gets */
	UPROPERTY() TArray<FNHGarageCar> Cars;
	int32 NextCar = 1;
	/** Which of the cast is being played (Data/characters.json; none: the first lead), and where the others were left (ANHLeads) */
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FName Lead;
	UPROPERTY() TArray<FNHLeadSpot> LeadSpots;
	/** The story's flags (found_shina_bag, spared_jaguar, zainab_alive, kept_ledger...): 0 where never set */
	UPROPERTY() TMap<FName, int32> Flags;
	int32 Flag(FName Name) const { const int32* V = Flags.Find(Name); return V ? *V : 0; }
	void SetFlag(FName Name, int32 Value = 1) { Flags.Add(Name, Value); }
	/** The best medal on each story mission: 1 bronze, 2 silver, 3 gold (ANHMissions) */
	UPROPERTY() TMap<FName, int32> Medals;
	/** Characters bought in the store (premium ones) */
	UPROPERTY() TArray<FName> Bought;
	int64 Worth() const { return static_cast<int64>(Cash) + Bank; }
	/** Takes Amount from the pocket and then the bank, or takes nothing and says false */
	bool Pay(int64 Amount, const FString& Why);
	/** Into the bank (a sale, a transfer) */
	void Bankroll(int64 Amount, const FString& Why);
	static FString Naira(int64 Amount);

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
	/**
	 * False on a machine that has joined somebody else's game. There the numbers here are only a copy of what the
	 * server says (ANHPlayerState, ANHGameState): Earn, AddHeat, ClearHeat, the clock and Save do nothing.
	 */
	bool IsAuthority() const;

private:
	float HeatTimer = 0.f;
	float SecondsPerStar = 18.f;
};
