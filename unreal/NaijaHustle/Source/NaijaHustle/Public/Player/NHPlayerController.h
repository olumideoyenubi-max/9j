#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NHPlayerController.generated.h"

class UNHInputSet;

UENUM(BlueprintType)
enum class ENHInputMode : uint8
{
	OnFoot,
	Vehicle,
	Menu
};

/** Owns the input set and decides which mapping contexts are active (on foot, driving, in a menu). */
UCLASS()
class NAIJAHUSTLE_API ANHPlayerController : public APlayerController
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

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void OnCycleLighting();

	UPROPERTY(Transient)
	TObjectPtr<UNHInputSet> InputSet;

	ENHInputMode InputMode = ENHInputMode::OnFoot;
};
