#include "Core/NHGameMode.h"

#include "Audio/NHAudioTest.h"
#include "Audio/NHAudioZone.h"
#include "Core/NHGameData.h"
#include "Core/NHNetState.h"
#include "EngineUtils.h"
#include "Gameplay/NHCrowd.h"
#include "Gameplay/NHGameDirector.h"
#include "Gameplay/NHLaw.h"
#include "Gameplay/NHResponse.h"
#include "Player/NHCharacter.h"
#include "Player/NHPlayerController.h"
#include "Phone/NHPhone.h"
#include "UI/NHHUD.h"
#include "Vehicles/NHCarTheft.h"
#include "Vehicles/NHTraffic.h"
#include "World/NHStreetScatter.h"
#include "World/NHStreets.h"

ANHGameMode::ANHGameMode()
{
	DefaultPawnClass = ANHCharacter::StaticClass();
	PlayerControllerClass = ANHPlayerController::StaticClass();
	HUDClass = ANHHUD::StaticClass();
	GameStateClass = ANHGameState::StaticClass();   // the clock and the sky, for everybody
	PlayerStateClass = ANHPlayerState::StaticClass(); // each player's money and wanted level, owned by the server
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
	GetWorld()->SpawnActor<ANHCarTheft>(ANHCarTheft::StaticClass(), FTransform::Identity);
	const UNHGameData* Data = UNHGameData::Get(this);
	if (Data && Data->bRealCity)
	{
		GetWorld()->SpawnActor<ANHTraffic>(ANHTraffic::StaticClass(), FTransform::Identity);
		GetWorld()->SpawnActor<ANHCrowd>(ANHCrowd::StaticClass(), FTransform::Identity); // the people on the pavements round the player
		if (FParse::Param(FCommandLine::Get(), TEXT("NHPopulationTest")))
		{
			GetWorld()->SpawnActor<ANHPopulationTest>(ANHPopulationTest::StaticClass(), FTransform::Identity); // counts them and the frame rate, and quits
		}
		GetWorld()->SpawnActor<ANHStreets>(ANHStreets::StaticClass(), FTransform::Identity); // real street names: banner, signs, map labels
		if (!FParse::Param(FCommandLine::Get(), TEXT("NHNoScatter"))) // -NHNoScatter: without, to measure what it costs
		{
			GetWorld()->SpawnActor<ANHStreetScatter>(ANHStreetScatter::StaticClass(), FTransform::Identity); // street clutter round the player
		}
	}
	GetWorld()->SpawnActor<ANHResponse>(ANHResponse::StaticClass(), FTransform::Identity); // who comes when you have wanted stars
	GetWorld()->SpawnActor<ANHLaw>(ANHLaw::StaticClass(), FTransform::Identity); // witnesses: a crime counts only if somebody reports it
	ANHAudioZone::SpawnCityZones(GetWorld()); // where the city sounds like a motor park or a market
	if (FParse::Param(FCommandLine::Get(), TEXT("NHAudioTest")))
	{
		// plays test tones through the mix, records the output, quits: see ANHAudioTest
		GetWorld()->SpawnActor<ANHAudioTest>(ANHAudioTest::StaticClass(), FTransform::Identity)->bQuitWhenDone = true;
	}
	if (!ANHGameDirector::Get(this))
	{
		GetWorld()->SpawnActor<ANHGameDirector>(ANHGameDirector::StaticClass(), FTransform::Identity);
	}
}
