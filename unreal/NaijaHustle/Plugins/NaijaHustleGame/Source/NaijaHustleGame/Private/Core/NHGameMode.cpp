#include "Core/NHGameMode.h"

#include "Core/NHGameData.h"
#include "EngineUtils.h"
#include "Gameplay/NHGameDirector.h"
#include "Player/NHCharacter.h"
#include "Player/NHPlayerController.h"
#include "Phone/NHPhone.h"
#include "UI/NHHUD.h"
#include "Vehicles/NHTraffic.h"

ANHGameMode::ANHGameMode()
{
	DefaultPawnClass = ANHCharacter::StaticClass();
	PlayerControllerClass = ANHPlayerController::StaticClass();
	HUDClass = ANHHUD::StaticClass();
}

void ANHGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// the real-scale Lagos level has its own stops, motor park and road graph; every other level uses the small city's
	if (UNHGameData* Data = UNHGameData::Get(this))
	{
		Data->UseRealCity(MapName.Contains(TEXT("L_Lagos_City")));
	}
}

void ANHGameMode::StartPlay()
{
	Super::StartPlay();
	GetWorld()->SpawnActor<ANHPhone>(ANHPhone::StaticClass(), FTransform::Identity);
	const UNHGameData* Data = UNHGameData::Get(this);
	if (Data && Data->bRealCity)
	{
		GetWorld()->SpawnActor<ANHTraffic>(ANHTraffic::StaticClass(), FTransform::Identity);
	}
	if (!ANHGameDirector::Get(this))
	{
		GetWorld()->SpawnActor<ANHGameDirector>(ANHGameDirector::StaticClass(), FTransform::Identity);
	}
}
