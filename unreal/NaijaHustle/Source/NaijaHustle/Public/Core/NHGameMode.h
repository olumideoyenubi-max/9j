#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NHGameMode.generated.h"

/** Default game mode (set in DefaultEngine.ini): NHPlayerController + the step-1 test character. */
UCLASS()
class NAIJAHUSTLE_API ANHGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANHGameMode();
};
