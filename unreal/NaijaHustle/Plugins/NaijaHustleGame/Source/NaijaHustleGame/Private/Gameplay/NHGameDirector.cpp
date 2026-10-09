#include "Gameplay/NHGameDirector.h"

#include "Core/NHHustleSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/NHPerson.h"
#include "Kismet/GameplayStatics.h"
#include "Lighting/NHLightingRig.h"
#include "NaijaHustleGame.h"
#include "Player/NHPlayerController.h"
#include "UI/NHHUD.h"
#include "Vehicles/NHVehicle.h"

namespace NHDir
{
	const FName FirstDay(TEXT("lag_01"));
	const FName BaloGate(TEXT("balogate"));
	const FLinearColor Shirts[] = { FLinearColor(0.75f, 0.14f, 0.05f), FLinearColor(0.03f, 0.37f, 0.15f), FLinearColor(0.7f, 0.08f, 0.26f), FLinearColor(0.05f, 0.2f, 0.7f),
		FLinearColor(0.91f, 0.55f, 0.f), FLinearColor(0.27f, 0.05f, 0.4f), FLinearColor(0.02f, 0.36f, 0.27f), FLinearColor(0.85f, 0.83f, 0.78f) };

	template <typename T> const T& Pick(const TArray<T>& A) { return A[FMath::RandRange(0, A.Num() - 1)]; }
	FString N(int32 Amount) { return UNHHustleSubsystem::Naira(Amount); }
	int32 Round50(float V) { return FMath::RoundToInt(V / 50.f) * 50; }
}

ANHGameDirector::ANHGameDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ANHGameDirector* ANHGameDirector::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<ANHGameDirector> It(World);
	return It ? *It : nullptr;
}

const UNHGameData* ANHGameDirector::Data() const
{
	return UNHGameData::Get(this);
}

// ---------------------------------------------------------------------------------------------------- setup
void ANHGameDirector::BeginPlay()
{
	Super::BeginPlay();
	const UNHGameData* D = Data();
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	if (!D || !D->bLoaded || !Hustle)
	{
		UE_LOG(LogNHGame, Error, TEXT("NHGameDirector: no game data, so no missions. Check the plugin's Data folder."));
		return;
	}

	// Baba Driver by the bays; the mission danfo in bay 1; more danfos, a keke and an okada in the park
	Baba = GetWorld()->SpawnActor<ANHPerson>(ANHPerson::StaticClass(), Ground(D->Park + FVector2D(275.f, -150.f)), FRotator::ZeroRotator);
	if (Baba.IsValid())
	{
		Baba->Init(7, FLinearColor(0.32f, 0.17f, 0.08f), false, 1.05f);
	}
	if (!Hustle->IsDone(NHDir::FirstDay))
	{
		SpawnMissionBus();
		Stage = EStage::Meet;
	}
	else
	{
		Stage = EStage::Done;
		if (D->ParkBays.Num() > 0)
		{
			SpawnVehicle(TEXT("danfo"), D->ParkBays[0].Pos, D->ParkBays[0].Yaw, D->Spec(TEXT("danfo")).Colors.Num() ? D->Spec(TEXT("danfo")).Colors[0] : FLinearColor(0.9f, 0.55f, 0.f), TEXT("YABA"));
		}
	}
	const TCHAR* Boards[] = { TEXT("OSHODI"), TEXT("EKO"), TEXT("IYA B.") };
	for (int32 I = 1; I <= 3 && I < D->ParkBays.Num(); ++I)
	{
		SpawnVehicle(TEXT("danfo"), D->ParkBays[I].Pos, D->ParkBays[I].Yaw, FLinearColor(0.9f, 0.55f, 0.f), Boards[I - 1]);
	}
	if (D->ParkBays.Num() > 5)
	{
		SpawnVehicle(TEXT("keke"), D->ParkBays[4].Pos, D->ParkBays[4].Yaw, FLinearColor(0.9f, 0.55f, 0.f), FString());
		SpawnVehicle(TEXT("okada"), D->ParkBays[5].Pos, D->ParkBays[5].Yaw, FLinearColor(0.6f, 0.05f, 0.05f), FString());
	}
	if (D->ParkBays.Num() > 6)
	{
		SpawnVehicle(TEXT("sedan"), D->ParkBays[6].Pos, D->ParkBays[6].Yaw, FLinearColor(0.12f, 0.2f, 0.3f), FString());
	}
	for (const FNHParkedVehicle& P : D->Parked) // the luxury cars: two at the park, the rest on Lagos Island
	{
		SpawnVehicle(P.Type, P.Pos, P.Yaw, P.Color, FString());
	}
	ANHHUD::Toast(this, TEXT("NAIJA HUSTLE: welcome to Lagos. Find Baba Driver at Oshodi Motor Park."), 1);
}

ANHVehicle* ANHGameDirector::SpawnVehicle(FName Type, const FVector2D& Pos, float Yaw, const FLinearColor& Paint, const FString& Board)
{
	const FTransform T(FRotator(0.f, Yaw, 0.f), Ground(Pos, 150.f));
	ANHVehicle* V = GetWorld()->SpawnActorDeferred<ANHVehicle>(ANHVehicle::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!V)
	{
		return nullptr;
	}
	V->VehicleType = Type;
	V->Paint = Paint;
	V->Board = Board;
	UGameplayStatics::FinishSpawningActor(V, T);
	return V;
}

void ANHGameDirector::SpawnMissionBus()
{
	const UNHGameData* D = Data();
	if (MissionBus.IsValid())
	{
		MissionBus->Destroy();
	}
	if (D && D->ParkBays.Num() > 0)
	{
		MissionBus = SpawnVehicle(TEXT("danfo"), D->ParkBays[0].Pos, D->ParkBays[0].Yaw, FLinearColor(0.9f, 0.55f, 0.f), TEXT("YABA"));
	}
}

// ---------------------------------------------------------------------------------------------------- helpers
FVector ANHGameDirector::Ground(const FVector2D& P, float Up) const
{
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(NHDirectorGround), false);
	const FVector From(P.X, P.Y, 3000.f), To(P.X, P.Y, -1000.f);
	float Z = 16.f;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, From, To, FCollisionObjectQueryParams(ECC_WorldStatic), Q))
	{
		Z = Hit.ImpactPoint.Z;
	}
	return FVector(P.X, P.Y, Z + Up);
}

FVector ANHGameDirector::StopKerb(FName StopId) const
{
	const FNHBusStop* S = Data() ? Data()->Stops.Find(StopId) : nullptr;
	return S ? Ground(S->Kerb) : FVector::ZeroVector;
}

FVector ANHGameDirector::StopWait(FName StopId) const
{
	const FNHBusStop* S = Data() ? Data()->Stops.Find(StopId) : nullptr;
	return S ? Ground(S->Wait) : FVector::ZeroVector;
}

FVector ANHGameDirector::QueueSpot(FName StopId, int32 Slot) const
{
	// waiting passengers line up along the pavement beside the stop, a second row behind the first
	const FNHBusStop* S = Data() ? Data()->Stops.Find(StopId) : nullptr;
	if (!S)
	{
		return FVector::ZeroVector;
	}
	const bool bAlongX = FMath::Abs(S->Wait.Y - S->Kerb.Y) > FMath::Abs(S->Wait.X - S->Kerb.X);
	const FVector2D Away = (S->Wait - S->Kerb).GetSafeNormal();
	const float Off = (Slot % 2 ? 1.f : -1.f) * (Slot / 2 + 0.5f) * 85.f;
	const FVector2D P = S->Wait + (bAlongX ? FVector2D(Off, 0.f) : FVector2D(0.f, Off)) + Away * (Slot > 5 ? 70.f : 0.f);
	return Ground(P);
}

APawn* ANHGameDirector::PlayerPawn() const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	return PC ? PC->GetPawn() : nullptr;
}

ANHVehicle* ANHGameDirector::PlayerVehicle() const
{
	return Cast<ANHVehicle>(PlayerPawn());
}

FString ANHGameDirector::StopName(FName StopId) const
{
	if (const FString* Label = Shift.Route.Labels.Find(StopId))
	{
		return *Label;
	}
	const FNHBusStop* S = Data() ? Data()->Stops.Find(StopId) : nullptr;
	return S ? S->Name : StopId.ToString();
}

void ANHGameDirector::Say(const FString& Speaker, const TArray<FString>& Lines, TFunction<void()> OnDone)
{
	if (Lines.Num() == 0)
	{
		if (OnDone)
		{
			OnDone();
		}
		return;
	}
	Dialogue.bOpen = true;
	Dialogue.Speaker = Speaker;
	Dialogue.Lines = Lines;
	Dialogue.Index = 0;
	Dialogue.T = 0.f;
	Dialogue.OnDone = MoveTemp(OnDone);
}

void ANHGameDirector::OpenPanel(const FString& Title, const TArray<FString>& Lines, const TArray<FString>& Options, TFunction<void(int32)> OnChoose)
{
	Panel.bOpen = true;
	Panel.Title = Title;
	Panel.Lines = Lines;
	Panel.Options = Options;
	Panel.OnChoose = MoveTemp(OnChoose);
}

// ---------------------------------------------------------------------------------------------------- input
void ANHGameDirector::OnChoice(int32 Index)
{
	if (!Panel.bOpen || !Panel.Options.IsValidIndex(Index))
	{
		return;
	}
	TFunction<void(int32)> Choose = MoveTemp(Panel.OnChoose);
	Panel = FPanel(); // close first: the choice may open the next panel
	if (Choose)
	{
		Choose(Index);
	}
}

void ANHGameDirector::OnAction(APawn* Pawn)
{
	if (Dialogue.bOpen)
	{
		if (++Dialogue.Index >= Dialogue.Lines.Num())
		{
			TFunction<void()> Done = MoveTemp(Dialogue.OnDone);
			Dialogue = FDialogue();
			if (Done)
			{
				Done();
			}
		}
		Dialogue.T = 0.f;
		return;
	}
	if (Panel.bOpen || !Pawn)
	{
		return;
	}
	const UNHGameData* D = Data();
	if (ANHVehicle* V = Cast<ANHVehicle>(Pawn))
	{
		if (Shift.bOn && Shift.Bus.Get() == V)
		{
			if (!Shift.AtStop.IsNone())
			{
				CallPassengers();
			}
			else if (!Shift.bMission)
			{
				OpenShiftMenu();
			}
			else
			{
				ANHHUD::Toast(this, FString::Printf(TEXT("Drive to %s and stop at the kerb."), *StopName(Shift.Route.Stops[Shift.Idx])));
			}
		}
		else if (!Shift.bOn && V->VehicleType == TEXT("danfo") && (V != MissionBus.Get() || Stage == EStage::Done))
		{
			OpenRoutePicker(V);
		}
		return;
	}
	// on foot: talk to Baba Driver
	if (Baba.IsValid() && FVector::Dist2D(Pawn->GetActorLocation(), Baba->GetActorLocation()) < 350.f && D)
	{
		Baba->FaceTowards(Pawn->GetActorLocation());
		if (Stage == EStage::Meet)
		{
			Stage = EStage::Talk;
			Say(TEXT("Baba Driver"), D->BabaLines(TEXT("intro")), [this]() { Stage = EStage::Board; });
		}
		else if (Stage == EStage::Failed)
		{
			RetryFirstDay();
		}
		else
		{
			Say(TEXT("Baba Driver"), { Stage == EStage::Done ? TEXT("My conductor! Motor dey bay. Take any danfo, press E and pick your route.") : TEXT("Wetin you dey find here? The danfo dey wait for you.") }, nullptr);
		}
	}
}

FString ANHGameDirector::ActionPrompt(const APawn* Pawn) const
{
	if (IsBusy() || !Pawn)
	{
		return FString();
	}
	if (const ANHVehicle* V = Cast<ANHVehicle>(Pawn))
	{
		if (Shift.bOn && Shift.Bus.Get() == V)
		{
			if (!Shift.AtStop.IsNone())
			{
				const TArray<FPax>* W = Shift.Waiting.Find(Shift.AtStop);
				return FString::Printf(TEXT("E  Call passengers (%d waiting)"), W ? W->Num() : 0);
			}
			return Shift.bMission ? FString() : TEXT("E  Shift menu");
		}
		if (!Shift.bOn && V->VehicleType == TEXT("danfo") && (V != MissionBus.Get() || Stage == EStage::Done))
		{
			return TEXT("E  Pick a route");
		}
		return FString();
	}
	if (Baba.IsValid() && FVector::Dist2D(Pawn->GetActorLocation(), Baba->GetActorLocation()) < 350.f)
	{
		return TEXT("E  Talk to Baba Driver");
	}
	return FString();
}

// ---------------------------------------------------------------------------------------------------- tick
void ANHGameDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	if (!Hustle || !Data() || !Data()->bLoaded)
	{
		return;
	}
	Hustle->TickClock(DeltaSeconds, bSlowClock ? 0.5f : 1.f);
	Hustle->TickHeat(DeltaSeconds, false); // no Task Force on the streets yet: heat cools off unseen
	UpdateLighting(DeltaSeconds);

	if (Dialogue.bOpen)
	{
		Dialogue.T += DeltaSeconds;
	}
	if (Baba.IsValid() && !Baba->IsWalking())
	{
		if (const APawn* P = PlayerPawn())
		{
			if (FVector::Dist2D(P->GetActorLocation(), Baba->GetActorLocation()) < 900.f)
			{
				Baba->FaceTowards(P->GetActorLocation());
			}
		}
	}

	UpdateShift(DeltaSeconds);
	UpdateFirstDay(DeltaSeconds);
	if (!Shift.ChangeQ.IsEmpty() && !Panel.bOpen && !Dialogue.bOpen)
	{
		OpenChangePanel();
	}

	// the HUD's shift status
	ShiftLine.Reset();
	NextStopLine.Reset();
	if (Shift.bOn)
	{
		const FTotals T = Totals();
		ShiftLine = FString::Printf(TEXT("Seats %d/%d   Bag %s   Comfort %d%%"), Shift.Onboard.Num(), Data()->Conductor.Capacity,
			*NHDir::N(Shift.Fares + Shift.Tips + Shift.Bonus + Shift.Keep), FMath::RoundToInt(Shift.Comfort));
		if (Shift.bRouteDone)
		{
			NextStopLine = TEXT("Route done");
		}
		else if (Shift.Route.Stops.IsValidIndex(Shift.Idx))
		{
			const FName Next = Shift.AtStop.IsNone() ? Shift.Route.Stops[Shift.Idx] : Shift.AtStop;
			NextStopLine = FString::Printf(TEXT("%s %s (%d/%d)   take-home %s"), Shift.AtStop.IsNone() ? TEXT("Next:") : TEXT("At:"), *StopName(Next), Shift.Route.Stops.IndexOfByKey(Next) + 1, Shift.Route.Stops.Num(), *NHDir::N(T.Net));
		}
	}
}

void ANHGameDirector::UpdateLighting(float DeltaSeconds)
{
	LightCheck -= DeltaSeconds;
	if (LightCheck > 0.f || bManualLighting)
	{
		return;
	}
	LightCheck = 2.f;
	const UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	ANHLightingRig* Rig = ANHLightingRig::Find(this);
	if (!Hustle || !Rig)
	{
		return;
	}
	const float H = Hustle->HourOfDay();
	const ENHLightingPreset Want = (H < 6.f || H >= 19.5f) ? ENHLightingPreset::NightRain : H >= 17.5f ? ENHLightingPreset::Sunset : H >= 16.f ? ENHLightingPreset::GoldenEvening : (H >= 11.5f && H < 15.f) ? ENHLightingPreset::DustyNoon : (H >= 8.f && H < 11.5f) ? ENHLightingPreset::HarshMorning : ENHLightingPreset::Day;
	if (static_cast<int32>(Want) != LastPreset)
	{
		LastPreset = static_cast<int32>(Want);
		Rig->ApplyPreset(Want);
	}
}

void ANHGameDirector::OpenLightingMenu()
{
	if (IsBusy())
	{
		return;
	}
	OpenPanel(TEXT("Lighting (debug)"), { TEXT("Pick a time of day to look at, or let the clock decide."), TEXT("L steps through every preset.") },
		{ TEXT("Harsh morning"), TEXT("Golden evening"), TEXT("Follow the clock"), TEXT("Close") }, [this](int32 Choice)
		{
			ANHLightingRig* Rig = ANHLightingRig::Find(this);
			if (Choice == 2)
			{
				bManualLighting = false;
				LastPreset = -1;
				LightCheck = 0.f;
			}
			else if (Rig && Choice < 2)
			{
				bManualLighting = true;
				Rig->ApplyPreset(Choice == 0 ? ENHLightingPreset::HarshMorning : ENHLightingPreset::GoldenEvening);
			}
		});
}

// ---------------------------------------------------------------------------------------------------- "First Day on the Danfo"
void ANHGameDirector::UpdateFirstDay(float DeltaSeconds)
{
	const UNHGameData* D = Data();
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	APawn* Pawn = PlayerPawn();
	auto Obj = [&](int32 I) { return D->FirstDayObjectives.IsValidIndex(I) ? D->FirstDayObjectives[I] : FString(); };
	auto Sub = [&](int32 I) { return D->FirstDaySubs.IsValidIndex(I) ? D->FirstDaySubs[I] : FString(); };
	const FVector Park = Ground(D->Park);
	DeadlineMinutesLeft = (Deadline > 0.f && (Stage == EStage::Route || Stage == EStage::Return)) ? FMath::Max(0.f, Deadline - Hustle->Minutes) : -1.f;
	RouteStopPoints.Reset();
	if (Shift.bOn && !Shift.bRouteDone)
	{
		for (int32 I = Shift.Idx; I < Shift.Route.Stops.Num(); ++I)
		{
			RouteStopPoints.Add(StopKerb(Shift.Route.Stops[I]));
		}
	}

	if (Stage == EStage::Done || Stage == EStage::Failed)
	{
		bSlowClock = false;
		if (!Shift.bOn)
		{
			ObjTitle = Stage == EStage::Done ? TEXT("FREE ROAM") : D->FirstDayTitle.ToUpper();
			ObjText = Stage == EStage::Done ? TEXT("Hustle: take a danfo from Oshodi Motor Park") : TEXT("Talk to Baba Driver to try again");
			ObjSub = Stage == EStage::Done ? TEXT("Get in, press E to pick a route. More jobs come in the next update.") : FString();
			bMarker = Stage == EStage::Failed;
			Marker = Park;
		}
		else
		{
			ObjTitle = TEXT("CONDUCTOR SHIFT");
			ObjText = Shift.Route.Name;
			ObjSub = TEXT("Stop at each bus stop, press E to call passengers, give correct change.");
			bMarker = !Shift.bRouteDone && Shift.Route.Stops.IsValidIndex(Shift.Idx);
			if (bMarker)
			{
				Marker = StopKerb(Shift.AtStop.IsNone() ? Shift.Route.Stops[Shift.Idx] : Shift.AtStop);
			}
		}
		return;
	}

	ObjTitle = D->FirstDayTitle.ToUpper();
	ANHVehicle* Bus = MissionBus.Get();
	if (Stage >= EStage::Board && Stage <= EStage::Return)
	{
		if (!Bus || Bus->IsWrecked())
		{
			FailFirstDay(TEXT("wrecked"));
			return;
		}
		if (Stage != EStage::Board && Deadline > 0.f && Hustle->Minutes > Deadline)
		{
			FailFirstDay(TEXT("late"));
			return;
		}
	}
	switch (Stage)
	{
	case EStage::Meet:
		ObjText = Obj(0);
		ObjSub = TEXT("Walk up to him and press E");
		bMarker = true;
		Marker = Park;
		if (Pawn && Baba.IsValid() && FVector::Dist2D(Pawn->GetActorLocation(), Park) < 750.f && !Dialogue.bOpen)
		{
			Stage = EStage::Talk;
			Baba->FaceTowards(Pawn->GetActorLocation());
			Say(TEXT("Baba Driver"), D->BabaLines(TEXT("intro")), [this]() { Stage = EStage::Board; });
		}
		break;
	case EStage::Talk:
		ObjText = Obj(0);
		ObjSub = FString();
		bMarker = false;
		break;
	case EStage::Board:
		ObjText = Obj(1);
		ObjSub = Sub(1);
		bMarker = Bus != nullptr;
		Marker = Bus ? Bus->GetActorLocation() : Park;
		if (Bus && PlayerVehicle() == Bus)
		{
			StartShift(D->FirstRoute, Bus, true, D->Conductor.FirstCut, TEXT("Baba Driver"));
			Deadline = Hustle->Minutes + D->FirstDayClock;
			if (D->bRealCity)
			{
				// real distances: time for the whole round at a danfo's pace in traffic, a minute a stop, and half as much again
				float Round = 0.f;
				FVector2D From = D->Park;
				for (const FName& Id : D->FirstRoute.Stops)
				{
					if (const FNHBusStop* Stop = D->Stops.Find(Id))
					{
						Round += FVector2D::Distance(From, Stop->Kerb);
						From = Stop->Kerb;
					}
				}
				Round += FVector2D::Distance(From, D->Park);
				const float Seconds = (Round / 1200.f + 60.f * D->FirstRoute.Stops.Num()) * 1.5f;
				Deadline = Hustle->Minutes + FMath::Max(D->FirstDayClock, Seconds * D->ClockMinutesPerSecond * 0.5f); // the clock runs at half speed meanwhile
			}
			bSlowClock = true; // the clock runs at half speed while the deadline counts
			Stage = EStage::Route;
		}
		break;
	case EStage::Route:
		ObjText = Obj(2);
		if (PlayerVehicle() != Bus)
		{
			ObjSub = TEXT("Get back in the danfo");
			bMarker = true;
			Marker = Bus->GetActorLocation();
		}
		else
		{
			ObjSub = TEXT("Stop at each bus stop and press E to call passengers");
			bMarker = Shift.Route.Stops.IsValidIndex(Shift.Idx);
			if (bMarker)
			{
				Marker = StopKerb(Shift.AtStop.IsNone() ? Shift.Route.Stops[Shift.Idx] : Shift.AtStop);
			}
		}
		if (Shift.bRouteDone)
		{
			Stage = EStage::Return;
			ANHHUD::Toast(this, TEXT("Route done. Bring the danfo back to Baba Driver."), 1);
		}
		break;
	case EStage::Return:
		ObjText = Obj(3);
		ObjSub = Sub(3);
		bMarker = true;
		Marker = Park;
		if (PlayerVehicle() == Bus && FVector::Dist2D(Bus->GetActorLocation(), Park) < 875.f)
		{
			Bus->SetHeld(true); // no driving off while Baba Driver counts the money
			Stage = EStage::Wrapping;
			if (Baba.IsValid())
			{
				Baba->FaceTowards(Bus->GetActorLocation());
			}
			Say(TEXT("Baba Driver"), D->BabaLines(TEXT("done")), [this]()
			{
				EndShift(Data()->FirstDayTitle, [this]() { PassFirstDay(); });
			});
		}
		break;
	default:
		break;
	}
}

void ANHGameDirector::FailFirstDay(const FString& Reason)
{
	ClearPassengers();
	Shift = FShift();
	Stage = EStage::Failed;
	Deadline = -1.f;
	bSlowClock = false;
	Say(TEXT("Baba Driver"), Data()->BabaLines(Reason), [this, Reason]()
	{
		OpenPanel(TEXT("WAHALA!"), { Reason == TEXT("late") ? TEXT("You ran out of time.") : TEXT("The danfo is wrecked."), TEXT("Baba Driver is not happy.") },
			{ TEXT("Try again"), TEXT("Not now") }, [this](int32 Choice) { if (Choice == 0) RetryFirstDay(); });
	});
}

void ANHGameDirector::RetryFirstDay()
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	if (Hustle && Hustle->Stars() > 0)
	{
		Hustle->ClearHeat();
	}
	if (ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		PC->LeaveVehicle(true);
		if (APawn* P = PC->GetPawn())
		{
			P->SetActorLocation(Ground(Data()->Park + FVector2D(0.f, 250.f), 100.f));
		}
	}
	SpawnMissionBus();
	Stage = EStage::Talk;
	Say(TEXT("Baba Driver"), Data()->BabaLines(TEXT("retry")), [this]() { Stage = EStage::Board; });
}

void ANHGameDirector::PassFirstDay()
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	Stage = EStage::Done;
	Deadline = -1.f;
	bSlowClock = false;
	if (MissionBus.IsValid())
	{
		MissionBus->SetHeld(false);
	}
	if (Hustle && !Hustle->IsDone(NHDir::FirstDay))
	{
		Hustle->Cred += Data()->FirstDayCred;
		Hustle->Done.Add(NHDir::FirstDay);
		Hustle->Save();
	}
	ANHHUD::Toast(this, FString::Printf(TEXT("JOB DONE!  %s   +%d cred"), *Data()->FirstDayTitle, Data()->FirstDayCred), 1);
	ANHHUD::Toast(this, TEXT("Baba Driver gave you a danfo union card and the vehicle papers."), 1);
}

// ---------------------------------------------------------------------------------------------------- conductor shift
void ANHGameDirector::StartShift(const FNHRoute& Route, ANHVehicle* Bus, bool bMission, float Cut, const FString& Owner)
{
	if (!Bus || Route.Stops.Num() == 0)
	{
		return;
	}
	ClearPassengers();
	Shift = FShift();
	Shift.bOn = true;
	Shift.Route = Route;
	Shift.Bus = Bus;
	Shift.bMission = bMission;
	Shift.Cut = Cut;
	Shift.Owner = Owner;
	Shift.HpStart = Bus->Health;
	Shift.LastSpeed = Bus->Speed;
	Shift.LastYaw = Bus->GetActorRotation().Yaw;
	Shift.Refill = Data()->Conductor.Refill;
	// a free shift starts at the route's stop nearest the bus
	if (!bMission)
	{
		float Best = TNumericLimits<float>::Max();
		for (int32 I = 0; I < Route.Stops.Num(); ++I)
		{
			const float D = FVector::Dist2D(StopKerb(Route.Stops[I]), Bus->GetActorLocation());
			if (D < Best)
			{
				Best = D;
				Shift.Idx = I;
			}
		}
	}
	for (const FName& Id : Route.Stops)
	{
		TArray<FPax>& Queue = Shift.Waiting.Add(Id);
		const int32 Count = 2 + FMath::RandRange(0, FMath::Max(0, Route.Busy - 2));
		for (int32 K = 0; K < Count; ++K)
		{
			FPax P = MakePax(Id);
			SpawnWaiting(Id, P, Queue.Num());
			Queue.Add(P);
		}
	}
	ANHHUD::Toast(this, FString::Printf(TEXT("Route: %s. First stop %s."), *Route.Name, *StopName(Route.Stops[Shift.Idx])), 1);
}

ANHGameDirector::FPax ANHGameDirector::MakePax(FName StopId)
{
	const FNHRoute& R = Shift.Route;
	const int32 I = R.Stops.IndexOfByKey(StopId);
	FPax P;
	P.Id = ++PaxId;
	P.Stop = StopId;
	int32 Fare = 200;
	if (R.bLinear)
	{
		TArray<FName> Later;
		for (int32 K = I + 1; K < R.Stops.Num(); ++K)
		{
			Later.Add(R.Stops[K]);
		}
		P.Dest = Later.Num() ? NHDir::Pick(Later) : NAME_None; // none: rides to the park at the end
		Fare = FMath::Clamp(100 + (P.Dest.IsNone() ? 1 : Later.IndexOfByKey(P.Dest) + 1) * 100 + (FMath::FRand() < 0.5f ? 50 : 0), R.FareLo, R.FareHi);
	}
	else
	{
		const int32 MaxHops = FMath::Min(4, R.Stops.Num() - 1);
		const int32 Hops = 1 + FMath::RandRange(0, FMath::Max(0, MaxHops - 1));
		P.Dest = R.Stops[(I + Hops) % R.Stops.Num()];
		Fare = NHDir::Round50(R.FareLo + (R.FareHi - R.FareLo) * static_cast<float>(Hops - 1) / FMath::Max(1, MaxHops - 1));
	}
	P.Fare = FMath::Clamp(Fare, 200, 500);
	const float Roll = FMath::FRand();
	P.Note = R.bLinear ? P.Fare : Roll < 0.35f ? P.Fare : Roll < 0.7f ? (P.Fare < 500 ? 500 : 1000) : 1000; // first-day passengers pay exact
	P.Name = Data()->PaxNames.Num() ? NHDir::Pick(Data()->PaxNames) : TEXT("Passenger");
	return P;
}

void ANHGameDirector::SpawnWaiting(FName StopId, FPax& Pax, int32 Slot)
{
	ANHPerson* Body = GetWorld()->SpawnActor<ANHPerson>(ANHPerson::StaticClass(), QueueSpot(StopId, Slot), FRotator::ZeroRotator);
	if (Body)
	{
		Body->Init(Pax.Id * 31, NHDir::Shirts[Pax.Id % 8], FMath::FRand() < 0.5f);
		Body->FaceTowards(StopKerb(StopId));
		Body->SetWaving(Slot == 0);
		Pax.Body = Body;
	}
}

void ANHGameDirector::ClearPassengers()
{
	for (auto& KV : Shift.Waiting)
	{
		for (FPax& P : KV.Value)
		{
			if (P.Body.IsValid())
			{
				P.Body->Destroy();
			}
		}
	}
	Shift.Waiting.Reset();
	Shift.ChangeQ.Reset();
	if (Panel.bOpen && Panel.Title.StartsWith(TEXT("Change")))
	{
		Panel = FPanel();
	}
}

void ANHGameDirector::Arrive(FName StopId)
{
	const FNHConductorRules& C = Data()->Conductor;
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	ANHVehicle* Bus = Shift.Bus.Get();
	Shift.AtStop = StopId;
	Shift.bNear = false;
	Shift.Idx = Shift.Route.Stops.IndexOfByKey(StopId);
	const FVector Wait = StopWait(StopId);

	// passengers for this stop get down; happy passengers tip
	for (int32 I = Shift.Onboard.Num() - 1; I >= 0; --I)
	{
		if (Shift.Onboard[I].Dest != StopId)
		{
			continue;
		}
		Shift.Onboard.RemoveAt(I);
		Shift.Carried++;
		Hustle->Jobs++;
		if (Bus)
		{
			if (ANHPerson* Body = GetWorld()->SpawnActor<ANHPerson>(ANHPerson::StaticClass(), Bus->ExitPoint() - FVector(0, 0, 100.f), FRotator::ZeroRotator))
			{
				Body->Init(PaxId + I * 7, NHDir::Shirts[(PaxId + I) % 8], FMath::FRand() < 0.5f);
				Body->WalkTo(Wait + FVector(FMath::FRandRange(-400.f, 400.f), FMath::FRandRange(-100.f, 100.f), 0.f));
				Body->LifeLeft = 4.f;
			}
		}
		if (FMath::FRand() < Shift.Comfort / 200.f)
		{
			const int32 Tip = NHDir::Round50(50.f + Shift.Comfort * 1.5f);
			Shift.Tips += Tip;
			ANHHUD::Floater(this, Wait + FVector(0, 0, 200.f), FString::Printf(TEXT("Tip %s"), *NHDir::N(Tip)));
		}
	}
	// a rough ride: one passenger demands their money back and gets down here
	if (Shift.Comfort < 30.f && Shift.Onboard.Num())
	{
		const int32 I = FMath::RandRange(0, Shift.Onboard.Num() - 1);
		const FPax P = Shift.Onboard[I];
		Shift.Onboard.RemoveAt(I);
		Shift.Penalties += P.Fare;
		ANHHUD::Floater(this, Wait + FVector(0, 0, 220.f), TEXT("\"Driver, you wan kill us? My money!\""));
		ANHHUD::Toast(this, FString::Printf(TEXT("%s got down and took back %s. Drive gently."), *P.Name, *NHDir::N(P.Fare)), 2);
	}
	const FNHBusStop* Stop = Data()->Stops.Find(StopId);
	if (Stop && Stop->Agbero > 0 && (!Shift.bMission || StopId == NHDir::BaloGate))
	{
		const int32* Mult = Shift.Demand.Find(StopId);
		OpenAgbero(StopId, Shift.bMission ? C.FirstTicket : Stop->Agbero * (Mult ? *Mult : 1), true);
	}
	else
	{
		const TArray<FPax>* W = Shift.Waiting.Find(StopId);
		ANHHUD::Toast(this, FString::Printf(TEXT("%s: %d waiting. Press E to call."), *StopName(StopId), W ? W->Num() : 0));
	}
}

void ANHGameDirector::CallPassengers()
{
	const FNHConductorRules& C = Data()->Conductor;
	const FName StopId = Shift.AtStop;
	TArray<FPax>* Wait = Shift.Waiting.Find(StopId);
	ANHVehicle* Bus = Shift.Bus.Get();
	if (!Wait || !Bus)
	{
		return;
	}
	if (Data()->Calls.Num())
	{
		ANHHUD::Floater(this, Bus->GetActorLocation() + FVector(0, 0, 300.f), NHDir::Pick(Data()->Calls).Replace(TEXT("\""), TEXT("")));
	}
	if (!Shift.bMission && FMath::FRand() < 0.4f) // shouting the route pulls in one more
	{
		FPax P = MakePax(StopId);
		SpawnWaiting(StopId, P, Wait->Num());
		Wait->Add(P);
	}
	const int32 Free = C.Capacity - Shift.Onboard.Num();
	if (Free <= 0)
	{
		ANHHUD::Toast(this, TEXT("Bus full! Move on."), 1);
		return;
	}
	const int32 N = FMath::Min3(Shift.bMission ? 99 : 3, Wait->Num(), Free);
	if (N <= 0)
	{
		ANHHUD::Toast(this, TEXT("Nobody else waiting here."));
		return;
	}
	const FVector Door = Bus->GetActorLocation() + Bus->GetActorRightVector() * (Bus->GetSpec().Width * 0.5f + 40.f);
	for (int32 K = 0; K < N; ++K)
	{
		FPax P = (*Wait)[0];
		Wait->RemoveAt(0);
		Shift.Fares += P.Fare;
		ANHHUD::Floater(this, StopWait(StopId) + FVector(0, 0, 220.f + K * 40.f), TEXT("+") + NHDir::N(P.Fare));
		if (P.Body.IsValid())
		{
			P.Body->SetWaving(false);
			P.Body->WalkTo(Door, 220.f);
			P.Body->LifeLeft = FVector::Dist2D(P.Body->GetActorLocation(), Door) / 220.f + 0.3f;
			P.Body = nullptr;
		}
		if (P.Note > P.Fare)
		{
			FChange Ch;
			Ch.Pax = P;
			Ch.Right = P.Note - P.Fare;
			Ch.Opts.Add(Ch.Right);
			for (const int32 Dx : { 100, -100, 200, -200, 50 })
			{
				if (Ch.Opts.Num() >= 3)
				{
					break;
				}
				if (Ch.Right + Dx > 0)
				{
					Ch.Opts.AddUnique(Ch.Right + Dx);
				}
			}
			for (int32 I = Ch.Opts.Num() - 1; I > 0; --I) // shuffle
			{
				Ch.Opts.Swap(I, FMath::RandRange(0, I));
			}
			Shift.ChangeQ.Add(Ch);
		}
		Shift.Onboard.Add(P);
	}
	for (int32 I = 0; I < Wait->Num(); ++I) // the rest shuffle up the queue
	{
		if ((*Wait)[I].Body.IsValid())
		{
			(*Wait)[I].Body->WalkTo(QueueSpot(StopId, I), 100.f);
			(*Wait)[I].Body->SetWaving(I == 0);
		}
	}
}

void ANHGameDirector::OpenChangePanel()
{
	const FChange& Q = Shift.ChangeQ[0];
	TArray<FString> Opts;
	for (const int32 O : Q.Opts)
	{
		Opts.Add(FString::Printf(TEXT("Give %s"), *NHDir::N(O)));
	}
	Opts.Add(TEXT("No change (keep it)"));
	const TArray<int32> Values = Q.Opts;
	OpenPanel(FString::Printf(TEXT("Change for %s"), *Q.Pax.Name),
		{ FString::Printf(TEXT("Paid %s for a %s ride."), *NHDir::N(Q.Pax.Note), *NHDir::N(Q.Pax.Fare)), TEXT("How much change do you give?") },
		Opts, [this, Values](int32 Choice) { AnswerChange(Values.IsValidIndex(Choice) ? Values[Choice] : -1); });
}

void ANHGameDirector::AnswerChange(int32 Choice)
{
	if (Shift.ChangeQ.IsEmpty())
	{
		return;
	}
	const FChange Q = Shift.ChangeQ[0];
	Shift.ChangeQ.RemoveAt(0);
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const FVector At = Shift.Bus.IsValid() ? Shift.Bus->GetActorLocation() + FVector(0, 0, 320.f) : FVector::ZeroVector;
	if (Choice == Q.Right)
	{
		if (FMath::FRand() < 0.5f)
		{
			Shift.Tips += 50;
			ANHHUD::Floater(this, At, TEXT("\"Thank you o!\" +N50"));
		}
	}
	else if (Choice == -1)
	{
		Shift.Keep += Q.Right;
		Hustle->Integrity = FMath::Clamp(Hustle->Integrity - 3, -100, 100);
		Shift.Comfort = FMath::Max(0.f, Shift.Comfort - 15.f);
		ANHHUD::Floater(this, At, FString::Printf(TEXT("\"%s: Conductor, where my change?!\""), *Q.Pax.Name));
	}
	else if (Choice > Q.Right)
	{
		Shift.Penalties += Choice - Q.Right;
		ANHHUD::Floater(this, At, FString::Printf(TEXT("Gave %s too much"), *NHDir::N(Choice - Q.Right)));
	}
	else
	{
		Shift.Keep += Q.Right - Choice;
		Hustle->Integrity = FMath::Clamp(Hustle->Integrity - 2, -100, 100);
		Shift.Comfort = FMath::Max(0.f, Shift.Comfort - 10.f);
		ANHHUD::Floater(this, At, FString::Printf(TEXT("\"%s: My change no complete!\""), *Q.Pax.Name));
	}
}

void ANHGameDirector::Depart()
{
	const FNHConductorRules& C = Data()->Conductor;
	if (!Shift.ChangeQ.IsEmpty()) // drove off mid-change: you paid out correctly, but nobody tips a rushed conductor
	{
		Shift.ChangeQ.Reset();
		if (Panel.bOpen && Panel.Title.StartsWith(TEXT("Change")))
		{
			Panel = FPanel();
		}
	}
	if (Shift.Onboard.Num() >= C.Capacity)
	{
		Shift.Bonus += C.FullBusBonus;
		ANHHUD::Toast(this, FString::Printf(TEXT("Full bus! +%s bonus."), *NHDir::N(C.FullBusBonus)), 1);
	}
	const int32 I = Shift.Route.Stops.IndexOfByKey(Shift.AtStop);
	if (Shift.Route.bLinear && I == Shift.Route.Stops.Num() - 1)
	{
		Shift.bRouteDone = true;
	}
	Shift.Idx = (I + 1) % Shift.Route.Stops.Num();
	Shift.AtStop = NAME_None;
	Shift.bNear = false;
}

void ANHGameDirector::MissStop(FName StopId, const FString& Why)
{
	const FNHConductorRules& C = Data()->Conductor;
	Shift.Missed++;
	Shift.Penalties += C.MissedStopFine; // the owner's late / missed-stop fine
	int32 Off = 0;
	for (int32 I = Shift.Onboard.Num() - 1; I >= 0; --I)
	{
		if (Shift.Onboard[I].Dest == StopId)
		{
			Shift.Penalties += Shift.Onboard[I].Fare;
			Shift.Comfort = FMath::Max(0.f, Shift.Comfort - 10.f);
			Shift.Onboard.RemoveAt(I);
			++Off;
		}
	}
	ANHHUD::Toast(this, FString::Printf(TEXT("You %s %s! %s-%s missed-stop fine."), *Why, *StopName(StopId),
		Off ? *FString::Printf(TEXT("%d passenger%s took back their fare. "), Off, Off > 1 ? TEXT("s") : TEXT("")) : TEXT(""), *NHDir::N(C.MissedStopFine)), 2);
	const int32 I = Shift.Route.Stops.IndexOfByKey(StopId);
	if (Shift.Route.bLinear && I == Shift.Route.Stops.Num() - 1)
	{
		Shift.bRouteDone = true;
	}
	Shift.Idx = (I + 1) % Shift.Route.Stops.Num();
	Shift.bNear = false;
}

void ANHGameDirector::OpenAgbero(FName StopId, int32 Amount, bool bCanBeg)
{
	const FString Name = StopName(StopId);
	TArray<FString> Opts = { FString::Printf(TEXT("Pay %s"), *NHDir::N(Amount)) };
	if (bCanBeg)
	{
		Opts.Add(TEXT("Beg: \"Abeg, na my first day.\" (cred and a good outfit help)"));
	}
	Opts.Add(TEXT("Drive off (no ticket, 1 wanted star)"));
	const int32* Mult = Shift.Demand.Find(StopId);
	OpenPanel(FString::Printf(TEXT("Agbero at %s"), *Name),
		{ bCanBeg ? FString::Printf(TEXT("\"Driver! Ticket. %s. Na for the boys.\""), *NHDir::N(Amount)) : FString::Printf(TEXT("\"You dey form big man? Now na %s!\""), *NHDir::N(Amount)),
		  (Mult && *Mult > 1) ? TEXT("They remember you drove off last time.") : TEXT("Park touts want their \"ticket\" before you load.") },
		Opts, [this, StopId, Amount, bCanBeg](int32 Choice)
		{
			UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
			const int32 Action = (!bCanBeg && Choice == 1) ? 2 : Choice; // without "Beg", the second option is "Drive off"
			if (Action == 0)
			{
				Shift.AgberoPaid += Amount;
				Shift.Demand.Add(StopId, 1);
				ANHHUD::Toast(this, FString::Printf(TEXT("Paid %s ticket. \"Load well, driver!\""), *NHDir::N(Amount)));
			}
			else if (Action == 1)
			{
				const int32* Respect = Data()->OutfitRespect.Find(Hustle->Outfit);
				if (FMath::FRand() < 0.35f + Hustle->Cred / 400.f + (Respect ? *Respect : 0) / 100.f)
				{
					const int32 Half = NHDir::Round50(Amount / 2.f);
					Shift.AgberoPaid += Half;
					ANHHUD::Toast(this, FString::Printf(TEXT("\"Ok, %s. Next time no story.\" Paid."), *NHDir::N(Half)), 1);
				}
				else
				{
					OpenAgbero(StopId, NHDir::Round50(Amount * 1.5f), false);
				}
			}
			else
			{
				const int32* M = Shift.Demand.Find(StopId);
				Shift.Demand.Add(StopId, (M ? *M : 1) * 2);
				Shift.Comfort = FMath::Max(0.f, Shift.Comfort - 10.f);
				ANHHUD::Floater(this, StopWait(StopId) + FVector(0, 0, 220.f), TEXT("\"Driver! Come back here!\""));
				Hustle->AddHeat(1.f);
				// no loading at a stop you just ran from
				const int32 I = Shift.Route.Stops.IndexOfByKey(StopId);
				Shift.AtStop = NAME_None;
				if (Shift.Route.bLinear && I == Shift.Route.Stops.Num() - 1)
				{
					Shift.bRouteDone = true;
				}
				Shift.Idx = (I + 1) % Shift.Route.Stops.Num();
			}
		});
}

void ANHGameDirector::OpenRoutePicker(ANHVehicle* Bus)
{
	const UNHGameData* D = Data();
	TArray<FString> Opts;
	for (const FNHRoute& R : D->Routes)
	{
		Opts.Add(FString::Printf(TEXT("%s   (%d stops, %s-%s)"), *R.Name, R.Stops.Num(), *NHDir::N(R.FareLo), *NHDir::N(R.FareHi)));
	}
	Opts.Add(TEXT("Not now"));
	TWeakObjectPtr<ANHVehicle> WeakBus = Bus;
	OpenPanel(TEXT("Pick a route"),
		{ TEXT("Stop at each bus stop, press E to call passengers, give the right change, drive gently."),
		  FString::Printf(TEXT("The owner takes %d%% of fares."), FMath::RoundToInt(D->Conductor.OwnerCut * 100.f)) },
		Opts, [this, WeakBus](int32 Choice)
		{
			const UNHGameData* Dd = Data();
			if (WeakBus.IsValid() && Dd->Routes.IsValidIndex(Choice))
			{
				StartShift(Dd->Routes[Choice], WeakBus.Get(), false, Dd->Conductor.OwnerCut, TEXT("Baba Sule"));
			}
		});
}

void ANHGameDirector::OpenShiftMenu()
{
	const FTotals T = Totals();
	OpenPanel(Shift.Route.Name,
		{ FString::Printf(TEXT("Seats %d/%d, bag %s, comfort %d%%."), Shift.Onboard.Num(), Data()->Conductor.Capacity, *NHDir::N(Shift.Fares + Shift.Tips + Shift.Bonus + Shift.Keep), FMath::RoundToInt(Shift.Comfort)),
		  FString::Printf(TEXT("If you stopped now you'd take home %s."), *NHDir::N(T.Net)) },
		{ TEXT("Keep driving"), TEXT("End shift and settle up") }, [this](int32 Choice) { if (Choice == 1) EndShift(TEXT("Shift summary"), nullptr); });
}

ANHGameDirector::FTotals ANHGameDirector::Totals() const
{
	const FNHConductorRules& C = Data()->Conductor;
	const ANHVehicle* Bus = Shift.Bus.Get();
	const int32 Damage = FMath::Max(0, FMath::RoundToInt((Shift.HpStart - (Bus ? FMath::Max(0.f, Bus->Health) : Shift.HpStart)) * C.DamageCost));
	const int32 Cut = Shift.Owner.IsEmpty() ? 0 : FMath::RoundToInt(Shift.Fares * Shift.Cut);
	const int32 Net = Shift.Fares + Shift.Tips + Shift.Bonus + Shift.Keep - Shift.Penalties - Shift.AgberoPaid - Damage - Cut;
	return { Shift.Fares, Shift.Tips, Shift.Bonus, Shift.Keep, Shift.Penalties, Shift.AgberoPaid, Damage, Cut, Net };
}

void ANHGameDirector::EndShift(const FString& Title, TFunction<void()> OnClose)
{
	if (!Shift.bOn)
	{
		if (OnClose)
		{
			OnClose();
		}
		return;
	}
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const FTotals T = Totals();
	const FString RouteName = Shift.Route.Name;
	const int32 CutPct = FMath::RoundToInt(Shift.Cut * 100.f);
	// anyone still on board gets down; their fares were already collected
	Shift.Carried += Shift.Onboard.Num();
	Hustle->Jobs += Shift.Onboard.Num();
	Shift.Onboard.Reset();
	const int32 Paid = T.Net >= 0 ? T.Net : -FMath::Min(Hustle->Cash, -T.Net);
	if (Paid == T.Net) // the ledger shows each part of the settle-up; the total is the same
	{
		Hustle->Earn(T.Fares + T.Tips + T.Bonus + T.Keep, TEXT("Fares & tips (conductor shift)"));
		Hustle->Earn(-T.Cut, FString::Printf(TEXT("%s's cut"), *Shift.Owner));
		Hustle->Earn(-T.Agbero, TEXT("Agbero tickets"));
		Hustle->Earn(-(T.Penalties + T.Damage), TEXT("Shift penalties & damage"));
	}
	else
	{
		Hustle->Earn(Paid, TEXT("Conductor shift"));
	}
	auto Neg = [](int32 N) { return N ? TEXT("-") + NHDir::N(N) : NHDir::N(0); };
	TArray<FString> Lines = {
		FString::Printf(TEXT("%s   %d missed stop%s"), *RouteName, Shift.Missed, Shift.Missed == 1 ? TEXT("") : TEXT("s")),
		FString::Printf(TEXT("Passengers carried: %d"), Shift.Carried),
		FString::Printf(TEXT("Fares collected: %s     Tips: %s     Full-bus bonus: %s"), *NHDir::N(T.Fares), *NHDir::N(T.Tips), *NHDir::N(T.Bonus)) };
	if (T.Keep)
	{
		Lines.Add(FString::Printf(TEXT("Change you \"forgot\" to give: %s"), *NHDir::N(T.Keep)));
	}
	Lines.Add(FString::Printf(TEXT("Refunds & missed-stop fines: %s     Agbero tickets: %s"), *Neg(T.Penalties), *Neg(T.Agbero)));
	Lines.Add(FString::Printf(TEXT("Damage to the bus: %s     %s's cut (%d%%): %s"), *Neg(T.Damage), *Shift.Owner, CutPct, *Neg(T.Cut)));
	Lines.Add(FString::Printf(TEXT("YOUR EARNINGS: %s"), *NHDir::N(T.Net)));
	ClearPassengers();
	Shift.bOn = false;
	RouteStopPoints.Reset();
	Hustle->Save();
	OpenPanel(Title, Lines, { TEXT("Collect") }, [OnClose](int32) { if (OnClose) OnClose(); });
}

void ANHGameDirector::UpdateShift(float DeltaSeconds)
{
	if (!Shift.bOn)
	{
		return;
	}
	const FNHConductorRules& C = Data()->Conductor;
	ANHVehicle* V = Shift.Bus.Get();
	if (!V)
	{
		Shift = FShift();
		return;
	}
	if (PlayerVehicle() != V)
	{
		if (!Shift.bMission) // stepping out ends a free shift; on the first day, the route waits
		{
			EndShift(TEXT("Shift summary"), nullptr);
		}
		return;
	}
	// comfort: hard acceleration, braking and fast cornering upset passengers; smooth driving calms them
	const float Dt = FMath::Max(DeltaSeconds, 1e-4f);
	const float Acc = FMath::Abs(V->Speed - Shift.LastSpeed) / Dt;
	const float Yaw = V->GetActorRotation().Yaw;
	const float Lat = FMath::Abs(V->Speed * FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(Shift.LastYaw, Yaw))) / Dt;
	Shift.LastSpeed = V->Speed;
	Shift.LastYaw = Yaw;
	const float Rough = FMath::Max(0.f, (Acc - C.RoughAccel) / C.RoughAccel) + FMath::Max(0.f, (Lat - C.RoughLateral) / C.RoughLateral);
	if (Shift.Onboard.Num())
	{
		Shift.Comfort = FMath::Clamp(Shift.Comfort + (Rough > 0.f ? -FMath::Min(3.f, Rough) * 60.f : 1.5f) * DeltaSeconds, 0.f, 100.f);
	}
	if (Shift.Onboard.Num() && Rough == 0.f && FMath::Abs(V->Speed) > 500.f && Shift.Comfort > 80.f)
	{
		Shift.SmoothT += DeltaSeconds; // a smooth stretch: somebody tips the driver
		if (Shift.SmoothT > C.SmoothTipAfter)
		{
			Shift.SmoothT = 0.f;
			Shift.Tips += 50;
			const TCHAR* Lines[] = { TEXT("\"Driver, you try!\" +N50"), TEXT("\"Smooth like butter\" +N50"), TEXT("\"God bless you, driver\" +N50") };
			ANHHUD::Floater(this, V->GetActorLocation() + FVector(0, 0, 300.f), Lines[FMath::RandRange(0, 2)]);
		}
	}
	else if (Rough > 0.f)
	{
		Shift.SmoothT = 0.f;
	}
	if (Shift.bRouteDone)
	{
		return;
	}
	// waiting passengers slowly build up again at the other stops
	Shift.Refill -= DeltaSeconds;
	if (Shift.Refill <= 0.f && !Shift.bMission)
	{
		Shift.Refill = C.Refill;
		for (const FName& Id : Shift.Route.Stops)
		{
			TArray<FPax>& Q = Shift.Waiting.FindOrAdd(Id);
			if (Id != Shift.AtStop && Q.Num() < Shift.Route.Busy)
			{
				FPax P = MakePax(Id);
				SpawnWaiting(Id, P, Q.Num());
				Q.Add(P);
			}
		}
	}
	if (!Shift.AtStop.IsNone())
	{
		if (FMath::Abs(V->Speed) > C.DepartSpeed && FVector::Dist2D(V->GetActorLocation(), StopKerb(Shift.AtStop)) > C.DepartRadius)
		{
			Depart();
		}
		return;
	}
	const FName Sid = Shift.Route.Stops[Shift.Idx];
	const float D = FVector::Dist2D(V->GetActorLocation(), StopKerb(Sid));
	const bool bSlow = FMath::Abs(V->Speed) < C.SlowSpeed;
	if (D < C.ArriveRadius && bSlow)
	{
		Arrive(Sid);
		return;
	}
	// stopping at one of the next two stops skips the one(s) in between
	const int32 N = Shift.Route.Stops.Num();
	for (int32 K = 1; K <= FMath::Min(2, N - 1); ++K)
	{
		const int32 J = (Shift.Idx + K) % N;
		if (Shift.Route.bLinear && J <= Shift.Idx)
		{
			break; // a one-way route never loops back
		}
		if (bSlow && FVector::Dist2D(V->GetActorLocation(), StopKerb(Shift.Route.Stops[J])) < C.ArriveRadius)
		{
			for (int32 M = 0; M < K; ++M)
			{
				MissStop(Shift.Route.Stops[Shift.Idx % N], TEXT("skipped"));
			}
			Arrive(Shift.Route.Stops[J]);
			return;
		}
	}
	if (D < C.NearRadius)
	{
		Shift.bNear = true;
	}
	else if (Shift.bNear && D > C.PassRadius)
	{
		MissStop(Sid, TEXT("drove past"));
	}
}
