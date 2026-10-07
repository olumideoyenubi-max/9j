#include "Core/NHGameMode.h"

#include "Player/NHCharacter.h"
#include "Player/NHPlayerController.h"

ANHGameMode::ANHGameMode()
{
	DefaultPawnClass = ANHCharacter::StaticClass();
	PlayerControllerClass = ANHPlayerController::StaticClass();
}
