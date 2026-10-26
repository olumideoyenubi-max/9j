#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Core/NHHustleSubsystem.h"
#include "NHPlayerState.generated.h"

/**
 * One player's hustle: money, cred, integrity, finished missions, story flags, outfit and the wanted
 * level ("heat", 0–5 stars). Replicated, and changed only on the server: clients ask through the
 * player controller's Server* calls and read the result here. In single-player your own machine is the
 * server, so nothing changes. See docs/MULTIPLAYER.md.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ANHPlayerState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** The PlayerState of the local player (the one this machine shows on the HUD) */
	static ANHPlayerState* Local(const UObject* WorldContext);
	/** The host's PlayerState: the player the story runs for */
	static ANHPlayerState* Host(const UObject* WorldContext);

	UPROPERTY(ReplicatedUsing = OnRep_Cash, BlueprintReadOnly, Category = "Naija") int32 Cash = 5000;
	/** Naira bought with Gold Kobo, kept apart: it can't be bet, invested or put into crypto (HUSTLES.md) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Naija") int32 BoughtCash = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Naija") int32 Cred = 0;
	/** -100..100: decides the ending with the story choices (STORY.md) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Naija") int32 Integrity = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Naija") int32 Jobs = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Naija") TArray<FName> Done;
	/** Story flags such as found_shina_bag, spared_jaguar, zainab_alive, kept_ledger */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Naija") TArray<FName> Flags;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Naija") FName Outfit = TEXT("fit_street_basic");
	/** Wanted level: 0..5, shown as stars rounded up */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Naija") float Heat = 0.f;
	/** Only the owning player needs their money history */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Naija") TArray<FNHLedgerEntry> Ledger;

	// ---- server only: each returns quietly on a client
	UFUNCTION(BlueprintCallable, Category = "Naija") void Earn(int32 Amount, const FString& Why);
	UFUNCTION(BlueprintCallable, Category = "Naija") void AddCred(int32 Amount);
	UFUNCTION(BlueprintCallable, Category = "Naija") void AddIntegrity(int32 Amount);
	UFUNCTION(BlueprintCallable, Category = "Naija") void AddJobs(int32 Count);
	UFUNCTION(BlueprintCallable, Category = "Naija") void MarkDone(FName Mission);
	UFUNCTION(BlueprintCallable, Category = "Naija") void SetFlag(FName Flag, bool bOn = true);
	UFUNCTION(BlueprintCallable, Category = "Naija") void AddHeat(float Amount);
	UFUNCTION(BlueprintCallable, Category = "Naija") void ClearHeat();

	UFUNCTION(BlueprintPure, Category = "Naija") bool IsDone(FName Mission) const { return Done.Contains(Mission); }
	UFUNCTION(BlueprintPure, Category = "Naija") bool HasFlag(FName Flag) const { return Flags.Contains(Flag); }
	UFUNCTION(BlueprintPure, Category = "Naija") int32 Stars() const;
	/** Naira this player may bet or invest: earned cash only */
	UFUNCTION(BlueprintPure, Category = "Naija") int32 RiskableCash() const { return FMath::Max(0, Cash - BoughtCash); }

	/** Copies to and from the save. The clock is saved by the subsystem from the game state. */
	void WriteTo(UNHSaveGame& Save) const;
	void ReadFrom(const UNHSaveGame& Save);
	/** Back to a fresh start */
	void ResetToStart();

	/** Fires on this machine whenever the cash changes (amount, and why when known), for the HUD */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCashChanged, int32 /*Delta*/, const FString& /*Why*/);
	FOnCashChanged OnCashChanged;

protected:
	/** Starting money and the heat timing from the rules, set as the player joins (before any save loads) */
	virtual void PostInitializeComponents() override;

private:
	UFUNCTION() void OnRep_Cash();
	/** What the HUD last saw, so a replicated change can show "+N500" */
	int32 SeenCash = 0;
	bool bSeenCash = false;
	float HeatTimer = 0.f;
	float SecondsPerStar = 18.f;
};
