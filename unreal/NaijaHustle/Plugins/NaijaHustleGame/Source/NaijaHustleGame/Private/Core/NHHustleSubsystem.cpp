#include "Core/NHHustleSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include "Core/NHGameData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NaijaHustleGame.h"

namespace NHSave
{
	const TCHAR* Slot = TEXT("NaijaHustle");
}

void UNHHustleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UNHGameData>();
	Super::Initialize(Collection);
	if (const UNHGameData* Data = GetGameInstance()->GetSubsystem<UNHGameData>())
	{
		Cash = Data->StartCash;
		Minutes = Data->StartMinutes;
		SecondsPerStar = Data->SecondsPerStar;
	}
	Load();
}

UNHHustleSubsystem* UNHHustleSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UNHHustleSubsystem>() : nullptr;
}

bool UNHHustleSubsystem::IsAuthority() const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	return !World || World->GetNetMode() != NM_Client;
}

void UNHHustleSubsystem::Earn(int32 Amount, const FString& Why)
{
	if (Amount == 0)
	{
		return;
	}
	if (!IsAuthority())
	{
		UE_LOG(LogNHGame, Warning, TEXT("MONEY refused on a client: %+d (%s). Only the server pays."), Amount, *Why);
		return;
	}
	Cash += Amount;
	UE_LOG(LogNHGame, Log, TEXT("MONEY player=host %+d (%s) balance=%d"), Amount, *Why, Cash);
	FNHLedgerEntry E;
	E.Minutes = Minutes;
	E.Amount = Amount;
	E.Why = Why;
	Ledger.Insert(E, 0);
	if (Ledger.Num() > 20)
	{
		Ledger.SetNum(20);
	}
	OnEarn.Broadcast(Amount, Why);
}

int32 UNHHustleSubsystem::Stars() const
{
	return FMath::CeilToInt(FMath::Min(Heat, 5.f) - 1e-4f);
}

void UNHHustleSubsystem::AddHeat(float Amount)
{
	if (!IsAuthority())
	{
		return;
	}
	Heat = FMath::Min(5.f, Heat + Amount);
	HeatTimer = 0.f;
}

void UNHHustleSubsystem::ClearHeat()
{
	if (!IsAuthority())
	{
		return;
	}
	Heat = 0.f;
	HeatTimer = 0.f;
}

void UNHHustleSubsystem::TickHeat(float DeltaSeconds, bool bSeen)
{
	if (!IsAuthority())
	{
		return;
	}
	if (Stars() <= 0)
	{
		Heat = 0.f;
		return;
	}
	if (bSeen)
	{
		HeatTimer = 0.f;
		return;
	}
	HeatTimer += DeltaSeconds;
	if (HeatTimer >= SecondsPerStar)
	{
		HeatTimer = 0.f;
		Heat = static_cast<float>(FMath::Max(0, Stars() - 1));
	}
}

void UNHHustleSubsystem::TickClock(float DeltaSeconds, float Scale)
{
	if (!IsAuthority())
	{
		return; // the server's clock arrives through ANHGameState
	}
	const UNHGameData* Data = GetGameInstance()->GetSubsystem<UNHGameData>();
	Minutes += DeltaSeconds * (Data ? Data->ClockMinutesPerSecond : 2.f) * Scale;
}

FString UNHHustleSubsystem::ClockText() const
{
	const int32 M = FMath::FloorToInt(FMath::Fmod(Minutes, 1440.f));
	return FString::Printf(TEXT("%02d:%02d  Day %d"), M / 60, M % 60, Day());
}

FString UNHHustleSubsystem::Naira(int32 Amount)
{
	// The engine's default font has no naira sign, so the HUD writes "N" (as on a Lagos price tag)
	FString Digits = FString::FormatAsNumber(FMath::Abs(Amount));
	return (Amount < 0 ? TEXT("-N") : TEXT("N")) + Digits;
}

FString UNHHustleSubsystem::Naira(int64 Amount)
{
	// 300,000,000,000 is a mouthful on a HUD: from a billion up it is written N300.00bn
	const int64 Abs = Amount < 0 ? -Amount : Amount;
	if (Abs >= 1000000000)
	{
		return FString::Printf(TEXT("%sN%.2fbn"), Amount < 0 ? TEXT("-") : TEXT(""), static_cast<double>(Abs) / 1e9);
	}
	return Naira(static_cast<int32>(Amount));
}

bool UNHHustleSubsystem::Pay(int64 Amount, const FString& Why)
{
	if (Amount <= 0 || !IsAuthority() || Amount > Worth())
	{
		return Amount == 0;
	}
	// what the pocket can cover comes out of the pocket; anything bigger is a transfer, and leaves the pocket alone if the bank can stand it
	const int32 FromPocket = Amount <= Cash ? static_cast<int32>(Amount) : Bank >= Amount ? 0 : static_cast<int32>(FMath::Min<int64>(Amount, FMath::Max(Cash, 0)));
	if (FromPocket > 0)
	{
		Earn(-FromPocket, Why);
	}
	if (Amount > FromPocket)
	{
		Bank -= Amount - FromPocket;
		UE_LOG(LogNHGame, Log, TEXT("MONEY player=host bank -%lld (%s) bank=%lld"), Amount - FromPocket, *Why, Bank);
	}
	return true;
}

void UNHHustleSubsystem::Bankroll(int64 Amount, const FString& Why)
{
	if (Amount > 0 && IsAuthority())
	{
		Bank += Amount;
		UE_LOG(LogNHGame, Log, TEXT("MONEY player=host bank +%lld (%s) bank=%lld"), Amount, *Why, Bank);
	}
}

void UNHHustleSubsystem::Save()
{
	if (!IsAuthority())
	{
		return; // a guest's numbers are the server's to keep, not this disk's
	}
	if (static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("NHEstateTest")) || FParse::Param(FCommandLine::Get(), TEXT("NHNoSave")); bTest)
	{
		return; // a test that plays the rich must not leave the player's own save a billionaire's
	}
	UNHSaveGame* S = Cast<UNHSaveGame>(UGameplayStatics::CreateSaveGameObject(UNHSaveGame::StaticClass()));
	if (!S)
	{
		return;
	}
	S->Cash = Cash;
	S->Cred = Cred;
	S->Integrity = Integrity;
	S->Minutes = Minutes;
	S->Jobs = Jobs;
	S->Done = Done;
	S->Outfit = Outfit;
	S->Ledger = Ledger;
	S->Bank = Bank;
	S->Persona = Persona;
	S->Owned = Owned;
	S->Home = Home;
	S->Cars = Cars;
	S->NextCar = NextCar;
	UGameplayStatics::SaveGameToSlot(S, NHSave::Slot, 0);
}

bool UNHHustleSubsystem::Load()
{
	if (!UGameplayStatics::DoesSaveGameExist(NHSave::Slot, 0))
	{
		return false;
	}
	const UNHSaveGame* S = Cast<UNHSaveGame>(UGameplayStatics::LoadGameFromSlot(NHSave::Slot, 0));
	if (!S)
	{
		return false;
	}
	Cash = S->Cash;
	Cred = S->Cred;
	Integrity = S->Integrity;
	Minutes = S->Minutes;
	Jobs = S->Jobs;
	Done = S->Done;
	Outfit = S->Outfit.IsNone() ? FName(TEXT("fit_street_basic")) : S->Outfit;
	Ledger = S->Ledger;
	Bank = S->Bank;
	Persona = S->Persona;
	Owned = S->Owned;
	Home = S->Home;
	Cars = S->Cars;
	NextCar = FMath::Max(S->NextCar, 1);
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: save loaded (%s, %d jobs done)"), *Naira(Cash), Done.Num());
	return true;
}

void UNHHustleSubsystem::ResetProgress()
{
	UGameplayStatics::DeleteGameInSlot(NHSave::Slot, 0);
	const UNHGameData* Data = GetGameInstance()->GetSubsystem<UNHGameData>();
	Cash = Data ? Data->StartCash : 5000;
	Minutes = Data ? Data->StartMinutes : 480.f;
	Cred = Integrity = Jobs = 0;
	Bank = 0;
	Persona = Home = NAME_None;
	Owned.Reset();
	Cars.Reset();
	Done.Reset();
	Ledger.Reset();
	Outfit = TEXT("fit_street_basic");
	ClearHeat();
}
