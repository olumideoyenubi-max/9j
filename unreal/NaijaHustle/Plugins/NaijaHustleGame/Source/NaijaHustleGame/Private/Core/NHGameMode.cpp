#include "Core/NHGameMode.h"

#include "Core/NHGameState.h"
#include "Core/NHHustleSubsystem.h"
#include "Core/NHPlayerState.h"
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
	PlayerStateClass = ANHPlayerState::StaticClass();
	GameStateClass = ANHGameState::StaticClass();
}

void ANHGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	// the save on this machine belongs to the player at this machine: the host
	if (NewPlayer && NewPlayer->IsLocalController())
	{
		if (UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this))
		{
			Hustle->LoadInto(NewPlayer->GetPlayerState<ANHPlayerState>(), GetGameState<ANHGameState>());
		}
		// the story's leads (Tunde, Amaka, and whoever the save has unlocked) belong to the host
		if (ANHPlayerController* PC = Cast<ANHPlayerController>(NewPlayer))
		{
			PC->RefreshLeads();
		}
	}
}

void ANHGameMode::StartPlay()
{
	Super::StartPlay();
	if (!ANHGameDirector::Get(this))
	{
		GetWorld()->SpawnActor<ANHGameDirector>(ANHGameDirector::StaticClass(), FTransform::Identity);
	}
}
