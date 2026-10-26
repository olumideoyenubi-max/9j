#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "NHGameState.generated.h"

/**
 * The shared world: the in-game clock (and, later, weather, power cuts and market prices). The server runs
 * the clock and every machine sees the same time. Each machine applies the lighting for that time itself.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ANHGameState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	/** The start time from the rules, before any save loads */
	virtual void PostInitializeComponents() override;

	static ANHGameState* Get(const UObject* WorldContext);

	/** In-game minutes since the start of day 1 (8:00 = 480) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Naija") float Minutes = 480.f;
	/** How fast the clock runs (0.5 while a mission deadline counts down) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Naija") float ClockScale = 1.f;

	UFUNCTION(BlueprintPure, Category = "Naija") FString ClockText() const;
	UFUNCTION(BlueprintPure, Category = "Naija") int32 Day() const { return 1 + FMath::FloorToInt(Minutes / 1440.f); }
	UFUNCTION(BlueprintPure, Category = "Naija") float HourOfDay() const { return FMath::Fmod(Minutes, 1440.f) / 60.f; }

	/** Server only */
	void SetMinutes(float NewMinutes);
	void SetClockScale(float Scale);

	/** The player picked a lighting preset by hand on this machine: stop following the clock */
	void SetManualLighting() { bManualLighting = true; }

private:
	void UpdateLighting(float DeltaSeconds);
	bool bManualLighting = false;
	float LightCheck = 0.f;
	int32 LastPreset = -1;
};
