#include "Core/NHPlayerState.h"

#include "Core/NHGameData.h"
#include "Core/NHGameState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

ANHPlayerState::ANHPlayerState()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ANHPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANHPlayerState, Cash);
	DOREPLIFETIME(ANHPlayerState, BoughtCash);
	DOREPLIFETIME(ANHPlayerState, Cred);
	DOREPLIFETIME(ANHPlayerState, Integrity);
	DOREPLIFETIME(ANHPlayerState, Jobs);
	DOREPLIFETIME(ANHPlayerState, Done);
	DOREPLIFETIME(ANHPlayerState, Flags);
	DOREPLIFETIME(ANHPlayerState, Outfit);
	DOREPLIFETIME(ANHPlayerState, Heat);
	DOREPLIFETIME_CONDITION(ANHPlayerState, Ledger, COND_OwnerOnly);
}

void ANHPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (const UNHGameData* Data = UNHGameData::Get(this))
	{
		SecondsPerStar = Data->SecondsPerStar;
	}
	ResetToStart();
}

ANHPlayerState* ANHPlayerState::Local(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC ? PC->GetPlayerState<ANHPlayerState>() : nullptr;
}

ANHPlayerState* ANHPlayerState::Host(const UObject* WorldContext)
{
	// the host is the player at the server's own screen: none on a client or a dedicated server
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}
	const APlayerController* PC = World->GetFirstPlayerController();
	return (PC && PC->IsLocalController()) ? PC->GetPlayerState<ANHPlayerState>() : nullptr;
}

// ---------------------------------------------------------------------------------------------------- server-side changes
void ANHPlayerState::Earn(int32 Amount, const FString& Why)
{
	if (!HasAuthority() || Amount == 0)
	{
		return;
	}
	Cash += Amount;
	// spending comes out of bought naira last, so bought naira never becomes riskable
	BoughtCash = FMath::Min(BoughtCash, FMath::Max(0, Cash));
	FNHLedgerEntry E;
	const ANHGameState* GS = ANHGameState::Get(this);
	E.Minutes = GS ? GS->Minutes : 0.f;
	E.Amount = Amount;
	E.Why = Why;
	Ledger.Insert(E, 0);
	if (Ledger.Num() > 20)
	{
		Ledger.SetNum(20);
	}
	// the server doesn't get OnRep calls, so a host sees its own change here
	SeenCash = Cash;
	bSeenCash = true;
	OnCashChanged.Broadcast(Amount, Why);
	ForceNetUpdate(); // player states only send about once a second otherwise
}

void ANHPlayerState::AddCred(int32 Amount)
{
	if (HasAuthority())
	{
		Cred = FMath::Max(0, Cred + Amount);
	}
}

void ANHPlayerState::AddIntegrity(int32 Amount)
{
	if (HasAuthority())
	{
		Integrity = FMath::Clamp(Integrity + Amount, -100, 100);
	}
}

void ANHPlayerState::AddJobs(int32 Count)
{
	if (HasAuthority())
	{
		Jobs += Count;
	}
}

void ANHPlayerState::MarkDone(FName Mission)
{
	if (HasAuthority())
	{
		Done.AddUnique(Mission);
	}
}

void ANHPlayerState::SetFlag(FName Flag, bool bOn)
{
	if (!HasAuthority())
	{
		return;
	}
	if (bOn)
	{
		Flags.AddUnique(Flag);
	}
	else
	{
		Flags.Remove(Flag);
	}
}

int32 ANHPlayerState::Stars() const
{
	return FMath::CeilToInt(FMath::Min(Heat, 5.f) - 1e-4f);
}

void ANHPlayerState::AddHeat(float Amount)
{
	if (HasAuthority())
	{
		Heat = FMath::Min(5.f, Heat + Amount);
		HeatTimer = 0.f;
		ForceNetUpdate();
	}
}

void ANHPlayerState::ClearHeat()
{
	if (HasAuthority())
	{
		Heat = 0.f;
		HeatTimer = 0.f;
	}
}

void ANHPlayerState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	// no Task Force on the streets yet, so the heat cools off unseen: a star every SecondsPerStar
	if (Stars() <= 0)
	{
		Heat = 0.f;
		return;
	}
	HeatTimer += DeltaSeconds;
	if (HeatTimer >= SecondsPerStar)
	{
		HeatTimer = 0.f;
		Heat = static_cast<float>(FMath::Max(0, Stars() - 1));
	}
}

void ANHPlayerState::OnRep_Cash()
{
	if (bSeenCash && Cash != SeenCash)
	{
		const FNHLedgerEntry* Last = Ledger.Num() > 0 ? &Ledger[0] : nullptr;
		OnCashChanged.Broadcast(Cash - SeenCash, Last ? Last->Why : FString());
	}
	SeenCash = Cash;
	bSeenCash = true;
}

// ---------------------------------------------------------------------------------------------------- save
void ANHPlayerState::WriteTo(UNHSaveGame& Save) const
{
	Save.Cash = Cash;
	Save.BoughtCash = BoughtCash;
	Save.Cred = Cred;
	Save.Integrity = Integrity;
	Save.Jobs = Jobs;
	Save.Done = Done;
	Save.Flags = Flags;
	Save.Outfit = Outfit;
	Save.Ledger = Ledger;
}

void ANHPlayerState::ReadFrom(const UNHSaveGame& Save)
{
	if (!HasAuthority())
	{
		return;
	}
	Cash = Save.Cash;
	BoughtCash = FMath::Clamp(Save.BoughtCash, 0, FMath::Max(0, Save.Cash));
	Cred = Save.Cred;
	Integrity = FMath::Clamp(Save.Integrity, -100, 100);
	Jobs = Save.Jobs;
	Done = Save.Done;
	Flags = Save.Flags;
	Outfit = Save.Outfit.IsNone() ? FName(TEXT("fit_street_basic")) : Save.Outfit;
	Ledger = Save.Ledger;
	SeenCash = Cash;
	bSeenCash = true;
}

void ANHPlayerState::ResetToStart()
{
	if (!HasAuthority())
	{
		return;
	}
	const UNHGameData* Data = UNHGameData::Get(this);
	Cash = Data ? Data->StartCash : 5000;
	BoughtCash = Cred = Integrity = Jobs = 0;
	Done.Reset();
	Flags.Reset();
	Ledger.Reset();
	Outfit = TEXT("fit_street_basic");
	Heat = HeatTimer = 0.f;
	SeenCash = Cash;
	bSeenCash = true;
}
