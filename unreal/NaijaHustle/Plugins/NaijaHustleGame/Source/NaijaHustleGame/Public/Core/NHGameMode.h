#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NHGameMode.generated.h"

/**
 * The NAIJA HUSTLE game mode (server only): NHPlayerController, the on-foot character, the canvas HUD,
 * the replicated player and game state (money and the clock), and a game director (missions, conductor
 * shifts) spawned at the start of play. In your own project, set it
 * as the level's GameMode Override (build_street_block.py does that for L_Slice_Street), or subclass it
 * in Blueprint to swap in your own character.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANHGameMode();
	virtual void StartPlay() override;
	/** Loads the local save into the host's player state and gives the host the story's leads.
	 *  Other players start fresh, as their own character, for now. */
	virtual void PostLogin(APlayerController* NewPlayer) override;
};
