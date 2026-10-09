#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NHGameMode.generated.h"

/**
 * The NAIJA HUSTLE game mode: NHPlayerController, the on-foot character, the canvas HUD, and a game
 * director (missions, conductor shifts, clock) spawned at the start of play. In your own project, set it
 * as the level's GameMode Override (build_street_block.py does that for L_Slice_Street), or subclass it
 * in Blueprint to swap in your own character.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANHGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
};
