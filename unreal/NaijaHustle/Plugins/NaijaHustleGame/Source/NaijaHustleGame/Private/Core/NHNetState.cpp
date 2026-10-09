#include "Core/NHNetState.h"

#include "Core/NHGameData.h"
#include "Core/NHHustleSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Lighting/NHLightingRig.h"
#include "NaijaHustleGame.h"
#include "Net/UnrealNetwork.h"

// ----------------------------------------------------------------------------------------------- world state
ANHGameState::ANHGameState()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ANHGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANHGameState, Minutes);
	DOREPLIFETIME(ANHGameState, LightingPreset);
}

void ANHGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	ANHLightingRig* Rig = ANHLightingRig::Find(this);
	if (HasAuthority())
	{
		if (Hustle)
		{
			Minutes = Hustle->Minutes;
		}
		LightingPreset = Rig ? static_cast<uint8>(Rig->Preset) : 255;
	}
	else
	{
		// a client: the server's clock and sky
		if (Hustle)
		{
			Hustle->Minutes = Minutes;
		}
		if (Rig && LightingPreset != 255 && static_cast<uint8>(Rig->Preset) != LightingPreset)
		{
			Rig->ApplyPreset(static_cast<ENHLightingPreset>(LightingPreset));
		}
	}
}

// ---------------------------------------------------------------------------------------------- player state
ANHPlayerState::ANHPlayerState()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ANHPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ANHPlayerState, Cash, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ANHPlayerState, Cred, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ANHPlayerState, Integrity, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ANHPlayerState, Jobs, COND_OwnerOnly);
	DOREPLIFETIME(ANHPlayerState, Heat);
}

bool ANHPlayerState::IsHostWallet() const
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	return HasAuthority() && PC && PC->IsLocalController();
}

void ANHPlayerState::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && !IsHostWallet())
	{
		// somebody who joined: a wallet of their own (there is no account to load it from yet)
		const UNHGameData* Data = UNHGameData::Get(this);
		Cash = Data ? Data->StartCash : 5000;
		UE_LOG(LogNHGame, Log, TEXT("MONEY player=%s opened with %d"), *GetPlayerName(), Cash);
	}
}

void ANHPlayerState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	if (!Hustle)
	{
		return;
	}
	if (IsHostWallet())
	{
		// the host's money is the single-player store's: shown here for others (the wanted level) and for symmetry
		Cash = Hustle->Cash;
		Cred = Hustle->Cred;
		Integrity = Hustle->Integrity;
		Jobs = Hustle->Jobs;
		Heat = Hustle->Heat;
	}
	else if (!HasAuthority())
	{
		const APlayerController* PC = Cast<APlayerController>(GetOwner());
		if (PC && PC->IsLocalController())
		{
			// this client's own player: what the server says, put where the HUD reads it
			Hustle->Cash = Cash;
			Hustle->Cred = Cred;
			Hustle->Integrity = Integrity;
			Hustle->Jobs = Jobs;
			Hustle->Heat = Heat;
			// said now and then while the networking is being built, so a two-machine test can be read from the log
			if (GetWorld()->GetTimeSeconds() - LastSaid > 5.f)
			{
				LastSaid = GetWorld()->GetTimeSeconds();
				UE_LOG(LogNHGame, Log, TEXT("NET client %s: wallet %d, heat %.1f, clock %s, from the server"), *GetPlayerName(), Cash, Heat, *Hustle->ClockText());
			}
		}
	}
}

bool ANHPlayerState::Earn(int32 Amount, const FString& Why)
{
	if (!HasAuthority() || Amount == 0)
	{
		return false;
	}
	if (IsHostWallet())
	{
		if (UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this))
		{
			Hustle->Earn(Amount, Why); // logs it there
			Cash = Hustle->Cash;
		}
		return true;
	}
	Cash += Amount;
	UE_LOG(LogNHGame, Log, TEXT("MONEY player=%s %+d (%s) balance=%d"), *GetPlayerName(), Amount, *Why, Cash);
	return true;
}

bool ANHPlayerState::AddHeat(float Amount)
{
	if (!HasAuthority())
	{
		return false;
	}
	if (UNHHustleSubsystem* Hustle = IsHostWallet() ? UNHHustleSubsystem::Get(this) : nullptr)
	{
		Hustle->AddHeat(Amount);
		Heat = Hustle->Heat;
	}
	else
	{
		Heat = FMath::Min(5.f, Heat + Amount);
	}
	return true;
}

bool ANHPlayerState::ClearHeat()
{
	if (!HasAuthority())
	{
		return false;
	}
	if (UNHHustleSubsystem* Hustle = IsHostWallet() ? UNHHustleSubsystem::Get(this) : nullptr)
	{
		Hustle->ClearHeat();
	}
	Heat = 0.f;
	return true;
}
