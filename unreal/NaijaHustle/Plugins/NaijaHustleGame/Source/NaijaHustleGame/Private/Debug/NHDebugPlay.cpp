#include "Debug/NHDebugPlay.h"

#include "Components/BoxComponent.h"
#include "Core/NHGameData.h"
#include "Core/NHHustleSubsystem.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/NHGameDirector.h"
#include "Gameplay/NHPerson.h"
#include "Kismet/GameplayStatics.h"
#include "Lighting/NHLightingRig.h"
#include "NaijaHustleGame.h"
#include "Player/NHCharacter.h"
#include "Player/NHPlayerController.h"
#include "Vehicles/NHVehicle.h"

FString UNHDebugPlay::PendingRun;
bool UNHDebugPlay::bPendingQuit = false;

namespace NHPlay
{
	const FName FirstDay(TEXT("lag_01"));
	const TCHAR* SaveSlot = TEXT("NaijaHustle");
	FString N(int32 Amount) { return UNHHustleSubsystem::Naira(Amount); }
}

// ---------------------------------------------------------------------------------------------------- the game, as the script sees it
ANHGameDirector* UNHDebugPlay::Dir() const { return ANHGameDirector::Get(PC); }
UNHHustleSubsystem* UNHDebugPlay::Hustle() const { return UNHHustleSubsystem::Get(PC); }
ANHVehicle* UNHDebugPlay::Bus() const { return Dir() ? Dir()->MissionBus.Get() : nullptr; }
ANHCharacter* UNHDebugPlay::Character() const { return PC ? PC->OnFootCharacter.Get() : nullptr; }

FString UNHDebugPlay::StageName() const
{
	static const TCHAR* Names[] = { TEXT("Meet"), TEXT("Talk"), TEXT("Board"), TEXT("Route"), TEXT("Return"), TEXT("Wrapping"), TEXT("Failed"), TEXT("Done") };
	return Dir() ? Names[static_cast<int32>(Dir()->Stage)] : TEXT("none");
}

void UNHDebugPlay::Note(const FString& Text) const
{
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: [%s] %s"), RunName.IsEmpty() ? TEXT("debug") : *RunName, *Text);
}

bool UNHDebugPlay::Check(const FString& What, bool bOk, const FString& Detail)
{
	(bOk ? Passed : Failed)++;
	Note(FString::Printf(TEXT("%s  %s%s%s"), bOk ? TEXT("PASS") : TEXT("FAIL"), *What, Detail.IsEmpty() ? TEXT("") : TEXT("  : "), *Detail));
	return bOk;
}

int32 UNHDebugPlay::WaitingAt(FName StopId) const
{
	const auto* W = Dir() ? Dir()->Shift.Waiting.Find(StopId) : nullptr;
	return W ? W->Num() : 0;
}

FVector2D UNHDebugPlay::StopForward(FName StopId) const
{
	const UNHGameData* D = UNHGameData::Get(PC);
	const FNHBusStop* S = D ? D->Stops.Find(StopId) : nullptr;
	if (!S)
	{
		return FVector2D(1.f, 0.f);
	}
	const FVector2D Away = (S->Wait - S->Kerb).GetSafeNormal(); // towards the pavement: the bus's right-hand side
	return FVector2D(Away.Y, -Away.X);
}

void UNHDebugPlay::PlaceVehicle(ANHVehicle* V, const FVector2D& At, const FVector2D& Forward) const
{
	V->SetDriveInput(0.f, 0.f, 0.f);
	V->Speed = 0.f;
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Forward.Y, Forward.X));
	V->SetActorLocationAndRotation(Dir()->Ground(At, 150.f), FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	if (PC->GetPawn() == V)
	{
		PC->SetControlRotation(V->GetActorRotation());
	}
}

FVector2D UNHDebugPlay::ApproachStart(ANHVehicle* V, const FVector2D& End, const FVector2D& Preferred, float Distance) const
{
	// the preferred direction first, then round the compass; the longest clear run wins if none is clear all the way
	const UNHGameData* D = UNHGameData::Get(PC);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(NHDebugApproach), false, V);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_Vehicle); // parked cars
	const FCollisionShape Shape = FCollisionShape::MakeBox(FVector(V->GetSpec().Length * 0.5f, V->GetSpec().Width * 0.5f + 20.f, 40.f));
	FVector2D Best = End - Preferred * 600.f;
	float BestClear = 0.f;
	for (int32 K = 0; K < 8; ++K)
	{
		const FVector2D Fwd = Preferred.GetRotated(K * 45.f);
		float Clear = 0.f;
		for (float Step = 200.f; Step <= Distance; Step += 200.f)
		{
			const FVector2D P = End - Fwd * Step;
			const TCHAR Tile = D->TileAt(FVector(P.X, P.Y, 0.f));
			const FVector At(P.X, P.Y, 150.f);
			const bool bDrivable = Tile == TEXT('R') || Tile == TEXT('P') || Tile == TEXT('F');
			if (!bDrivable || PC->GetWorld()->OverlapAnyTestByObjectType(At, FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Fwd.Y, Fwd.X)), 0.f).Quaternion(), Objects, Shape, Q))
			{
				break;
			}
			Clear = Step;
		}
		if (Clear > BestClear)
		{
			BestClear = Clear;
			Best = End - Fwd * Clear;
			if (Clear >= Distance)
			{
				break;
			}
		}
	}
	return Best;
}

bool UNHDebugPlay::Drive(ANHVehicle* V, const FVector2D& Target, float Cruise, bool bStopThere) const
{
	const FVector2D Here(V->GetActorLocation());
	const FVector2D To = Target - Here;
	const float Dist = static_cast<float>(To.Size());
	const float Want = FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X));
	const float Off = FMath::FindDeltaAngleDegrees(V->GetActorRotation().Yaw, Want);
	const float Steer = Dist > 200.f ? FMath::Clamp(Off / 25.f, -1.f, 1.f) : 0.f;
	// slow for the target so it can stop there: v² = 2 a d, with the gentle half of the brakes
	const float Limit = bStopThere ? FMath::Min(Cruise, FMath::Sqrt(FMath::Max(0.f, Dist - 80.f) * V->GetSpec().Accel)) : Cruise;
	const bool bArrived = bStopThere && Dist < 150.f;
	float Throttle = 0.f, Brake = 0.f;
	if (!bArrived && V->Speed < Limit - 40.f)
	{
		Throttle = 0.6f;
	}
	else if (V->Speed > Limit + 60.f && V->Speed > 60.f)
	{
		Brake = 0.5f; // above 50 cm/s the brake pedal brakes; below it would start to reverse
	}
	V->SetDriveInput(Throttle, Brake, Steer);
	return bArrived && FMath::Abs(V->Speed) < 30.f;
}

bool UNHDebugPlay::WalkTowards(const FVector& Target, float Reach) const
{
	ANHCharacter* C = Cast<ANHCharacter>(PC->GetPawn());
	if (!C)
	{
		return false;
	}
	const FVector To = (Target - C->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	if (To.Size() < Reach)
	{
		C->SetSprinting(false);
		return true;
	}
	C->SetSprinting(true);
	C->AddMovementInput(To.GetSafeNormal(), 1.f);
	return false;
}

bool UNHDebugPlay::AnswerChangeRight()
{
	ANHGameDirector* D = Dir();
	if (!D || D->Shift.ChangeQ.IsEmpty() || !D->Panel.bOpen || !D->Panel.Title.StartsWith(TEXT("Change")))
	{
		return false;
	}
	const auto& Q = D->Shift.ChangeQ[0];
	const int32 Option = Q.Opts.IndexOfByKey(Q.Right);
	Note(FString::Printf(TEXT("change for %s: paid %s for a %s ride, giving %s (option %d)"), *Q.Pax.Name, *NHPlay::N(Q.Pax.Note), *NHPlay::N(Q.Pax.Fare), *NHPlay::N(Q.Right), Option + 1));
	D->OnChoice(Option);
	return true;
}

// ---------------------------------------------------------------------------------------------------- single commands
void UNHDebugPlay::Goto(const FString& Where)
{
	const UNHGameData* D = UNHGameData::Get(PC);
	APawn* Pawn = PC->GetPawn();
	if (!D || !Dir() || !Pawn)
	{
		return;
	}
	FVector2D At, Fwd(1.f, 0.f);
	if (Where.Equals(TEXT("park"), ESearchCase::IgnoreCase))
	{
		At = D->Park + FVector2D(0.f, 250.f);
	}
	else if (Where.Equals(TEXT("bay1"), ESearchCase::IgnoreCase) && D->ParkBays.Num())
	{
		Fwd = FVector2D(1.f, 0.f).GetRotated(D->ParkBays[0].Yaw);
		At = D->ParkBays[0].Pos + (Cast<ANHVehicle>(Pawn) ? FVector2D::ZeroVector : Fwd.GetRotated(-90.f) * 260.f); // on foot: beside the driver's door
	}
	else if (const FNHBusStop* S = D->Stops.Find(FName(*Where.ToLower())))
	{
		Fwd = StopForward(S->Id);
		At = Cast<ANHVehicle>(Pawn) ? S->Kerb : S->Wait;
	}
	else
	{
		TArray<FString> Ids;
		for (const auto& KV : D->Stops)
		{
			Ids.Add(KV.Key.ToString());
		}
		UE_LOG(LogNHGame, Warning, TEXT("NHGoto: no place called '%s' (park, bay1, %s)"), *Where, *FString::Join(Ids, TEXT(", ")));
		return;
	}
	if (ANHVehicle* V = Cast<ANHVehicle>(Pawn))
	{
		PlaceVehicle(V, At, Fwd);
	}
	else
	{
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Fwd.Y, Fwd.X));
		Pawn->SetActorLocationAndRotation(Dir()->Ground(At, 100.f), FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(FRotator(-10.f, Yaw, 0.f));
	}
	Note(FString::Printf(TEXT("goto %s: %s at %.0f, %.0f"), *Where, Cast<ANHVehicle>(Pawn) ? TEXT("vehicle") : TEXT("on foot"), At.X, At.Y));
}

void UNHDebugPlay::Board()
{
	ANHGameDirector* D = Dir();
	ANHVehicle* V = Cast<ANHVehicle>(PC->GetPawn());
	if (!D || !V || !D->Shift.bOn || D->Shift.Bus.Get() != V || D->Shift.AtStop.IsNone())
	{
		UE_LOG(LogNHGame, Warning, TEXT("NHBoard: stop the bus at a stop on your route first"));
		return;
	}
	if (D->Panel.bOpen && !D->Panel.Title.StartsWith(TEXT("Change")))
	{
		UE_LOG(LogNHGame, Warning, TEXT("NHBoard: answer '%s' first (NHAgbero pay)"), *D->Panel.Title);
		return;
	}
	const FName Stop = D->Shift.AtStop;
	const int32 FaresBefore = D->Shift.Fares, SeatsBefore = D->Shift.Onboard.Num();
	const int32 Capacity = UNHGameData::Get(PC)->Conductor.Capacity;
	for (int32 Guard = 0; Guard < 40; ++Guard)
	{
		while (!D->Shift.ChangeQ.IsEmpty()) // every change prompt, through its own panel
		{
			if (!D->Panel.bOpen)
			{
				D->OpenChangePanel();
			}
			if (!AnswerChangeRight())
			{
				break;
			}
		}
		if (WaitingAt(Stop) == 0 || D->Shift.Onboard.Num() >= Capacity)
		{
			break;
		}
		D->OnAction(V); // E: call passengers
	}
	Note(FString::Printf(TEXT("board at %s: %d got on, fares +%s, %d still waiting, seats %d/%d"), *D->StopName(Stop), D->Shift.Onboard.Num() - SeatsBefore,
		*NHPlay::N(D->Shift.Fares - FaresBefore), WaitingAt(Stop), D->Shift.Onboard.Num(), Capacity));
}

void UNHDebugPlay::Agbero(const FString& What)
{
	ANHGameDirector* D = Dir();
	if (!D || !D->Panel.bOpen || !D->Panel.Title.StartsWith(TEXT("Agbero")))
	{
		UE_LOG(LogNHGame, Warning, TEXT("NHAgbero: no agbero is asking for anything right now"));
		return;
	}
	const int32 Last = D->Panel.Options.Num() - 1; // pay, [beg], drive off
	const int32 Option = What.Equals(TEXT("beg"), ESearchCase::IgnoreCase) ? FMath::Min(1, Last) : What.Equals(TEXT("drive"), ESearchCase::IgnoreCase) ? Last : 0;
	Note(FString::Printf(TEXT("agbero: %s -> \"%s\""), *D->Panel.Title, *D->Panel.Options[Option]));
	D->OnChoice(Option);
}

void UNHDebugPlay::Finish()
{
	ANHGameDirector* D = Dir();
	if (!D)
	{
		return;
	}
	using EStage = ANHGameDirector::EStage;
	const FString Before = StageName();
	if (D->Dialogue.bOpen) // whatever is being said, hear it out
	{
		TFunction<void()> Done = MoveTemp(D->Dialogue.OnDone);
		D->Dialogue = ANHGameDirector::FDialogue();
		if (Done)
		{
			Done();
		}
	}
	else if (D->Panel.bOpen)
	{
		D->OnChoice(0);
	}
	else
	{
		switch (D->Stage)
		{
		case EStage::Meet:
			Goto(TEXT("park"));
			break;
		case EStage::Board:
			if (Bus() && PC->GetPawn() != Bus())
			{
				PC->LeaveVehicle(true);
				PC->EnterVehicle(Bus());
			}
			break;
		case EStage::Route:
			D->Shift.AtStop = NAME_None;
			D->Shift.bRouteDone = true;
			break;
		case EStage::Return:
			if (ANHVehicle* V = Bus())
			{
				PC->LeaveVehicle(true);
				PC->EnterVehicle(V);
				PlaceVehicle(V, UNHGameData::Get(PC)->Park + FVector2D(0.f, 400.f), FVector2D(1.f, 0.f));
			}
			break;
		default:
			break;
		}
	}
	Note(FString::Printf(TEXT("finish: stage %s -> %s"), *Before, *StageName()));
}

// ---------------------------------------------------------------------------------------------------- the script engine
void UNHDebugPlay::Begin(const FString& Name, bool bQuitWhenDone)
{
	Steps.Reset();
	StepIndex = 0;
	RunName = Name;
	bQuit = bQuitWhenDone;
	Passed = Failed = 0;
	Pace = 0.f;
}

bool UNHDebugPlay::NeedsFreshStory(const FString& Name, bool bQuitWhenDone)
{
	UNHHustleSubsystem* H = Hustle();
	ANHGameDirector* D = Dir();
	if (!H || !D)
	{
		UE_LOG(LogNHGame, Error, TEXT("NAIJA HUSTLE: [%s] no game director or hustle subsystem in this level"), *Name);
		return true;
	}
	if (!H->IsDone(NHPlay::FirstDay) && D->Stage == ANHGameDirector::EStage::Meet && !D->Shift.bOn)
	{
		return false;
	}
	// the story has moved on: wipe the save and load the level again, then carry on from the new player controller
	RunName = Name;
	Note(TEXT("the first day is already under way or done: resetting progress and restarting the level"));
	H->ResetProgress();
	PendingRun = Name;
	bPendingQuit = bQuitWhenDone;
	PC->ConsoleCommand(TEXT("RestartLevel"));
	return true;
}

void UNHDebugPlay::Do(const FString& Name, TFunction<void()> Fn)
{
	Until(Name, [Fn](float) { Fn(); return true; }, 5.f);
}

void UNHDebugPlay::Until(const FString& Name, TFunction<bool(float)> Fn, float Timeout)
{
	FStep S;
	S.Name = Name;
	S.Run = MoveTemp(Fn);
	S.Timeout = Timeout;
	Steps.Add(MoveTemp(S));
}

void UNHDebugPlay::End(const FString& Why)
{
	if (ANHVehicle* V = Cast<ANHVehicle>(PC->GetPawn()))
	{
		V->SetDriveInput(0.f, 0.f, 0.f);
	}
	if (ANHCharacter* C = Cast<ANHCharacter>(PC->GetPawn()))
	{
		C->SetSprinting(false);
	}
	const bool bClean = Why.IsEmpty() && Failed == 0;
	Note(FString::Printf(TEXT("RESULT: %s. %d checks passed, %d failed%s%s"), bClean ? TEXT("COMPLETED CLEANLY") : TEXT("FAILED"), Passed, Failed, Why.IsEmpty() ? TEXT("") : TEXT(". "), *Why));
	Steps.Reset();
	StepIndex = 0;
	if (bQuit)
	{
		PC->ConsoleCommand(TEXT("quit"));
	}
}

void UNHDebugPlay::Tick(float DeltaSeconds)
{
	if (!IsRunning() || !PC)
	{
		return;
	}
	DeltaSeconds = FMath::Min(DeltaSeconds, 0.1f); // a hitch (the first frame after loading) must not run a step's clock out
	FStep& S = Steps[StepIndex];
	if (S.T == 0.f)
	{
		Note(FString::Printf(TEXT("step %d/%d: %s  (stage %s, %s, %s)"), StepIndex + 1, Steps.Num(), *S.Name, *StageName(), *Hustle()->ClockText(), *NHPlay::N(Hustle()->Cash)));
	}
	S.T += DeltaSeconds;
	if (S.Run(DeltaSeconds))
	{
		Pace = 0.f;
		if (++StepIndex >= Steps.Num())
		{
			End(FString());
		}
		return;
	}
	if (S.T > S.Timeout)
	{
		ANHGameDirector* D = Dir();
		End(FString::Printf(TEXT("Step '%s' did not finish in %.0f s (stage %s, panel '%s', dialogue %s)"), *S.Name, S.Timeout, *StageName(),
			D && D->Panel.bOpen ? *D->Panel.Title : TEXT("none"), D && D->Dialogue.bOpen ? TEXT("open") : TEXT("closed")));
	}
}

// ---------------------------------------------------------------------------------------------------- building blocks
void UNHDebugPlay::AddSkipDialogue(const FString& Name)
{
	Until(Name, [this](float Dt)
	{
		ANHGameDirector* D = Dir();
		if (!D->Dialogue.bOpen)
		{
			return true;
		}
		Pace -= Dt;
		if (Pace <= 0.f)
		{
			Pace = 0.3f;
			Note(FString::Printf(TEXT("%s: \"%s\""), *D->Dialogue.Speaker, D->Dialogue.Lines.IsValidIndex(D->Dialogue.Index) ? *D->Dialogue.Lines[D->Dialogue.Index] : TEXT("")));
			PC->OnAction(); // E: next line
		}
		return false;
	}, 20.f);
}

void UNHDebugPlay::AddMeetBaba()
{
	using EStage = ANHGameDirector::EStage;
	Until(TEXT("walk to Baba Driver at the motor park"), [this](float)
	{
		ANHGameDirector* D = Dir();
		if (D->Stage != EStage::Meet)
		{
			return true; // he starts talking when you get close
		}
		if (D->Baba.IsValid() && Steps[StepIndex].T < 14.f)
		{
			WalkTowards(D->Baba->GetActorLocation(), 200.f);
		}
		else if (Pace <= 0.f) // something in the way: step over it, once
		{
			Pace = 100.f;
			Check(TEXT("the player can walk from home to Baba Driver in 14 s"), false, TEXT("placing the player at the park instead"));
			Goto(TEXT("park"));
		}
		return false;
	}, 20.f);
	Do(TEXT("Baba Driver is talking"), [this]()
	{
		ANHGameDirector* D = Dir();
		if (ANHCharacter* C = Character())
		{
			C->SetSprinting(false);
		}
		Check(TEXT("meeting Baba Driver starts his introduction"), D->Stage == EStage::Talk && D->Dialogue.bOpen && D->Dialogue.Lines == UNHGameData::Get(PC)->BabaLines(TEXT("intro")),
			FString::Printf(TEXT("%d lines"), D->Dialogue.Lines.Num()));
	});
	AddSkipDialogue(TEXT("hear Baba Driver out (E)"));
	Do(TEXT("the job is on"), [this]() { Check(TEXT("after the talk the objective is to get in the danfo"), Dir()->Stage == EStage::Board, Dir()->ObjText); });
}

void UNHDebugPlay::AddEnterBus()
{
	using EStage = ANHGameDirector::EStage;
	Until(TEXT("walk to the danfo in bay 1"), [this](float)
	{
		ANHVehicle* V = Bus();
		if (!V)
		{
			return false;
		}
		if (PC->NearbyVehicle() == V)
		{
			return true;
		}
		if (Steps[StepIndex].T < 12.f)
		{
			WalkTowards(V->ExitPoint(), 60.f);
			PC->SetControlRotation(FRotator(-10.f, (V->GetActorLocation() - PC->GetPawn()->GetActorLocation()).Rotation().Yaw, 0.f)); // look at it, as a player would
		}
		else if (Pace <= 0.f)
		{
			Pace = 100.f;
			Check(TEXT("the player can walk from Baba Driver to the danfo in 12 s"), false, TEXT("placing the player beside the danfo instead"));
			Character()->SetActorLocation(V->ExitPoint(), false, nullptr, ETeleportType::TeleportPhysics);
		}
		return false;
	}, 16.f);
	Do(TEXT("get in (F)"), [this]()
	{
		if (ANHCharacter* C = Character())
		{
			C->SetSprinting(false);
		}
		PC->OnInteract();
		Check(TEXT("F puts you in the driver's seat of the mission danfo"), PC->GetPawn() == Bus() && PC->GetNHInputMode() == ENHInputMode::Vehicle);
	});
	Until(TEXT("the route starts"), [this](float) { return Dir()->Stage == EStage::Route && Dir()->Shift.bOn; }, 5.f);
	Do(TEXT("route briefing"), [this]()
	{
		ANHGameDirector* D = Dir();
		const UNHGameData* Data = UNHGameData::Get(PC);
		TArray<FString> Queues;
		for (const FName& Id : D->Shift.Route.Stops)
		{
			Queues.Add(FString::Printf(TEXT("%s %d"), *D->StopName(Id), WaitingAt(Id)));
		}
		Note(FString::Printf(TEXT("route %s; waiting: %s"), *D->Shift.Route.Name, *FString::Join(Queues, TEXT(", "))));
		Check(TEXT("the deadline is two in-game hours away and the clock runs at half speed"), FMath::IsNearlyEqual(D->Deadline - Hustle()->Minutes, Data->FirstDayClock, 2.f) && D->bSlowClock,
			FString::Printf(TEXT("%.0f minutes left"), D->Deadline - Hustle()->Minutes));
		Check(TEXT("Baba Driver's cut is 40%"), FMath::IsNearlyEqual(D->Shift.Cut, 0.4f) && D->Shift.Owner == TEXT("Baba Driver"), FString::Printf(TEXT("%.0f%% to %s"), D->Shift.Cut * 100.f, *D->Shift.Owner));
	});
}

void UNHDebugPlay::AddDriveTo(const FString& Name, TFunction<FVector2D()> Target, float Cruise, bool bStopThere, TFunction<bool()> Done, float Timeout)
{
	Until(Name, [this, Target, Cruise, bStopThere, Done](float)
	{
		ANHVehicle* V = Cast<ANHVehicle>(PC->GetPawn());
		if (!V)
		{
			return false;
		}
		if (Done())
		{
			V->SetDriveInput(0.f, 0.f, 0.f);
			return true;
		}
		Drive(V, Target(), Cruise, bStopThere);
		return false;
	}, Timeout);
}

void UNHDebugPlay::AddBoard(const FString& Name)
{
	// E once, then the right change for everybody who paid with a big note, each through its own panel
	Do(Name, [this]()
	{
		ANHGameDirector* D = Dir();
		const FName Stop = D->Shift.AtStop;
		const auto* W = D->Shift.Waiting.Find(Stop);
		const int32 Capacity = UNHGameData::Get(PC)->Conductor.Capacity;
		const int32 Expect = W ? FMath::Min(W->Num(), Capacity - D->Shift.Onboard.Num()) : 0;
		int32 ExpectFares = 0;
		TArray<FString> Who;
		for (int32 I = 0; I < Expect; ++I)
		{
			ExpectFares += (*W)[I].Fare;
			Who.Add(FString::Printf(TEXT("%s %s"), *(*W)[I].Name, *NHPlay::N((*W)[I].Fare)));
		}
		const int32 Fares = D->Shift.Fares, Seats = D->Shift.Onboard.Num();
		const FString Prompt = D->ActionPrompt(PC->GetPawn());
		PC->OnAction(); // E: call passengers
		Note(FString::Printf(TEXT("called passengers at %s (prompt \"%s\"): %s"), *D->StopName(Stop), *Prompt, *FString::Join(Who, TEXT(", "))));
		Check(FString::Printf(TEXT("%s: everyone waiting gets on (%d) and their fares add up (%s)"), *D->StopName(Stop), Expect, *NHPlay::N(ExpectFares)),
			D->Shift.Onboard.Num() - Seats == Expect && D->Shift.Fares - Fares == ExpectFares && WaitingAt(Stop) == 0,
			FString::Printf(TEXT("%d got on, fares +%s, %d still waiting"), D->Shift.Onboard.Num() - Seats, *NHPlay::N(D->Shift.Fares - Fares), WaitingAt(Stop)));
	});
	Until(TEXT("give everyone the right change"), [this](float Dt)
	{
		ANHGameDirector* D = Dir();
		if (D->Shift.ChangeQ.IsEmpty())
		{
			return !D->Panel.bOpen;
		}
		Pace -= Dt;
		if (Pace <= 0.f && AnswerChangeRight())
		{
			Pace = 0.2f;
		}
		return false;
	}, 20.f);
}

void UNHDebugPlay::AddStop(int32 Index, bool bBoard)
{
	const UNHGameData* Data = UNHGameData::Get(PC);
	const FName Stop = Data->FirstRoute.Stops[Index];
	const FNHBusStop* S = Data->Stops.Find(Stop);
	const FVector2D Kerb = S->Kerb, Fwd = StopForward(Stop);
	const FString Name = Data->FirstRoute.Labels.Contains(Stop) ? Data->FirstRoute.Labels[Stop] : S->Name;

	Do(FString::Printf(TEXT("put the danfo 30 m before %s"), *Name), [this, Kerb, Fwd]()
	{
		const FVector2D Start = ApproachStart(Bus(), Kerb, Fwd, 3000.f);
		PlaceVehicle(Bus(), Start, (Kerb - Start).GetSafeNormal());
		Note(FString::Printf(TEXT("danfo at %.0f, %.0f, %.0f m from the kerb"), Start.X, Start.Y, FVector2D::Distance(Start, Kerb) / 100.f));
	});
	AddDriveTo(FString::Printf(TEXT("drive in to %s and stop"), *Name), [Kerb]() { return Kerb; }, 600.f, true, [this, Stop]() { return Dir()->Shift.AtStop == Stop; });
	Do(FString::Printf(TEXT("arrived at %s"), *Name), [this, Stop, Kerb]()
	{
		ANHGameDirector* D = Dir();
		Check(FString::Printf(TEXT("stopping at the kerb counts as arriving at %s"), *D->StopName(Stop)), D->Shift.AtStop == Stop,
			FString::Printf(TEXT("%.1f m from the kerb at %.0f cm/s; %d on board, %d waiting, comfort %.0f%%"), FVector2D::Distance(FVector2D(Bus()->GetActorLocation()), Kerb) / 100.f, Bus()->Speed,
				D->Shift.Onboard.Num(), WaitingAt(Stop), D->Shift.Comfort));
	});
	if (S->Agbero > 0 && Stop == FName(TEXT("balogate"))) // on the first day only Balo Gate's agbero asks
	{
		Until(TEXT("the agbero comes for his ticket"), [this](float) { return Dir()->Panel.bOpen && Dir()->Panel.Title.StartsWith(TEXT("Agbero")); }, 5.f);
		Do(TEXT("pay the agbero"), [this]()
		{
			ANHGameDirector* D = Dir();
			const int32 Paid = D->Shift.AgberoPaid, Ticket = UNHGameData::Get(PC)->Conductor.FirstTicket;
			for (const FString& L : D->Panel.Lines)
			{
				Note(FString::Printf(TEXT("agbero: %s"), *L));
			}
			Agbero(TEXT("pay"));
			Check(FString::Printf(TEXT("paying the agbero costs the first-day ticket (%s) and closes his panel"), *NHPlay::N(Ticket)), D->Shift.AgberoPaid - Paid == Ticket && !D->Panel.bOpen,
				FString::Printf(TEXT("agbero tickets so far %s"), *NHPlay::N(D->Shift.AgberoPaid)));
		});
	}
	if (bBoard)
	{
		AddBoard(FString::Printf(TEXT("call passengers at %s (E)"), *Name));
	}
}

// ---------------------------------------------------------------------------------------------------- NHAutoplay
void UNHDebugPlay::Autoplay(bool bQuitWhenDone)
{
	if (NeedsFreshStory(TEXT("autoplay"), bQuitWhenDone))
	{
		return;
	}
	Begin(TEXT("autoplay"), bQuitWhenDone);
	using EStage = ANHGameDirector::EStage;
	const UNHGameData* Data = UNHGameData::Get(PC);
	const int32 CashBefore = Hustle()->Cash, CredBefore = Hustle()->Cred;
	Note(FString::Printf(TEXT("First Day on the Danfo by script. Cash before: %s, cred %d, %s"), *NHPlay::N(CashBefore), CredBefore, *Hustle()->ClockText()));

	AddMeetBaba();
	AddEnterBus();
	for (int32 I = 0; I < Data->FirstRoute.Stops.Num(); ++I)
	{
		AddStop(I, true);
	}
	// pulling away from the last stop ends the route
	const FName Last = Data->FirstRoute.Stops.Last();
	const FVector2D Onward = Data->Stops[Last].Kerb + StopForward(Last) * 2000.f;
	AddDriveTo(TEXT("pull away from the last stop"), [Onward]() { return Onward; }, 600.f, false, [this]() { return Dir()->Shift.bRouteDone; }, 20.f);
	Until(TEXT("the objective changes to bringing the danfo back"), [this](float) { return Dir()->Stage == EStage::Return; }, 5.f);
	Do(TEXT("put the danfo 30 m from the motor park"), [this, Data]()
	{
		ANHGameDirector* D = Dir();
		Check(TEXT("the route is done with nobody left waiting on it"), D->Shift.bRouteDone && D->Shift.Missed == 0, FString::Printf(TEXT("%d missed stops, %d on board"), D->Shift.Missed, D->Shift.Onboard.Num()));
		const FVector2D End = Data->Park + FVector2D(0.f, 500.f);
		const FVector2D Start = ApproachStart(Bus(), End, FVector2D(-1.f, 0.f), 3000.f);
		PlaceVehicle(Bus(), Start, (End - Start).GetSafeNormal());
		Note(FString::Printf(TEXT("danfo at %.0f, %.0f, %.0f m from the park"), Start.X, Start.Y, FVector2D::Distance(Start, Data->Park) / 100.f));
	});
	AddDriveTo(TEXT("drive back to Baba Driver"), [Data]() { return Data->Park + FVector2D(0.f, 500.f); }, 500.f, true, [this]() { return Dir()->Stage == EStage::Wrapping; });

	// the settle-up: worked out here from the shift's own counters before the game pays it
	struct FExpect { FVector2D Parked = FVector2D::ZeroVector; int32 Fares = 0, Tips = 0, Bonus = 0, Keep = 0, Penalties = 0, Agbero = 0, Damage = 0, Cut = 0, Net = 0; };
	TSharedRef<FExpect> E = MakeShared<FExpect>();
	Do(TEXT("back at the park"), [this, E, Data]()
	{
		ANHGameDirector* D = Dir();
		const auto& S = D->Shift;
		E->Fares = S.Fares; E->Tips = S.Tips; E->Bonus = S.Bonus; E->Keep = S.Keep; E->Penalties = S.Penalties; E->Agbero = S.AgberoPaid;
		E->Damage = FMath::Max(0, FMath::RoundToInt((S.HpStart - FMath::Max(0.f, Bus()->Health)) * Data->Conductor.DamageCost));
		E->Cut = FMath::RoundToInt(S.Fares * 0.4f);
		E->Net = E->Fares + E->Tips + E->Bonus + E->Keep - E->Penalties - E->Agbero - E->Damage - E->Cut;
		Check(TEXT("stopping by Baba Driver brings his closing line"), D->Dialogue.bOpen && D->Dialogue.Lines == Data->BabaLines(TEXT("done")));
		E->Parked = FVector2D(Bus()->GetActorLocation());
		Bus()->SetDriveInput(1.f, 0.f, 0.f); // foot down through the whole settle-up: the danfo must stay put
		Note(FString::Printf(TEXT("expected settle-up: fares %s + tips %s + bonus %s + kept change %s - refunds and fines %s - agbero %s - damage %s - Baba's 40%% %s = %s"), *NHPlay::N(E->Fares),
			*NHPlay::N(E->Tips), *NHPlay::N(E->Bonus), *NHPlay::N(E->Keep), *NHPlay::N(E->Penalties), *NHPlay::N(E->Agbero), *NHPlay::N(E->Damage), *NHPlay::N(E->Cut), *NHPlay::N(E->Net)));
	});
	AddSkipDialogue(TEXT("hear Baba Driver out (E)"));
	Until(TEXT("the summary opens"), [this](float) { return Dir()->Panel.bOpen; }, 5.f);
	Do(TEXT("read the summary"), [this, E, CashBefore, Data]()
	{
		ANHGameDirector* D = Dir();
		Note(FString::Printf(TEXT("summary: %s"), *D->Panel.Title));
		for (const FString& L : D->Panel.Lines)
		{
			Note(FString::Printf(TEXT("summary: %s"), *L));
		}
		const FString All = FString::Join(D->Panel.Lines, TEXT(" | "));
		const float Moved = static_cast<float>(FVector2D::Distance(E->Parked, FVector2D(Bus()->GetActorLocation())));
		Bus()->SetDriveInput(0.f, 0.f, 0.f);
		Check(TEXT("the danfo stays put while Baba Driver counts the money, even with the throttle down"), Moved < 5.f && Bus()->Speed == 0.f, FString::Printf(TEXT("moved %.0f cm"), Moved));
		Check(TEXT("the summary is titled after the job and offers Collect"), D->Panel.Title == Data->FirstDayTitle && D->Panel.Options.Num() == 1 && D->Panel.Options[0] == TEXT("Collect"), D->Panel.Title);
		Check(TEXT("the summary shows the fares collected"), All.Contains(FString::Printf(TEXT("Fares collected: %s "), *NHPlay::N(E->Fares))));
		Check(TEXT("the summary shows Baba Driver's 40% cut of the fares"), E->Cut > 0 && All.Contains(FString::Printf(TEXT("Baba Driver's cut (40%%): -%s"), *NHPlay::N(E->Cut))), NHPlay::N(E->Cut));
		Check(TEXT("the summary shows the agbero ticket"), All.Contains(FString::Printf(TEXT("Agbero tickets: -%s"), *NHPlay::N(E->Agbero))), NHPlay::N(E->Agbero));
		Check(TEXT("the summary's earnings are fares and tips less the cut, the ticket, fines and damage"), All.Contains(FString::Printf(TEXT("YOUR EARNINGS: %s"), *NHPlay::N(E->Net))), NHPlay::N(E->Net));
		const int32 Paid = E->Net >= 0 ? E->Net : -FMath::Min(CashBefore, -E->Net);
		Check(TEXT("the net is paid into cash when the summary opens"), Hustle()->Cash == CashBefore + Paid, FString::Printf(TEXT("%s + %s = %s"), *NHPlay::N(CashBefore), *NHPlay::N(Paid), *NHPlay::N(Hustle()->Cash)));
		Check(TEXT("the shift is over and its passengers are gone"), !D->Shift.bOn && D->Shift.Onboard.Num() == 0);
		PC->Choose(0); // 1: Collect
	});
	Until(TEXT("the job is marked done"), [this](float) { return Dir()->Stage == EStage::Done; }, 5.f);
	Do(TEXT("after the job"), [this, CashBefore, CredBefore, Data]()
	{
		UNHHustleSubsystem* H = Hustle();
		Check(TEXT("the first day is recorded as done, with its cred"), H->IsDone(NHPlay::FirstDay) && H->Cred == CredBefore + Data->FirstDayCred, FString::Printf(TEXT("cred %d"), H->Cred));
		const UNHSaveGame* Save = Cast<UNHSaveGame>(UGameplayStatics::LoadGameFromSlot(NHPlay::SaveSlot, 0));
		Check(TEXT("the save game is written with the new cash and the finished job"), Save && Save->Cash == H->Cash && Save->Done.Contains(NHPlay::FirstDay) && Save->Cred == H->Cred,
			Save ? FString::Printf(TEXT("save has %s, %d jobs done"), *NHPlay::N(Save->Cash), Save->Done.Num()) : TEXT("no save in the NaijaHustle slot"));
		Bus()->SetDriveInput(0.3f, 0.f, 0.f);
		Bus()->Tick(0.1f);
		Bus()->SetDriveInput(0.f, 0.f, 0.f);
		Check(TEXT("after collecting, the danfo drives again"), Bus()->Speed > 0.f, FString::Printf(TEXT("%.0f cm/s"), Bus()->Speed));
		Check(TEXT("free roam: the same danfo now offers routes"), Dir()->ActionPrompt(PC->GetPawn()) == TEXT("E  Pick a route"), Dir()->ActionPrompt(PC->GetPawn()));
		Note(FString::Printf(TEXT("Cash before: %s. Cash after: %s. %s"), *NHPlay::N(CashBefore), *NHPlay::N(H->Cash), *H->ClockText()));
	});
}

// ---------------------------------------------------------------------------------------------------- NHSelfTest
void UNHDebugPlay::SelfTest(bool bQuitWhenDone)
{
	if (NeedsFreshStory(TEXT("selftest"), bQuitWhenDone))
	{
		return;
	}
	Begin(TEXT("selftest"), bQuitWhenDone);
	using EStage = ANHGameDirector::EStage;
	const UNHGameData* Data = UNHGameData::Get(PC);
	AddMeetBaba();

	// ---- in and out of every parked vehicle, the mission danfo last (getting into it starts the route)
	Do(TEXT("get in and out of every parked vehicle"), [this]()
	{
		TArray<ANHVehicle*> All;
		for (TActorIterator<ANHVehicle> It(PC->GetWorld()); It; ++It)
		{
			if (*It != Bus())
			{
				All.Add(*It);
			}
		}
		All.Add(Bus());
		TSet<FName> Types;
		for (ANHVehicle* V : All)
		{
			ANHCharacter* C = Character();
			Types.Add(V->VehicleType);
			C->SetActorLocation(V->ExitPoint(), false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation(FRotator(-10.f, (V->GetActorLocation() - C->GetActorLocation()).Rotation().Yaw, 0.f)); // standing by its door, looking at it
			const bool bOffered = PC->NearbyVehicle() == V && PC->Prompt().Contains(TEXT("F  Get in ") + V->DisplayName());
			PC->OnInteract(); // F
			const bool bIn = PC->GetPawn() == V && C->IsHidden() && PC->GetNHInputMode() == ENHInputMode::Vehicle && PC->Prompt().Contains(TEXT("F  Get out"));
			PC->OnInteract(); // F
			const FVector Out = C->GetActorLocation();
			const float Away = static_cast<float>(FVector::Dist2D(Out, V->GetActorLocation()));
			const bool bOut = PC->GetPawn() == C && !C->IsHidden() && C->GetActorEnableCollision() && PC->GetNHInputMode() == ENHInputMode::OnFoot
				&& Away > V->GetSpec().Width * 0.5f && Away < V->GetSpec().Length * 0.5f + 200.f && !V->GetController();
			Check(FString::Printf(TEXT("%s (%s): F offers it, gets you in, and gets you out beside it"), *V->DisplayName(), *V->VehicleType.ToString()), bOffered && bIn && bOut,
				FString::Printf(TEXT("offered %d, in %d, out %d, %.1f m from its centre"), bOffered, bIn, bOut, Away / 100.f));
		}
		Check(TEXT("the park has a danfo, a keke, an okada and a car"), Types.Contains(TEXT("danfo")) && Types.Contains(TEXT("keke")) && Types.Contains(TEXT("okada")) && Types.Contains(TEXT("sedan")),
			FString::Printf(TEXT("%d vehicles, %d types"), All.Num(), Types.Num()));
	});

	// ---- a missed stop: board at the first stop, then drive straight past the second
	AddEnterBus();
	Do(TEXT("step out of the danfo mid-route (F)"), [this]() { PC->OnInteract(); });
	Until(TEXT("the game asks you back in"), [this](float) { return Dir()->ObjSub == TEXT("Get back in the danfo"); }, 3.f);
	Do(TEXT("get back in the danfo"), [this]()
	{
		Check(TEXT("stepping out of the mission danfo keeps the route waiting"), Cast<ANHCharacter>(PC->GetPawn()) && Dir()->Shift.bOn && Dir()->Stage == EStage::Route, Dir()->ObjSub);
		PC->SetControlRotation(FRotator(-10.f, (Bus()->GetActorLocation() - PC->GetPawn()->GetActorLocation()).Rotation().Yaw, 0.f));
		PC->OnInteract();
		Check(TEXT("F gets you back in"), PC->GetPawn() == Bus());
	});
	AddStop(0, true);
	const FName Second = Data->FirstRoute.Stops[1];
	const FVector2D Kerb2 = Data->Stops[Second].Kerb, Fwd2 = StopForward(Second);
	TSharedRef<TArray<int32>> Before = MakeShared<TArray<int32>>();
	Do(TEXT("line the danfo up to drive past the second stop"), [this, Second, Kerb2, Fwd2, Before]()
	{
		ANHGameDirector* D = Dir();
		if (D->Shift.Onboard.Num())
		{
			D->Shift.Onboard[0].Dest = Second; // make sure somebody wants to get down there
		}
		int32 Riders = 0, Refund = 0;
		for (const auto& P : D->Shift.Onboard)
		{
			if (P.Dest == Second)
			{
				++Riders;
				Refund += P.Fare;
			}
		}
		*Before = { D->Shift.Penalties, D->Shift.Onboard.Num(), Riders, Refund, D->Shift.Missed };
		const FVector2D Start = ApproachStart(Bus(), Kerb2, Fwd2, 2000.f);
		PlaceVehicle(Bus(), Start, (Kerb2 - Start).GetSafeNormal());
		Note(FString::Printf(TEXT("%d on board, %d of them for %s (fares %s); danfo %.0f m before the stop"), D->Shift.Onboard.Num(), Riders, *D->StopName(Second), *NHPlay::N(Refund), FVector2D::Distance(Start, Kerb2) / 100.f));
	});
	AddDriveTo(TEXT("drive past the second stop without stopping"), [Kerb2, Fwd2]() { return Kerb2 + Fwd2 * 3200.f; }, 900.f, false, [this, Before]() { return Dir()->Shift.Missed > (*Before)[4]; }, 30.f);
	Do(TEXT("missed stop"), [this, Before, Data]()
	{
		ANHGameDirector* D = Dir();
		const int32 Fine = Data->Conductor.MissedStopFine;
		Check(FString::Printf(TEXT("driving past a stop costs the %s fine and refunds the passengers for that stop"), *NHPlay::N(Fine)),
			Fine == 100 && D->Shift.Penalties - (*Before)[0] == Fine + (*Before)[3] && (*Before)[1] - D->Shift.Onboard.Num() == (*Before)[2] && D->Shift.Idx == 2,
			FString::Printf(TEXT("fines and refunds +%s (fine %s + %d fares %s), %d got down, next stop %d of %d"), *NHPlay::N(D->Shift.Penalties - (*Before)[0]), *NHPlay::N(Fine), (*Before)[2],
				*NHPlay::N((*Before)[3]), (*Before)[1] - D->Shift.Onboard.Num(), D->Shift.Idx + 1, D->Shift.Route.Stops.Num()));
	});

	// ---- a wrecked bus, then a missed deadline: Baba's lines, the panel, and "Try again"
	auto AddFail = [this, Data](const FString& Reason, const FString& PanelLine)
	{
		Until(FString::Printf(TEXT("Baba Driver reacts (%s)"), *Reason), [this](float) { return Dir()->Stage == EStage::Failed && Dir()->Dialogue.bOpen; }, 5.f);
		Do(TEXT("the job is failed"), [this, Reason, Data]()
		{
			ANHGameDirector* D = Dir();
			Check(FString::Printf(TEXT("%s: Baba Driver says his '%s' lines and the shift is cancelled without paying"), *Reason, *Reason),
				D->Dialogue.Lines == Data->BabaLines(Reason) && D->Dialogue.Lines.Num() > 0 && !D->Shift.bOn && D->DeadlineMinutesLeft < 0.f, FString::Printf(TEXT("%d lines"), D->Dialogue.Lines.Num()));
		});
		AddSkipDialogue(TEXT("hear Baba Driver out (E)"));
		Until(TEXT("the fail panel opens"), [this](float) { return Dir()->Panel.bOpen; }, 5.f);
		Do(TEXT("try again"), [this, PanelLine]()
		{
			ANHGameDirector* D = Dir();
			Check(TEXT("the fail panel says why and offers Try again"), D->Panel.Title == TEXT("WAHALA!") && D->Panel.Lines.Num() && D->Panel.Lines[0] == PanelLine && D->Panel.Options.Num() == 2 && D->Panel.Options[0] == TEXT("Try again"),
				D->Panel.Lines.Num() ? D->Panel.Lines[0] : FString());
			PC->Choose(0); // 1: Try again
		});
		Do(TEXT("back at the start"), [this, Data]()
		{
			ANHGameDirector* D = Dir();
			ANHVehicle* V = Bus();
			Check(TEXT("Try again: on foot at the park, a fresh danfo in bay 1, Baba Driver's retry line"),
				D->Stage == EStage::Talk && D->Dialogue.Lines == Data->BabaLines(TEXT("retry")) && Cast<ANHCharacter>(PC->GetPawn()) && V && !V->IsWrecked() && V->Health == V->MaxHealth
				&& FVector2D::Distance(FVector2D(V->GetActorLocation()), Data->ParkBays[0].Pos) < 50.f && FVector2D::Distance(FVector2D(PC->GetPawn()->GetActorLocation()), Data->Park) < 600.f,
				FString::Printf(TEXT("stage %s, danfo health %.0f"), *StageName(), V ? V->Health : -1.f));
		});
		AddSkipDialogue(TEXT("hear Baba Driver out (E)"));
		Do(TEXT("ready to board again"), [this]() { Check(TEXT("after the retry line the objective is to get in the danfo"), Dir()->Stage == EStage::Board, Dir()->ObjText); });
	};

	const int32 CashBefore = Hustle()->Cash;
	Do(TEXT("wreck the danfo (health 0)"), [this]() { Bus()->Health = 0.f; });
	AddFail(TEXT("wrecked"), TEXT("The danfo is wrecked."));
	AddEnterBus();
	Do(TEXT("run the deadline out"), [this]() { Hustle()->Minutes = Dir()->Deadline + 1.f; });
	AddFail(TEXT("late"), TEXT("You ran out of time."));
	Do(TEXT("money"), [this, CashBefore]()
	{
		Check(TEXT("failing costs no money and the job is still open"), Hustle()->Cash == CashBefore && !Hustle()->IsDone(NHPlay::FirstDay), NHPlay::N(Hustle()->Cash));
	});

	// ---- the lighting debug menu (F1)
	Do(TEXT("lighting menu: golden evening"), [this]()
	{
		PC->NHLightMenu();
		ANHGameDirector* D = Dir();
		Check(TEXT("F1 opens the lighting menu with both looks and the clock"), D->Panel.bOpen && D->Panel.Options.Num() == 4 && D->Panel.Options[0] == TEXT("Harsh morning") && D->Panel.Options[1] == TEXT("Golden evening"), D->Panel.Title);
		PC->Choose(1);
		const ANHLightingRig* Rig = ANHLightingRig::Find(PC);
		Check(TEXT("picking Golden evening sets it and stops the clock changing the light"), Rig && Rig->Preset == ENHLightingPreset::GoldenEvening && D->bManualLighting && !D->Panel.bOpen);
	});
	Do(TEXT("lighting menu: follow the clock"), [this]()
	{
		PC->NHLightMenu();
		PC->Choose(2);
	});
	Until(TEXT("the clock takes the light back"), [this](float) { const ANHLightingRig* Rig = ANHLightingRig::Find(PC); return Rig && Rig->Preset != ENHLightingPreset::GoldenEvening; }, 5.f);
	Do(TEXT("lighting follows the clock again"), [this]()
	{
		const ANHLightingRig* Rig = ANHLightingRig::Find(PC);
		const float H = Hustle()->HourOfDay();
		Check(TEXT("Follow the clock gives the light back to the time of day (harsh morning from 8:00 to 11:30)"), !Dir()->bManualLighting && (H < 8.f || H >= 11.5f || Rig->Preset == ENHLightingPreset::HarshMorning),
			FString::Printf(TEXT("%s, preset %d"), *Hustle()->ClockText(), static_cast<int32>(Rig->Preset)));
	});
}
