#include "Core/NHGameState.h"

#include "Core/NHGameData.h"
#include "Engine/World.h"
#include "Lighting/NHLightingRig.h"
#include "Net/UnrealNetwork.h"

ANHGameState::ANHGameState()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ANHGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANHGameState, Minutes);
	DOREPLIFETIME(ANHGameState, ClockScale);
}

void ANHGameState::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	const UNHGameData* Data = UNHGameData::Get(this);
	if (Data && HasAuthority())
	{
		Minutes = Data->StartMinutes;
	}
}

ANHGameState* ANHGameState::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetGameState<ANHGameState>() : nullptr;
}

void ANHGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UNHGameData* Data = UNHGameData::Get(this);
	// clients run the clock too between updates, so it moves smoothly; the server's value wins on each update
	Minutes += DeltaSeconds * (Data ? Data->ClockMinutesPerSecond : 2.f) * ClockScale;
	UpdateLighting(DeltaSeconds);
}

void ANHGameState::SetMinutes(float NewMinutes)
{
	if (HasAuthority())
	{
		Minutes = NewMinutes;
	}
}

void ANHGameState::SetClockScale(float Scale)
{
	if (HasAuthority())
	{
		ClockScale = Scale;
	}
}

FString ANHGameState::ClockText() const
{
	const int32 M = FMath::FloorToInt(FMath::Fmod(Minutes, 1440.f));
	return FString::Printf(TEXT("%02d:%02d  Day %d"), M / 60, M % 60, Day());
}

void ANHGameState::UpdateLighting(float DeltaSeconds)
{
	LightCheck -= DeltaSeconds;
	if (LightCheck > 0.f || bManualLighting)
	{
		return;
	}
	LightCheck = 2.f;
	ANHLightingRig* Rig = ANHLightingRig::Find(this);
	if (!Rig)
	{
		return;
	}
	const float H = HourOfDay();
	const ENHLightingPreset Want = (H < 6.f || H >= 19.5f) ? ENHLightingPreset::NightRain : H >= 17.5f ? ENHLightingPreset::Sunset : (H >= 11.5f && H < 15.f) ? ENHLightingPreset::DustyNoon : ENHLightingPreset::Day;
	if (static_cast<int32>(Want) != LastPreset)
	{
		LastPreset = static_cast<int32>(Want);
		Rig->ApplyPreset(Want);
	}
}
