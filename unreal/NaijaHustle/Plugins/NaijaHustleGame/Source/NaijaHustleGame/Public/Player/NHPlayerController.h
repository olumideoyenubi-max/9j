#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NHPlayerController.generated.h"

class UNHInputSet;
class ANHVehicle;
class ANHCharacter;

UENUM(BlueprintType)
enum class ENHInputMode : uint8
{
	OnFoot,
	Vehicle,
	Menu
};

/**
 * Owns the input set and decides which mapping contexts are active (on foot, driving, in a menu).
 * Gets the player in and out of vehicles (F), sends E and the number keys to the game director
 * (calls, choices, dialogue) and builds the on-screen prompt.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Built on first use, so pawns can bind to it from SetupPlayerInputComponent whatever the init order. */
	UNHInputSet* GetInputSet();

	/** Swaps the active mapping contexts. Global stays on in every mode. */
	UFUNCTION(BlueprintCallable, Category = "Naija|Input")
	void SetNHInputMode(ENHInputMode NewMode);

	UFUNCTION(BlueprintPure, Category = "Naija|Input")
	ENHInputMode GetNHInputMode() const { return InputMode; }

	/** Console: NHLighting Day | DustyNoon | Sunset | NightRain (or no argument for the next one) */
	UFUNCTION(Exec)
	void NHLighting(const FString& PresetName);

	/** Console: NHReset wipes the save and starts the story again (reload the level after) */
	UFUNCTION(Exec)
	void NHReset();

	/** Console: NHCash 50000 */
	UFUNCTION(Exec)
	void NHCash(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Naija|Vehicle") bool EnterVehicle(ANHVehicle* Vehicle);
	/** Steps out beside the vehicle. Refuses above walking pace unless bForce. */
	UFUNCTION(BlueprintCallable, Category = "Naija|Vehicle") bool LeaveVehicle(bool bForce = false);
	/** The nearest vehicle you can get into from here, if any */
	ANHVehicle* NearbyVehicle() const;
	/** The bottom-of-screen prompt for what F and E do right now */
	FString Prompt() const;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;

private:
	void OnCycleLighting();
	void OnInteract();
	void OnAction();
	void OnChoice1() { Choose(0); }
	void OnChoice2() { Choose(1); }
	void OnChoice3() { Choose(2); }
	void OnChoice4() { Choose(3); }
	void Choose(int32 Index);

	/** The on-foot character, kept while you drive */
	UPROPERTY(Transient)
	TObjectPtr<ANHCharacter> OnFootCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UNHInputSet> InputSet;

	ENHInputMode InputMode = ENHInputMode::OnFoot;
};
