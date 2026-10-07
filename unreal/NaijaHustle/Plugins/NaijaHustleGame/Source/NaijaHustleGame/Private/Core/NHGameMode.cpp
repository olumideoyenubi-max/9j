#include "Core/NHGameMode.h"

#include "EngineUtils.h"
#include "Gameplay/NHGameDirector.h"
#include "Player/NHCharacter.h"
#include "Player/NHPlayerController.h"
#include "UI/NHHUD.h"

ANHGameMode::ANHGameMode()
{
	DefaultPawnClass = ANHCharacter::StaticClass();
	PlayerControllerClass = ANHPlayerController::StaticClass();
	HUDClass = ANHHUD::StaticClass();
}

void ANHGameMode::StartPlay()
{
	Super::StartPlay();
	if (!ANHGameDirector::Get(this))
	{
		GetWorld()->SpawnActor<ANHGameDirector>(ANHGameDirector::StaticClass(), FTransform::Identity);
	}
}
