#include "Debug/NHDebugPlay.h"

#include "Characters/NHAdvancedMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Characters/NHCharacterEffectsComponent.h"
#include "Components/BoxComponent.h"
#include "Core/NHGameData.h"
#include "Core/NHHustleSubsystem.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "Gameplay/NHGameDirector.h"
#include "Gameplay/NHGuard.h"
#include "Gameplay/NHLeads.h"
#include "Gameplay/NHMissions.h"
#include "Gameplay/NHPerson.h"
#include "Kismet/GameplayStatics.h"
#include "Lighting/NHLightingRig.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "NaijaHustleGame.h"
#include "Player/NHCharacter.h"
#include "Player/NHPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Vehicles/NHVehicle.h"
#include "Vehicles/NHVehicleDynamicsComponent.h"
#include "Vehicles/NHVehicleMaterialComponent.h"

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
	// slow for the target so it can stop there: v² = 2 a d. Half brake takes off Accel * 0.15 a second (ANHVehicle::Drive);
	// the plan counts on four fifths of that, so the bus is under the limit when it reaches the kerb and does not run past it
	const float Limit = bStopThere ? FMath::Min(Cruise, FMath::Sqrt(FMath::Max(0.f, Dist - 80.f) * V->GetSpec().Accel * 0.24f)) : Cruise;
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

ANHMomentumDummy::ANHMomentumDummy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UNHAdvancedMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	AutoPossessAI = EAutoPossessAI::Disabled;
	GetCharacterMovement()->bRunPhysicsWithNoController = true; // nobody possesses it: the script feeds it input
}

void UNHDebugPlay::AddMomentumChecks()
{
	// a long straight piece of the dusty street, heading west
	static const FVector Start(7700.f, 14000.f, 300.f);
	static const FVector West(-1.f, 0.f, 0.f);
	Do(TEXT("momentum movement: stand on the dusty street"), [this]()
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->SetActorLocation(Start + FVector(300.f, 200.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MoveDummy = PC->GetWorld()->SpawnActor<ANHMomentumDummy>(ANHMomentumDummy::StaticClass(), Start, West.Rotation(), Params);
		MoveT = MoveStopT = 0.f; bMoveSawHeavyStop = bMoveHadPrediction = false;
	});
	Until(TEXT("momentum movement: the test character lands"), [this](float) { return MoveDummy && MoveDummy->GetCharacterMovement()->IsMovingOnGround(); }, 6.f);
	Until(TEXT("momentum movement: sprint for three seconds"), [this](float Dt)
	{
		UNHAdvancedMovementComponent* Move = MoveDummy ? Cast<UNHAdvancedMovementComponent>(MoveDummy->GetCharacterMovement()) : nullptr;
		if (!Move)
		{
			return true;
		}
		Move->SetSprinting(true);
		MoveDummy->AddMovementInput(West, 1.f);
		MoveT += Dt;
		if (MoveT < 3.f)
		{
			return false;
		}
		const float Speed = Move->Velocity.Size2D();
		Check(TEXT("sprinting builds past the jog speed towards the sprint speed"), Move->IsSprinting() && Speed > Move->MaxWalkSpeed + 150.f && Speed <= Move->SprintSpeed + 1.f,
			FString::Printf(TEXT("%.0f cm/s (jog %.0f, sprint %.0f)"), Speed, Move->MaxWalkSpeed, Move->SprintSpeed));
		const TArray<FNHAdvancedTrajectorySample>& Samples = Move->GetTrajectorySamples();
		bool bOrdered = Samples.Num() > 1;
		for (int32 i = 1; i < Samples.Num(); ++i)
		{
			bOrdered &= Samples[i].Time > Samples[i - 1].Time;
		}
		const int32 Want = Move->HistoryOutputSamples + 1 + Move->PredictionOutputSamples;
		const float Behind = Samples.Num() ? Samples[0].Position.X - MoveDummy->GetActorLocation().X : 0.f;
		const float Ahead = Samples.Num() ? MoveDummy->GetActorLocation().X - Samples.Last().Position.X : 0.f;
		Check(TEXT("the trajectory has past, present and predicted samples in time order, trailing behind and reaching ahead along the run"),
			Samples.Num() == Want && bOrdered && Behind > 200.f && Ahead > 200.f && Move->GetPoseSearchTrajectory().Samples.Num() == Want,
			FString::Printf(TEXT("%d samples, %.0f cm behind, %.0f cm ahead"), Samples.Num(), Behind, Ahead));
		MoveStopFrom = MoveDummy->GetActorLocation();
		return true;
	}, 8.f);
	Until(TEXT("momentum movement: let go and stop"), [this](float Dt)
	{
		UNHAdvancedMovementComponent* Move = MoveDummy ? Cast<UNHAdvancedMovementComponent>(MoveDummy->GetCharacterMovement()) : nullptr;
		if (!Move)
		{
			return true;
		}
		MoveStopT += Dt;
		bMoveSawHeavyStop |= Move->IsHeavyStopping();
		if (!bMoveHadPrediction && Move->IsHeavyStopping())
		{
			bMoveHadPrediction = Move->GetPredictedStopLocation(MoveStopPredicted);
		}
		if (MoveStopT < 0.1f || Move->Velocity.Size2D() > 1.f || Move->IsHeavyStopping())
		{
			return false;
		}
		const FVector Rest = MoveDummy->GetActorLocation();
		const float Carried = FVector::Dist2D(MoveStopFrom, Rest);
		Check(TEXT("letting go at a sprint commits to a heavy stop that carries a little and plants within about a second"),
			bMoveSawHeavyStop && Carried > 50.f && Carried < 400.f && MoveStopT > 0.25f && MoveStopT < 1.3f, FString::Printf(TEXT("carried %.0f cm in %.2f s"), Carried, MoveStopT));
		Check(TEXT("the stop location predicted at the start of the stop is where the character comes to rest"), bMoveHadPrediction && FVector::Dist2D(MoveStopPredicted, Rest) < 40.f,
			FString::Printf(TEXT("%.0f cm out"), FVector::Dist2D(MoveStopPredicted, Rest)));
		MoveDummy->Destroy();
		MoveDummy = nullptr;
		return true;
	}, 5.f);
}

void UNHDebugPlay::CarShow(const FVector& At, const FString& Folder)
{
	const UNHGameData* D = UNHGameData::Get(PC);
	ANHGameDirector* Director = Dir();
	if (!D || !Director)
	{
		return;
	}
	const float Along = 750.f, Across = 260.f;
	TArray<FName> Types;
	D->Vehicles.GetKeys(Types);
	Types.Sort(FNameLexicalLess());
	FString List;
	for (int32 i = 0; i < Types.Num(); ++i)
	{
		const FNHVehicleSpec& Spec = D->Spec(Types[i]);
		Director->SpawnVehicle(Types[i], FVector2D(At.X + (i / 2) * Along, At.Y + (i % 2 ? Across : -Across)), 0.f, Spec.Colors.Num() ? Spec.Colors[0] : FLinearColor::White, FString());
		List += (i ? TEXT(", ") : TEXT("")) + Types[i].ToString();
	}
	Note(FString::Printf(TEXT("NHCarShow: from %.0f, %.0f going east, near row then far row in each pair: %s"), At.X, At.Y, *List));
	if (Folder.IsEmpty())
	{
		return;
	}

	// pictures: two from above and to one side, each of half the line, then each pair from in front at head height
	ACameraActor* Cam = PC->GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(At));
	Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
	Cam->GetCameraComponent()->SetFieldOfView(70.f);
	if (AHUD* Hud = PC->GetHUD())
	{
		Hud->bShowHUD = false;
	}
	const int32 Columns = (Types.Num() + 1) / 2;
	struct FShot { FVector From, At; FString Name; };
	TArray<FShot> Shots;
	for (int32 Half = 0; Half < 2; ++Half)
	{
		const FVector Middle(At.X + (Half ? 0.75f : 0.25f) * (Columns - 1) * Along, At.Y, At.Z);
		Shots.Add({ Middle + FVector(0.f, -1100.f, 2400.f), Middle, FString::Printf(TEXT("above%d"), Half + 1) });
	}
	for (int32 Column = 0; Column < Columns; ++Column)
	{
		const FVector Pair(At.X + Column * Along, At.Y, At.Z + 90.f);
		Shots.Add({ Pair + FVector(880.f, 0.f, 170.f), Pair, FString::Printf(TEXT("front%d_%s_%s"), Column + 1, *Types[Column * 2].ToString(), Types.IsValidIndex(Column * 2 + 1) ? *Types[Column * 2 + 1].ToString() : TEXT("")) });
	}
	TWeakObjectPtr<ANHPlayerController> Player(PC);
	TWeakObjectPtr<ACameraActor> Camera(Cam);
	for (int32 i = 0; i < Shots.Num(); ++i)
	{
		const FShot Shot = Shots[i];
		const FString File = Folder / Shot.Name + TEXT(".png");
		const bool bLast = i == Shots.Num() - 1;
		FTimerHandle Aim, Take;
		PC->GetWorldTimerManager().SetTimer(Aim, FTimerDelegate::CreateWeakLambda(PC, [Player, Camera, Shot]()
		{
			if (Player.IsValid() && Camera.IsValid())
			{
				Camera->SetActorLocationAndRotation(Shot.From, (Shot.At - Shot.From).Rotation());
				Player->SetViewTarget(Camera.Get());
			}
		}), 2.f + i * 5.f, false);
		PC->GetWorldTimerManager().SetTimer(Take, FTimerDelegate::CreateWeakLambda(PC, [Player, File, bLast]()
		{
			if (Player.IsValid())
			{
				Player->ConsoleCommand(FString::Printf(TEXT("HighResShot 1920x1080 filename=\"%s\""), *File));
				UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: car show picture %s%s"), *File, bLast ? TEXT(" RESULT: car show done") : TEXT(""));
			}
		}), 5.f + i * 5.f, false);
	}
}

void UNHDebugPlay::DriveShots(FName Type, const FString& Folder)
{
	ANHVehicle* Car = nullptr;
	for (TActorIterator<ANHVehicle> It(PC->GetWorld()); It; ++It)
	{
		// the last one of the type: the first danfo is the mission bus, which has its own driver
		Car = It->VehicleType == Type && !It->GetController() ? *It : Car;
	}
	if (!Car || !PC->EnterVehicle(Car))
	{
		Note(FString::Printf(TEXT("NHDriveShots: could not get into a %s"), *Type.ToString()));
		return;
	}
	if (AHUD* Hud = PC->GetHUD())
	{
		Hud->bShowHUD = false;
	}
	PC->NHTime(12.f);
	ACameraActor* Cam = PC->GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(Car->GetActorLocation()));
	Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
	Cam->GetCameraComponent()->SetFieldOfView(55.f);
	const FVector Seat = Car->GetActorLocation() + FVector(0.f, 0.f, 30.f);
	const FVector From = Seat + Car->GetActorForwardVector() * 520.f - Car->GetActorRightVector() * 330.f + FVector(0.f, 0.f, 90.f);
	Cam->SetActorLocationAndRotation(From, (Seat - From).Rotation());
	PC->SetViewTarget(Cam);
	TWeakObjectPtr<ANHPlayerController> Player(PC);
	TWeakObjectPtr<ANHVehicle> Vehicle(Car);
	const auto After = [this](float Seconds, TFunction<void()> Do)
	{
		FTimerHandle Handle;
		PC->GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(PC, MoveTemp(Do)), Seconds, false);
	};
	After(4.f, [Player, Folder]() { if (Player.IsValid()) { Player->ConsoleCommand(FString::Printf(TEXT("HighResShot 1920x1080 filename=\"%s\""), *(Folder / TEXT("driver.png")))); } });
	After(5.5f, [Player, Vehicle]() { if (Player.IsValid() && Vehicle.IsValid()) { Vehicle->SetCabinView(true); Player->SetViewTarget(Vehicle.Get()); } });
	After(8.f, [Player, Folder]() { if (Player.IsValid()) { Player->ConsoleCommand(FString::Printf(TEXT("HighResShot 1920x1080 filename=\"%s\""), *(Folder / TEXT("cabin.png")))); } });
	After(10.f, [Player, Vehicle]()
	{
		if (Player.IsValid() && Vehicle.IsValid())
		{
			Vehicle->SetCabinView(false);
			Player->NHTime(21.5f);
			Vehicle->SetHeadlights(true);
			Player->SetViewTarget(Vehicle.Get());
		}
	});
	After(16.f, [Player, Vehicle, Folder]()
	{
		if (Player.IsValid())
		{
			Player->ConsoleCommand(FString::Printf(TEXT("HighResShot 1920x1080 filename=\"%s\""), *(Folder / TEXT("headlights.png"))));
			UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: drive shots saved, headlights %s. RESULT: drive shots done"), Vehicle.IsValid() && Vehicle->HeadlightsOn() ? TEXT("on") : TEXT("off"));
		}
	});
}

void UNHDebugPlay::SkinShots(const FString& Folder)
{
	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		return;
	}
	if (AHUD* Hud = PC->GetHUD())
	{
		Hud->bShowHUD = false;
	}
	PC->NHTime(10.f);
	ACameraActor* Cam = PC->GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(Pawn->GetActorLocation()));
	Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
	Cam->GetCameraComponent()->SetFieldOfView(40.f);
	TWeakObjectPtr<ANHPlayerController> Player(PC);
	TWeakObjectPtr<ACameraActor> Camera(Cam);
	TWeakObjectPtr<APawn> Body(Pawn);
	for (int32 i = 0; i < 2; ++i)
	{
		FTimerHandle Aim, Take;
		PC->GetWorldTimerManager().SetTimer(Aim, FTimerDelegate::CreateWeakLambda(PC, [Player, Camera, Body, i]()
		{
			if (Player.IsValid() && Camera.IsValid() && Body.IsValid())
			{
				const FVector At = Body->GetActorLocation() + FVector(0.f, 0.f, i ? 72.f : 0.f);
				const FVector From = At + Body->GetActorForwardVector() * (i ? 95.f : 330.f) + Body->GetActorRightVector() * (i ? 30.f : 90.f) + FVector(0.f, 0.f, i ? 4.f : 20.f);
				Camera->SetActorLocationAndRotation(From, (At - From).Rotation());
				Player->SetViewTarget(Camera.Get());
			}
		}), 3.f + i * 5.f, false);
		PC->GetWorldTimerManager().SetTimer(Take, FTimerDelegate::CreateWeakLambda(PC, [Player, Folder, i]()
		{
			if (Player.IsValid())
			{
				Player->ConsoleCommand(FString::Printf(TEXT("HighResShot 1280x1280 filename=\"%s\""), *(Folder / (i ? TEXT("face.png") : TEXT("body.png")))));
				UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: skin picture %d%s"), i + 1, i ? TEXT(" RESULT: skin shots done") : TEXT(""));
			}
		}), 6.f + i * 5.f, false);
	}
}

void UNHDebugPlay::PaintDemo(const FVector& At)
{
	UMaterialInterface* BodyMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NaijaHustle/Environment/Materials/MI_NHCarPaint_Body.MI_NHCarPaint_Body"));
	UMaterialInterface* GlassMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NaijaHustle/Environment/Materials/MI_NHCarPaint_Glass.MI_NHCarPaint_Glass"));
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!BodyMat || !GlassMat || !Sphere)
	{
		Note(TEXT("NHPaintDemo: run Scripts/nh_car_paint.py first"));
		return;
	}
	FHitResult Ground;
	const float Z = PC->GetWorld()->LineTraceSingleByChannel(Ground, At + FVector(0, 0, 2000.f), At - FVector(0, 0, 2000.f), ECC_Visibility) ? Ground.ImpactPoint.Z : At.Z;
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector P(At.X, At.Y + (i - 1.5f) * 170.f, Z + 75.f);
		AActor* Body = PC->GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(P));
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Body);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(Sphere);
		Mesh->SetMaterial(0, i == 3 ? GlassMat : BodyMat);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Body->SetRootComponent(Mesh);
		Mesh->RegisterComponent();
		Body->SetActorLocation(P);
		Body->SetActorScale3D(FVector(1.3f));

		UNHVehicleMaterialComponent* Fx = NewObject<UNHVehicleMaterialComponent>(Body);
		Fx->bCheckRainShelter = false;
		Fx->bOnlyUpdateWhenRendered = false;
		Fx->RegisterComponent();
		Fx->InitializeEffects();
		FNHVehiclePaint Paint;
		Paint.Color = i == 1 ? FLinearColor(0.02f, 0.06f, 0.35f) : i == 2 ? FLinearColor(0.02f, 0.02f, 0.02f) : Paint.Color;
		if (i != 3)
		{
			Fx->SetPaint(Paint);
		}
		const FVector ToViewer = PC->GetPawn() ? (PC->GetPawn()->GetActorLocation() - P).GetSafeNormal2D() : FVector(1.f, 0.f, 0.f);
		if (i == 1 || i == 3) // crashed paint, cracked glass: marks on the side facing the player
		{
			Fx->ApplyImpact(P + ToViewer * 62.f + FVector(0, 18.f, 8.f), 1.f);
			Fx->ApplyImpact(P + ToViewer * 50.f + FVector(0, -30.f, 35.f), 0.6f);
		}
		if (i == 2) // wet: as if it had been standing in the rain
		{
			if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))
			{
				Fx->SetComponentTickEnabled(false);
				MID->SetScalarParameterValue(TEXT("Wetness"), 1.f);
				MID->SetScalarParameterValue(TEXT("RainIntensity"), 1.f);
			}
		}
	}
	Note(FString::Printf(TEXT("NHPaintDemo: clean, crashed, wet and glass test bodies at %.0f, %.0f"), At.X, At.Y));
}

void UNHDebugPlay::AddVehicleDynamicsChecks()
{
	Do(TEXT("vehicle dynamics: grip and slides"), [this]()
	{
		// the grip model on its own, fed speeds and turn rates directly
		AActor* Rig = PC->GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(PC->GetPawn()->GetActorLocation() + FVector(0.f, 0.f, 500.f)));
		UNHVehicleDynamicsComponent* Dyn = NewObject<UNHVehicleDynamicsComponent>(Rig);
		Dyn->RegisterComponent();
		const float Dt = 1.f / 60.f;

		float Speed = 900.f, Slid = 0.f;
		for (int32 i = 0; i < 60; ++i) { Slid += FMath::Abs(Dyn->StepTraction(Dt, Speed, 0.6f, false)); }
		Check(TEXT("an ordinary corner stays inside the grip: no slide and no speed lost"), Slid == 0.f && !Dyn->IsSliding() && Speed == 900.f, FString::Printf(TEXT("slid %.1f cm"), Slid));

		Speed = 2400.f;
		for (int32 i = 0; i < 30; ++i) { Dyn->StepTraction(Dt, Speed, 2.4f, false); }
		const float SlipAtLimit = Dyn->GetSlipAngle();
		Check(TEXT("full lock at speed breaks traction: the car slides outwards (left, in a right turn) and scrubs speed"),
			Dyn->IsSliding() && Dyn->GetSideSpeed() < -100.f && SlipAtLimit < -5.f && Speed < 2400.f, FString::Printf(TEXT("slip %.1f degrees, side %.0f cm/s, speed %.0f"), SlipAtLimit, Dyn->GetSideSpeed(), Speed));

		float Recover = 0.f;
		while (Dyn->IsSliding() && Recover < 5.f) { Dyn->StepTraction(Dt, Speed, 0.f, false); Recover += Dt; }
		Check(TEXT("straightening the wheel catches the slide in well under two seconds"), !Dyn->IsSliding() && Recover > 0.05f && Recover < 2.f && FMath::Abs(Dyn->GetSideSpeed()) <= Dyn->SlideEndSpeed, FString::Printf(TEXT("%.2f s"), Recover));

		Dyn->ResetDynamics();
		Speed = 1200.f;
		for (int32 i = 0; i < 30; ++i) { Dyn->StepTraction(Dt, Speed, 1.5f, false); }
		const bool bGripped = !Dyn->IsSliding();
		for (int32 i = 0; i < 30; ++i) { Dyn->StepTraction(Dt, Speed, 1.5f, true); }
		Check(TEXT("the same corner holds without the handbrake and slides with it"), bGripped && Dyn->IsSliding(), FString::Printf(TEXT("slip %.1f degrees"), Dyn->GetSlipAngle()));

		// wet roads
		Dyn->ResetDynamics();
		Speed = 500.f;
		Dyn->StepTraction(Dt, Speed, 0.f, false);
		const float DryGrip = Dyn->GetCurrentGrip();
		ANHLightingRig* Lights = ANHLightingRig::Find(PC);
		UMaterialParameterCollection* MPC = Lights ? Lights->Weather.LoadSynchronous() : nullptr;
		if (MPC)
		{
			UKismetMaterialLibrary::SetScalarParameterValue(PC, MPC, TEXT("Wetness"), 1.f);
		}
		Dyn->StepTraction(Dt, Speed, 0.f, false);
		const float WetGrip = Dyn->GetCurrentGrip();
		if (Lights)
		{
			Lights->ApplyPreset(Lights->Preset);
		}
		Check(TEXT("a wet road from the game's weather cuts the grip"), MPC && FMath::IsNearlyEqual(WetGrip, DryGrip * Dyn->WetGripRatio, 1.f), FString::Printf(TEXT("%.0f dry, %.0f wet"), DryGrip, WetGrip));

		// the same slide at 20 and at 120 frames a second
		float Distance[2] = { 0.f, 0.f };
		const float Rates[2] = { 20.f, 120.f };
		for (int32 r = 0; r < 2; ++r)
		{
			Dyn->ResetDynamics();
			float V = 2400.f;
			const int32 Half = FMath::RoundToInt(0.5f * Rates[r]);
			for (int32 i = 0; i < Half * 2; ++i) { Distance[r] += Dyn->StepTraction(1.f / Rates[r], V, i < Half ? 2.4f : 0.f, false); }
		}
		Check(TEXT("a slide covers the same ground at 20 and at 120 frames a second (fixed sub-steps)"), Distance[0] < -50.f && FMath::Abs(Distance[0] - Distance[1]) < FMath::Abs(Distance[1]) * 0.06f,
			FString::Printf(TEXT("%.0f cm and %.0f cm"), Distance[0], Distance[1]));
		Rig->Destroy();
	});

	Do(TEXT("vehicle dynamics: weight transfer and suspension"), [this]()
	{
		// a real car on the dusty street, moved by hand so the body's answer can be read off
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FRotator West(0.f, 180.f, 0.f);
		ANHVehicle* Car = PC->GetWorld()->SpawnActorDeferred<ANHVehicle>(ANHVehicle::StaticClass(), FTransform(West, FVector(7900.f, 14000.f, 200.f)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Check(TEXT("a test car spawns"), Car != nullptr))
		{
			return;
		}
		Car->VehicleType = TEXT("sedan");
		Car->FinishSpawning(FTransform(West, FVector(7900.f, 14000.f, 200.f)));
		UNHVehicleDynamicsComponent* Dyn = Car->GetDynamics();
		const float Dt = 1.f / 60.f;
		FVector At = Car->GetActorLocation();
		float Yaw = 180.f;
		auto Move = [&](float Speed, float TurnRate, int32 Steps, TFunctionRef<void()> Each)
		{
			for (int32 i = 0; i < Steps; ++i)
			{
				Yaw += FMath::RadiansToDegrees(TurnRate) * Dt;
				At += FRotator(0.f, Yaw, 0.f).Vector() * Speed * Dt;
				Car->SetActorLocationAndRotation(At, FRotator(0.f, Yaw, 0.f));
				Dyn->StepChassis(Dt);
				Each();
			}
		};
		Dyn->ResetDynamics();
		Move(1000.f, 0.f, 90, []() {}); // up to a steady 36 km/h, body settled
		Check(TEXT("the car has four sprung wheels and knows it is on a dirt road (less grip than asphalt)"), Dyn->GetWheelCount() == 4 && Dyn->GetSurfaceGrip() < 0.8f, FString::Printf(TEXT("%d wheels, surface grip %.2f"), Dyn->GetWheelCount(), Dyn->GetSurfaceGrip()));

		// brake hard
		float MinPitch = 0.f, FrontPressed = 0.f, RearPressed = 0.f, Speed = 1000.f;
		for (int32 i = 0; i < 50 && Speed > 0.f; ++i)
		{
			Speed = FMath::Max(0.f, Speed - 1200.f * Dt);
			Move(Speed, 0.f, 1, [&]()
			{
				if (Dyn->GetBodyPitch() < MinPitch)
				{
					MinPitch = Dyn->GetBodyPitch();
					FrontPressed = Dyn->GetWheelCompression(0);
					RearPressed = Dyn->GetWheelCompression(2);
				}
			});
		}
		Check(TEXT("hard braking pitches the nose down, pressing the front wheels up into the arches and unloading the rear"), MinPitch < -1.5f && FrontPressed > 0.5f && RearPressed < -0.5f,
			FString::Printf(TEXT("pitch %.1f degrees, front %+.1f cm, rear %+.1f cm"), MinPitch, FrontPressed, RearPressed));

		// a right-hand bend at a steady speed
		Move(1000.f, 0.f, 90, []() {});
		float MinRoll = 0.f;
		Move(1000.f, 1.2f, 60, [&]() { MinRoll = FMath::Min(MinRoll, Dyn->GetBodyRoll()); });
		Check(TEXT("a right-hand bend rolls the body outwards, to the left"), MinRoll < -1.5f, FString::Printf(TEXT("roll %.1f degrees"), MinRoll));

		// stop and let it settle (on whatever slope the ground under the wheels has: the test car is not following it)
		Move(0.f, 0.f, 200, []() {});
		const float RestPitch = Dyn->GetBodyPitch(), RestRoll = Dyn->GetBodyRoll(), RestHeave = Dyn->GetBodyHeave();
		Move(0.f, 0.f, 40, []() {});
		Check(TEXT("standing still, the body comes to rest"), FMath::Abs(Dyn->GetBodyPitch() - RestPitch) < 0.02f && FMath::Abs(Dyn->GetBodyRoll() - RestRoll) < 0.02f && FMath::Abs(Dyn->GetBodyHeave() - RestHeave) < 0.02f,
			FString::Printf(TEXT("pitch %.2f, roll %.2f, heave %.2f"), Dyn->GetBodyPitch(), Dyn->GetBodyRoll(), Dyn->GetBodyHeave()));

		// a kerb: the car is lifted 12 cm at once
		const float Before = Dyn->GetBodyHeave();
		At.Z += 12.f;
		float Lowest = Before;
		Move(0.f, 0.f, 45, [&]() { Lowest = FMath::Min(Lowest, Dyn->GetBodyHeave()); });
		Move(0.f, 0.f, 200, []() {});
		const float Late = Dyn->GetBodyHeave();
		Move(0.f, 0.f, 40, []() {});
		Check(TEXT("hitting a kerb compresses the suspension within its travel, and it comes to rest again"), Lowest - Before < -2.f && Lowest >= -Dyn->SuspensionTravel && FMath::Abs(Dyn->GetBodyHeave() - Late) < 0.02f,
			FString::Printf(TEXT("compressed %.1f cm, at rest %.2f cm"), Lowest - Before, Dyn->GetBodyHeave()));
		Car->Destroy();
	});
}

void UNHDebugPlay::AddVehiclePaintChecks()
{
	// a test body: a cube twice life size, wearing the car paint material
	Do(TEXT("vehicle paint: a test body takes impacts"), [this]()
	{
		UMaterialInterface* PaintMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NaijaHustle/Environment/Materials/M_NHCarPaint.M_NHCarPaint"));
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		if (!Check(TEXT("the car paint material exists (Scripts/nh_car_paint.py makes it)"), PaintMat && Cube))
		{
			return;
		}
		const FVector At = PC->GetPawn()->GetActorLocation() + FVector(0.f, 600.f, 100.f);
		PaintBody = PC->GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(At));
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(PaintBody);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(Cube);
		Mesh->SetMaterial(0, PaintMat);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PaintBody->SetRootComponent(Mesh);
		Mesh->RegisterComponent();
		PaintBody->SetActorLocation(At);
		PaintBody->SetActorScale3D(FVector(2.f));

		UNHVehicleMaterialComponent* Fx = NewObject<UNHVehicleMaterialComponent>(PaintBody);
		Fx->bOnlyUpdateWhenRendered = false; // the camera may be looking elsewhere
		Fx->bCheckRainShelter = false;
		Fx->RegisterComponent();
		Fx->InitializeEffects();
		UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
		Check(TEXT("the component finds the paint slot and puts a pooled dynamic material on it"), Fx->GetDrivenSlotCount() == 1 && MID && MID->Parent == PaintMat);
		if (!MID)
		{
			return;
		}

		Fx->ApplyImpact(At + FVector(50.f, 0.f, 0.f), 1.f);
		FLinearColor Sphere, Data;
		MID->GetVectorParameterValue(FHashedMaterialParameterInfo(FMaterialParameterInfo(TEXT("DamageHit_0_Sphere"))), Sphere);
		MID->GetVectorParameterValue(FHashedMaterialParameterInfo(FMaterialParameterInfo(TEXT("DamageHit_0_Data"))), Data);
		Check(TEXT("an impact is passed to the material in the mesh's own space: 50 cm out on a body at double scale is 25 units, with the radius halved too"),
			Fx->GetActiveHits().Num() == 1 && FMath::IsNearlyEqual(Sphere.R, 25.f, 0.5f) && FMath::Abs(Sphere.G) < 0.5f && FMath::IsNearlyEqual(Sphere.A, Fx->ImpactRadius * 0.5f, 0.5f) && Data.R > 0.99f && Data.G > 0.99f,
			FString::Printf(TEXT("centre %.1f %.1f %.1f, radius %.1f, scratch %.2f, crack %.2f"), Sphere.R, Sphere.G, Sphere.B, Sphere.A, Data.R, Data.G));

		Fx->ApplyImpact(At + FVector(60.f, 0.f, 0.f), 0.5f);
		Fx->ApplyImpact(At + FVector(-90.f, 0.f, 0.f), 0.2f);
		MID->GetVectorParameterValue(FHashedMaterialParameterInfo(FMaterialParameterInfo(TEXT("DamageHit_1_Data"))), Data);
		Check(TEXT("a second knock on the same spot deepens the mark; a light knock elsewhere makes a new one that scratches paint but does not crack glass"),
			Fx->GetActiveHits().Num() == 2 && Data.R > 0.15f && Data.R < 0.25f && Data.G == 0.f, FString::Printf(TEXT("%d marks, light one: scratch %.2f, crack %.2f"), Fx->GetActiveHits().Num(), Data.R, Data.G));

		FNHVehiclePaint Blue;
		Blue.Color = FLinearColor(0.02f, 0.08f, 0.4f);
		Blue.ClearCoatRoughness = 0.2f;
		Fx->SetPaint(Blue);
		FLinearColor Colour;
		float CoatRough = 0.f;
		MID->GetVectorParameterValue(FHashedMaterialParameterInfo(FMaterialParameterInfo(TEXT("PaintColor"))), Colour);
		MID->GetScalarParameterValue(FHashedMaterialParameterInfo(FMaterialParameterInfo(TEXT("ClearCoatRoughness"))), CoatRough);
		Check(TEXT("a respray reaches the material: colour and clear coat"), Colour.Equals(Blue.Color, 0.001f) && FMath::IsNearlyEqual(CoatRough, 0.2f, 0.001f));

		// make it rain on the test body
		if (const ANHLightingRig* Rig = ANHLightingRig::Find(PC))
		{
			UKismetMaterialLibrary::SetScalarParameterValue(PC, Rig->Weather.LoadSynchronous(), TEXT("Rain"), 1.f);
		}
		PaintT = 0.f;
	});
	Until(TEXT("vehicle paint: two seconds of rain"), [this](float Dt) { PaintT += Dt; return !PaintBody || PaintT >= 2.f; }, 6.f);
	Do(TEXT("vehicle paint: wet, then repaired"), [this]()
	{
		UNHVehicleMaterialComponent* Fx = PaintBody ? PaintBody->FindComponentByClass<UNHVehicleMaterialComponent>() : nullptr;
		UStaticMeshComponent* Mesh = PaintBody ? PaintBody->FindComponentByClass<UStaticMeshComponent>() : nullptr;
		UMaterialInstanceDynamic* MID = Mesh ? Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)) : nullptr;
		if (ANHLightingRig* Rig = ANHLightingRig::Find(PC))
		{
			Rig->ApplyPreset(Rig->Preset); // the weather back to what the preset says
		}
		if (!Fx || !MID)
		{
			return;
		}
		float Wet = 0.f, Rain = 0.f;
		MID->GetScalarParameterValue(FHashedMaterialParameterInfo(FMaterialParameterInfo(TEXT("Wetness"))), Wet);
		MID->GetScalarParameterValue(FHashedMaterialParameterInfo(FMaterialParameterInfo(TEXT("RainIntensity"))), Rain);
		Check(TEXT("rain from the game's weather soaks the paint and switches on the ripples"), Fx->GetWetness() > 0.5f && FMath::IsNearlyEqual(Wet, Fx->GetWetness(), 0.02f) && Rain > 0.99f,
			FString::Printf(TEXT("wetness %.2f (material %.2f), rain %.2f"), Fx->GetWetness(), Wet, Rain));

		Fx->ClearDamage();
		FLinearColor Sphere;
		MID->GetVectorParameterValue(FHashedMaterialParameterInfo(FMaterialParameterInfo(TEXT("DamageHit_0_Sphere"))), Sphere);
		UMaterialInterface* Parent = MID->Parent;
		UNHCharacterEffectsMIDPool* Pool = PC->GetWorld()->GetSubsystem<UNHCharacterEffectsMIDPool>();
		const int32 FreeBefore = Pool->GetFreeCount(Parent);
		Fx->ReleaseEffects(true);
		Check(TEXT("a repair clears the marks, and releasing gives the material back to the pool and the mesh its own paint"),
			Fx->GetActiveHits().Num() == 0 && Sphere.A == 0.f && Pool->GetFreeCount(Parent) == FreeBefore + 1 && Mesh->GetMaterial(0) == Parent);
		PaintBody->Destroy();
		PaintBody = nullptr;
	});
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
		// what the vehicle was doing, when the step that ran out was a drive
		FString Driving;
		if (const ANHVehicle* V = Cast<ANHVehicle>(PC->GetPawn()))
		{
			Driving = FString::Printf(TEXT("; in the %s at %.0f, %.0f, %.0f doing %.0f cm/s, fuel %.0f%%%s%s, at stop '%s'"), *V->GetSpec().Name, V->GetActorLocation().X, V->GetActorLocation().Y,
				V->GetActorLocation().Z, V->Speed, V->Fuel * 100.f, V->IsHeld() ? TEXT(", held") : TEXT(""), V->IsWrecked() ? TEXT(", wrecked") : TEXT(""), D ? *D->Shift.AtStop.ToString() : TEXT("?"));
		}
		End(FString::Printf(TEXT("Step '%s' did not finish in %.0f s (stage %s, panel '%s', dialogue %s%s)"), *S.Name, S.Timeout, *StageName(),
			D && D->Panel.bOpen ? *D->Panel.Title : TEXT("none"), D && D->Dialogue.bOpen ? TEXT("open") : TEXT("closed"), *Driving));
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
	if (S->Agbero > 0 && Stop == FName(TEXT("balogate"))) // on the first day only Yaba's agbero asks
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

	// ---- character effects: the material pool and the weather it reads (no character meshes exist yet)
	Do(TEXT("character effects: material pool and weather"), [this]()
	{
		UNHCharacterEffectsMIDPool* Pool = PC->GetWorld()->GetSubsystem<UNHCharacterEffectsMIDPool>();
		UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NaijaHustle/Environment/Materials/M_NHSurface.M_NHSurface"));
		if (!Check(TEXT("the character effects pool exists in the world and has a material to pool"), Pool && Parent))
		{
			return;
		}
		Pool->PrewarmPool(Parent, 3);
		UMaterialInstanceDynamic* A = Pool->AcquireMID(Parent);
		const int32 FreeAfterAcquire = Pool->GetFreeCount(Parent);
		A->SetScalarParameterValue(TEXT("Grime"), 0.123f);
		Pool->ReleaseMID(A);
		UMaterialInstanceDynamic* B = Pool->AcquireMID(Parent);
		float Grime = 0.f, ParentGrime = 0.f;
		B->GetScalarParameterValue(FHashedMaterialParameterInfo(FMaterialParameterInfo(TEXT("Grime"))), Grime);
		Parent->GetScalarParameterValue(FHashedMaterialParameterInfo(FMaterialParameterInfo(TEXT("Grime"))), ParentGrime);
		Check(TEXT("pre-warming makes materials ahead of time, and a released one is handed out again with its overrides wiped"),
			FreeAfterAcquire == 2 && B == A && FMath::IsNearlyEqual(Grime, ParentGrime) && Pool->GetFreeCount(Parent) == 2,
			FString::Printf(TEXT("free %d, reused %d, grime %.3f vs %.3f"), FreeAfterAcquire, B == A ? 1 : 0, Grime, ParentGrime));
		Pool->ReleaseMID(B);

		const ANHLightingRig* Rig = ANHLightingRig::Find(PC);
		UMaterialParameterCollection* MPC = Rig ? Rig->Weather.LoadSynchronous() : nullptr;
		UMaterialParameterCollectionInstance* Weather = MPC ? PC->GetWorld()->GetParameterCollectionInstance(MPC) : nullptr;
		float Temp = -1.f, Hum = -1.f;
		const bool bRead = Weather && Weather->GetScalarParameterValue(TEXT("Temperature"), Temp) && Weather->GetScalarParameterValue(TEXT("Humidity"), Hum);
		const FNHLightingSettings Now = Rig ? Rig->SettingsFor(Rig->Preset) : FNHLightingSettings();
		Check(TEXT("the weather carries the lighting preset's temperature and humidity for sweat and drying"), bRead && FMath::IsNearlyEqual(Temp, Now.Temperature) && FMath::IsNearlyEqual(Hum, Now.Humidity),
			FString::Printf(TEXT("%.1f C, humidity %.2f"), Temp, Hum));
	});

	AddMomentumChecks();
	AddVehiclePaintChecks();
	AddVehicleDynamicsChecks();
}

// ---------------------------------------------------------------------------------------------------- the two leads
void UNHDebugPlay::Leads(bool bQuitWhenDone)
{
	Begin(TEXT("leads"), bQuitWhenDone);
	// what the script remembers between steps
	struct FSeen
	{
		TMap<FName, FVector> Left; // where each lead was last left
		int32 Cash = 0, Switches = 0;
		FName Expect;
		float RushT = 0.f;
		int32 Stars = 0;
		TWeakObjectPtr<ANHHackPoint> Camera;
	};
	const TSharedRef<FSeen> Seen = MakeShared<FSeen>();
	const auto L = [this]() { return ANHLeads::Get(PC); };
	// pictures with the HUD on, in Saved/Screenshots/NH/phase1/, and the frame rate over the whole run (Saved/Profiling/FPSChartStats)
	const auto Shot = [](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/NH/phase1") / (FString(Name) + TEXT(".png")), true, false); };
	Do(TEXT("start counting frames"), [this]() { PC->NHTime(9.5f); PC->ConsoleCommand(TEXT("startfpschart")); });

	// the save may be in the middle of somebody rich's life (ANHEstate): the test is of the two leads
	Do(TEXT("back from somebody rich, if the save is playing one"), [this, L]()
	{
		if (L() && Hustle() && !Hustle()->Persona.IsNone())
		{
			Note(FString::Printf(TEXT("the save is playing '%s'"), *Hustle()->Persona.ToString()));
			Check(TEXT("from somebody rich, the switch goes back to the lead last played"), L()->Switch(NAME_None, true), L()->Current().ToString());
		}
	});
	Until(TEXT("back as the lead"), [this, L](float) { return !L() || (!L()->IsSwitching() && Hustle()->Persona.IsNone()); }, 25.f);
	Do(TEXT("who is being played at the start"), [this, Seen, L]()
	{
		ANHLeads* Leads = L();
		if (!Check(TEXT("the level has the story's cast"), Leads && Leads->Members().Num() >= 2 && Hustle() && Character(), Leads ? FString::Printf(TEXT("%d in the cast"), Leads->Members().Num()) : TEXT("no ANHLeads")))
		{
			End(TEXT("nothing to test"));
			return;
		}
		Hustle()->ClearHeat();
		Seen->Cash = Hustle()->Cash;
		// away from Baba Driver: walking up to him starts the first job's talk, and nobody switches in the middle of a job
		if (const UNHGameData* Data = UNHGameData::Get(PC))
		{
			const FNHBusStop* Quiet = Data->Stops.Find(TEXT("second"));
			const FVector2D At = Quiet ? Quiet->Wait + (Quiet->Wait - Quiet->Kerb).GetSafeNormal() * 400.f : Data->Home + FVector2D(3000.f, 3000.f);
			PC->NHAt(At.X, At.Y);
			Note(FString::Printf(TEXT("standing at %.0f, %.0f, %.0f m from the motor park"), At.X, At.Y, FVector2D::Distance(At, Data->Park) / 100.f));
		}
		// Tunde's own place is beside the motor park: if landing there began Baba Driver's talk, walk away from it as if it had not
		if (ANHGameDirector* D = Dir(); D && D->Stage == ANHGameDirector::EStage::Talk)
		{
			D->Dialogue = ANHGameDirector::FDialogue();
			D->Stage = ANHGameDirector::EStage::Meet;
			Note(TEXT("Baba Driver's talk had begun; put back to before it"));
		}
		const ANHLeads::FMember* Who = Leads->Find(Leads->Current());
		Note(FString::Printf(TEXT("playing %s (%s), %s; body '%s'"), Who ? *Who->Name : TEXT("?"), *Leads->Current().ToString(), *NHPlay::N(Seen->Cash), *Character()->GetSkin().ToString()));
		Check(TEXT("Tunde and Amaka can both be played from the start, and nobody else of the cast"), Leads->IsUnlocked(TEXT("tunde")) && Leads->IsUnlocked(TEXT("amaka")) && !Leads->IsUnlocked(TEXT("chidi"))
			&& !Leads->IsUnlocked(TEXT("sule")) && !Leads->IsUnlocked(TEXT("zainab")) && !Leads->IsUnlocked(TEXT("jaguar")) && !Leads->IsUnlocked(TEXT("kemi")) && !Leads->IsUnlocked(TEXT("shina")));
	});

	for (int32 I = 0; I < 10; ++I)
	{
		Do(FString::Printf(TEXT("switch %d of 10 (Tab)"), I + 1), [this, Seen, L, I]()
		{
			ANHLeads* Leads = L();
			const FName From = Leads->Current();
			Seen->Expect = From == TEXT("tunde") ? FName(TEXT("amaka")) : FName(TEXT("tunde"));
			// a few steps off first, so "where they were left" is not just where they started
			if (ANHCharacter* C = Character())
			{
				C->SetActorLocation(C->GetActorLocation() + FVector(120.f * (I % 3), 90.f * (I % 2), 0.f), true);
				Seen->Left.Add(From, C->GetActorLocation());
			}
			Check(FString::Printf(TEXT("the switch from %s starts"), *From.ToString()), Leads->Switch() && Leads->IsSwitching());
		});
		Until(TEXT("the camera goes up, across and down"), [L](float) { return !L()->IsSwitching(); }, 20.f);
		Do(TEXT("after the switch"), [this, Seen, L]()
		{
			ANHLeads* Leads = L();
			ANHCharacter* C = Character();
			const FName Now = Leads->Current(), Other = Now == TEXT("tunde") ? FName(TEXT("amaka")) : FName(TEXT("tunde"));
			const ANHLeads::FMember* Who = Leads->Find(Now);
			++Seen->Switches;
			Check(FString::Printf(TEXT("the player is %s"), *Seen->Expect.ToString()), Now == Seen->Expect && C && PC->GetPawn() == C, Who ? Who->Name : FString());
			Check(TEXT("in that lead's own body"), C && Who && (C->GetSkin() == Who->Skin || !C->HasBody()), C ? C->GetSkin().ToString() : FString());
			if (const FVector* Was = Seen->Left.Find(Now))
			{
				const float Off = FVector::Dist2D(C->GetActorLocation(), *Was);
				Check(TEXT("standing where that lead was left"), Off < 150.f, FString::Printf(TEXT("%.0f cm from it"), Off));
			}
			else
			{
				Check(TEXT("the first time, at the place that lead starts from"), FVector::Dist2D(C->GetActorLocation(), Leads->SpotOf(Now)) < 150.f || Leads->WasPlaced(Now),
					FString::Printf(TEXT("at %.0f, %.0f"), C->GetActorLocation().X, C->GetActorLocation().Y));
			}
			Check(TEXT("on the ground, walking"), C && C->GetCharacterMovement()->MovementMode == MOVE_Walking && C->GetCharacterMovement()->IsMovingOnGround());
			Check(TEXT("the money is the same"), Hustle()->Cash == Seen->Cash, NHPlay::N(Hustle()->Cash));
			const FVector* LeftAt = Seen->Left.Find(Other);
			const float Away = LeftAt ? FVector::Dist2D(C->GetActorLocation(), *LeftAt) : 0.f;
			Note(FString::Printf(TEXT("%s was left %.0f m away"), *Other.ToString(), Away / 100.f));
		});
		if (I == 0)
		{
			Until(TEXT("a look at Amaka where she starts"), [this](float Dt) { Pace += Dt; return Pace > 1.5f; }, 5.f);
			Do(TEXT("picture: Amaka after the first switch"), [Shot]() { Shot(TEXT("1_amaka_after_switch")); });
		}
		Until(TEXT("the lead left behind is standing there, when near enough to see"), [this, Seen, L](float)
		{
			ANHLeads* Leads = L();
			const FName Other = Leads->Current() == TEXT("tunde") ? FName(TEXT("amaka")) : FName(TEXT("tunde"));
			const FVector* LeftAt = Seen->Left.Find(Other);
			if (!LeftAt || FVector::Dist2D(Character()->GetActorLocation(), *LeftAt) > 14000.f)
			{
				return true; // too far for a body: nothing to see
			}
			const ANHCharacter* Body = Leads->StandIn(Other);
			return Body && FVector::Dist2D(Body->GetActorLocation(), *LeftAt) < 150.f;
		}, 6.f);
	}

	// ---- Tunde: Hustle Rush
	Do(TEXT("be Tunde"), [L]() { if (L()->Current() != TEXT("tunde")) { L()->Switch(TEXT("tunde"), true); } });
	Until(TEXT("Tunde"), [L](float) { return !L()->IsSwitching() && L()->Current() == TEXT("tunde"); }, 20.f);
	Do(TEXT("Hustle Rush with the meter empty, then full (Z)"), [this, Seen, L, Shot]()
	{
		ANHLeads* Leads = L();
		Leads->SetMeter(TEXT("tunde"), 0.2f);
		Check(TEXT("it does nothing until the meter is full"), !Leads->UseAbility() && !Leads->RushOn(), FString::Printf(TEXT("meter %.0f%%"), Leads->Meter(TEXT("tunde")) * 100.f));
		Leads->SetMeter(TEXT("tunde"), 1.f);
		const float Before = Character()->GetCharacterMovement()->MaxWalkSpeed;
		Check(TEXT("full, it starts"), Leads->UseAbility() && Leads->RushOn(), FString::Printf(TEXT("%.1f s"), Leads->RushSecondsLeft()));
		Check(TEXT("he is faster and takes less of a blow, and the meter is spent"), Character()->SpeedBoost > 1.f && Character()->DamageTaken < 1.f && Character()->GetCharacterMovement()->MaxWalkSpeed > Before
			&& Leads->Meter(TEXT("tunde")) < 0.05f, FString::Printf(TEXT("speed x%.2f (%.0f -> %.0f cm/s), blows x%.2f"), Character()->SpeedBoost, Before, Character()->GetCharacterMovement()->MaxWalkSpeed, Character()->DamageTaken));
		const float Health = Character()->Health;
		Character()->Hurt(20.f);
		Check(TEXT("a blow of 20 takes less than 20 off him"), Health - Character()->Health < 19.f && Health - Character()->Health > 0.f, FString::Printf(TEXT("%.0f"), Health - Character()->Health));
		Seen->RushT = 0.f;
		Shot(TEXT("2_tunde_hustle_rush"));
	});
	Until(TEXT("Hustle Rush runs out"), [L](float) { return !L()->RushOn(); }, 30.f);
	Do(TEXT("after Hustle Rush"), [this, L]()
	{
		Check(TEXT("he is back to how he was"), FMath::IsNearlyEqual(Character()->SpeedBoost, 1.f) && FMath::IsNearlyEqual(Character()->DamageTaken, 1.f));
	});

	// ---- Amaka: Unlock
	Do(TEXT("be Amaka"), [L]() { L()->Switch(TEXT("amaka"), true); });
	Until(TEXT("Amaka"), [L](float) { return !L()->IsSwitching() && L()->Current() == TEXT("amaka"); }, 20.f);
	Do(TEXT("a camera nine metres in front of her, and two wanted stars"), [this, Seen, L]()
	{
		Seen->Camera = L()->PlaceHackPoint(ENHHackKind::Camera, 900.f);
		Hustle()->AddHeat(2.f);
		Seen->Stars = Hustle()->Stars();
		L()->SetMeter(TEXT("amaka"), 1.f);
		Check(TEXT("the camera stands there"), Seen->Camera.IsValid(), FString::Printf(TEXT("%d stars"), Seen->Stars));
	});
	Until(TEXT("the marker finds something to unlock"), [L](float) { return L()->Hud().bTarget; }, 5.f);
	Do(TEXT("picture: the marker on what Unlock would get into"), [Shot]() { Shot(TEXT("3_amaka_unlock_marker")); });
	Until(TEXT("the picture is saved"), [this](float Dt) { Pace += Dt; return Pace > 0.6f; }, 5.f);
	Do(TEXT("Unlock (Z)"), [this, Seen, L]()
	{
		ANHLeads* Leads = L();
		Note(FString::Printf(TEXT("marked: %s"), *Leads->Hud().TargetLabel));
		Check(TEXT("it unlocks what is marked"), Leads->UseAbility() && !Leads->LastUnlocked.IsNone(), Leads->LastUnlocked.ToString());
		Check(TEXT("the meter is spent"), Leads->Meter(TEXT("amaka")) < 0.05f);
		if (Leads->LastUnlocked == TEXT("camera"))
		{
			Check(TEXT("the camera is down and a star is gone"), Seen->Camera.IsValid() && Seen->Camera->IsHacked() && Leads->CamerasDown() && Hustle()->Stars() < Seen->Stars, FString::Printf(TEXT("%d -> %d stars"), Seen->Stars, Hustle()->Stars()));
		}
		Hustle()->ClearHeat();
		Note(FString::Printf(TEXT("%d switches made; %s"), Seen->Switches, *NHPlay::N(Hustle()->Cash)));
	});
	Until(TEXT("the last picture is saved"), [this](float Dt) { Pace += Dt; return Pace > 1.f; }, 5.f);
	Do(TEXT("stop counting frames"), [this]() { PC->ConsoleCommand(TEXT("stopfpschart")); });
}

// ---------------------------------------------------------------------------------------------------- the mission runner
void UNHDebugPlay::Systems(bool bQuitWhenDone)
{
	Begin(TEXT("systems"), bQuitWhenDone);
	struct FSeen
	{
		int32 Cash = 0, Integrity = 0, Restarts = 0;
		float Clock = 0.f;
		FVector At = FVector::ZeroVector;
		bool bAlerted = false;
		float Punch = 0.f;
	};
	const TSharedRef<FSeen> Seen = MakeShared<FSeen>();
	const auto M = [this]() { return ANHMissions::Get(PC); };
	const auto L = [this]() { return ANHLeads::Get(PC); };
	const auto Shot = [](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/NH/phase2") / (FString(Name) + TEXT(".png")), true, false); };
	// walking, not running: a run is a noise, and gives a disguise away
	const auto Walk = [this](const FVector& To, float Reach)
	{
		ANHCharacter* C = Cast<ANHCharacter>(PC->GetPawn());
		if (!C)
		{
			return false;
		}
		C->SetSprinting(false);
		const FVector Way = (To - C->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
		if (Way.Size() < Reach)
		{
			return true;
		}
		C->AddMovementInput(Way.GetSafeNormal(), 1.f);
		return false;
	};
	const auto Objective = [this, M](int32 Index, const TCHAR* Type, float Timeout)
	{
		Until(FString::Printf(TEXT("objective %d begins (%s)"), Index + 1, Type), [M, Index, Type](float) { return M()->IsActive() && M()->ObjectiveIndex() == Index && M()->ObjectiveType() == Type; }, Timeout);
	};
	const auto PanelIs = [this](const TCHAR* Starts) { return Dir()->Panel.bOpen && Dir()->Panel.Title.StartsWith(Starts); };

	Do(TEXT("back from somebody rich, if the save is playing one"), [this, L]() { if (L() && Hustle() && !Hustle()->Persona.IsNone()) { L()->Switch(NAME_None, true); } });
	Until(TEXT("back as the lead"), [this, L](float) { return !L() || (!L()->IsSwitching() && Hustle()->Persona.IsNone()); }, 25.f);
	Do(TEXT("be Tunde, on the road by the Anthony stop"), [this, L]() { if (L() && L()->Current() != TEXT("tunde")) { L()->Switch(TEXT("tunde"), true); } });
	Until(TEXT("Tunde"), [L](float) { return !L() || !L()->IsSwitching(); }, 25.f);
	Do(TEXT("the start"), [this, Seen, M]()
	{
		const UNHGameData* Data = UNHGameData::Get(PC);
		const FNHBusStop* Stop = Data ? Data->Stops.Find(TEXT("second")) : nullptr;
		if (!Check(TEXT("the level has the mission runner, both test missions and the Anthony stop"), M() && Stop && M()->Known().Contains(TEXT("m00_systems")) && M()->Known().Contains(TEXT("m00_night")),
			M() ? FString::Printf(TEXT("%d missions"), M()->Known().Num()) : TEXT("no ANHMissions")))
		{
			End(TEXT("nothing to test"));
			return;
		}
		const FVector2D Back = (Stop->Wait - Stop->Kerb).GetSafeNormal(), Along(-Back.Y, Back.X);
		const FVector2D At = Stop->Wait - Back * 900.f;
		PC->NHAt(At.X, At.Y);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X));
		PC->GetPawn()->SetActorRotation(FRotator(0.f, Yaw, 0.f));
		PC->SetControlRotation(FRotator(-10.f, Yaw, 0.f));
		if (ANHGameDirector* D = Dir(); D && D->Stage == ANHGameDirector::EStage::Talk)
		{
			D->Dialogue = ANHGameDirector::FDialogue();
			D->Stage = ANHGameDirector::EStage::Meet;
		}
		Hustle()->ClearHeat();
		PC->NHTime(9.5f); // by day, so the pictures show something (nothing is saved: the run is made with -NHNoSave)
		PC->ConsoleCommand(TEXT("startfpschart"));
		// the night shift's pay, against the table in docs/STORY.md
		int64 Total = 0;
		for (int32 N = 2; N <= 12; ++N)
		{
			Total += M()->PayFor(N);
		}
		Check(TEXT("the night shift pays nothing extra for mission 1, N1,000,000 for mission 2 and N3,547,796 for mission 12"), M()->PayFor(1) == 0 && M()->PayFor(2) == 1000000 && M()->PayFor(12) == 3547796,
			FString::Printf(TEXT("%d, %d, %d"), M()->PayFor(1), M()->PayFor(2), M()->PayFor(12)));
		Check(TEXT("missions 2 to 12 pay N22,420,357 between them"), Total == 22420357, FString::Printf(TEXT("%lld"), Total));
		Seen->Cash = Hustle()->Cash;
		Seen->Integrity = Hustle()->Integrity;
	});

	// ---- the job card and the talk
	Do(TEXT("start the test mission"), [this, M, PanelIs]()
	{
		Check(TEXT("it starts with a job card"), M()->Start(TEXT("m00_systems")) && M()->IsActive() && PanelIs(TEXT("SYSTEMS CHECK")), Dir()->Panel.Title);
		Check(TEXT("a second mission cannot start while one runs"), !M()->Start(TEXT("m00_night")));
		PC->Choose(0);
	});
	Objective(0, TEXT("talk"), 25.f);
	Do(TEXT("the brief: two lines, E for each"), [this, L]()
	{
		Check(TEXT("the scene's lines are on the screen"), Dir()->Dialogue.bOpen && Dir()->Dialogue.Lines.Num() == 2, Dir()->Dialogue.Speaker);
		Check(TEXT("switching lead is locked for the length of the job"), L()->IsLocked() && !L()->Switch());
		PC->OnAction();
		PC->OnAction();
	});

	// ---- goto, disguise, sneak
	Objective(1, TEXT("goto"), 10.f);
	Until(TEXT("walk to the marker"), [M, Walk](float) { FVector To; return !M()->Where(To) || M()->ObjectiveIndex() != 1 || Walk(To, 100.f); }, 30.f);
	Objective(2, TEXT("disguise"), 10.f);
	Until(TEXT("walk into the bundle"), [M, Walk](float) { FVector To; return M()->ObjectiveIndex() != 2 || (M()->Where(To) && Walk(To, 60.f)); }, 30.f);
	Objective(3, TEXT("sneak"), 10.f);
	Do(TEXT("dressed as a waiter, with a doorman ahead"), [this, M, Shot]()
	{
		Check(TEXT("the disguise is on"), M()->Disguise == TEXT("waiter"), M()->Disguise.ToString());
		Check(TEXT("the doorman stands at his post"), M()->Guards().Num() == 1 && M()->Guards()[0] && M()->Guards()[0]->GetState() == ENHGuardState::Patrol);
	});
	Until(TEXT("walk past the doorman"), [this, M, Walk, Seen, Shot](float)
	{
		if (M()->ObjectiveIndex() != 3)
		{
			return true;
		}
		const ANHGuard* Doorman = M()->Guards().Num() ? M()->Guards()[0].Get() : nullptr;
		if (Doorman && !Seen->bAlerted && FVector::Dist2D(PC->GetPawn()->GetActorLocation(), Doorman->GetActorLocation()) < 620.f)
		{
			Seen->bAlerted = true; // used here as "the picture has been taken"
			Shot(TEXT("1_waiter_past_the_doorman"));
		}
		FVector To;
		return (M()->Where(To) && Walk(To, 100.f)) || Dir()->Panel.bOpen;
	}, 40.f);
	Do(TEXT("past him"), [this, M, PanelIs]()
	{
		Check(TEXT("he took the waiter at face value: no failure card, no restart"), !PanelIs(TEXT("JOB FAILED")) && M()->Restarts() == 0);
	});

	// ---- a checkpoint, a noise, a takedown
	Objective(4, TEXT("takedown"), 10.f);
	Do(TEXT("fail on purpose, to see the checkpoint"), [this, M, Seen, PanelIs]()
	{
		Seen->Clock = M()->Elapsed();
		Seen->At = PC->GetPawn()->GetActorLocation();
		PC->GetPawn()->SetActorLocation(Seen->At + FVector(0.f, 300.f, 0.f));
		M()->Fail(TEXT("A test of the checkpoint."));
		Check(TEXT("a failure brings up the card with the checkpoint on it"), PanelIs(TEXT("JOB FAILED")) && Dir()->Panel.Options.Num() == 2, Dir()->Panel.Lines.Num() > 1 ? Dir()->Panel.Lines[1] : FString());
		PC->Choose(0);
	});
	Until(TEXT("back at the checkpoint"), [M](float) { return M()->IsActive() && M()->ObjectiveIndex() == 4 && M()->Guards().Num() == 1; }, 15.f);
	Do(TEXT("after the restart"), [this, M, Seen]()
	{
		Check(TEXT("it is the same objective again, with one restart counted"), M()->ObjectiveType() == TEXT("takedown") && M()->Restarts() == 1);
		Check(TEXT("the player is back where the checkpoint was"), FVector::Dist2D(PC->GetPawn()->GetActorLocation(), Seen->At) < 60.f, FString::Printf(TEXT("%.0f cm off"), FVector::Dist2D(PC->GetPawn()->GetActorLocation(), Seen->At)));
		Check(TEXT("the mission's clock kept running"), M()->Elapsed() >= Seen->Clock, FString::Printf(TEXT("%.1f s then, %.1f s now"), Seen->Clock, M()->Elapsed()));
		Check(TEXT("the disguise put on before the checkpoint is still on"), M()->Disguise == TEXT("waiter"));
		ANHGuard* Lookout = M()->Guards()[0];
		Check(TEXT("in front of the lookout, a takedown is not on offer"), !Lookout->CanBeTakenDown(Lookout->GetActorLocation() + Lookout->GetActorForwardVector() * 150.f));
		// a noise to his side, with the player well out of his sight while he looks about (he turns right round)
		PC->GetPawn()->SetActorLocation(Seen->At - Lookout->GetActorForwardVector() * 3000.f);
		Lookout->Hear(Lookout->GetActorLocation() + Lookout->GetActorRightVector() * 450.f, 900.f);
		Check(TEXT("a noise nearby sends him to look"), Lookout->GetState() == ENHGuardState::Investigate && Lookout->Investigated == 1);
	});
	Until(TEXT("he looks about and goes back to his post"), [M](float) { const ANHGuard* G = M()->Guards()[0]; return G->GetState() == ENHGuardState::Patrol && !G->IsWalking(); }, 25.f);
	Do(TEXT("back to the checkpoint's spot, behind him"), [this, Seen]() { PC->GetPawn()->SetActorLocation(Seen->At); });
	Until(TEXT("walk up behind him"), [this, M, Walk](float)
	{
		const ANHGuard* G = M()->Guards().Num() ? M()->Guards()[0].Get() : nullptr;
		return !G || G->IsDown() || G->CanBeTakenDown(PC->GetPawn()->GetActorLocation()) || Walk(G->GetActorLocation(), 120.f) || G->IsAlert();
	}, 30.f);
	Do(TEXT("take him down (E)"), [this, M]()
	{
		ANHGuard* G = M()->Guards()[0];
		Check(TEXT("behind him and unseen, the prompt offers it"), !G->IsAlert() && M()->ActionPrompt(PC->GetPawn()).Contains(TEXT("Take down")), FString::Printf(TEXT("awareness %.0f%%"), G->Awareness() * 100.f));
		PC->OnAction();
		Check(TEXT("he is down"), G->IsDown());
	});

	// ---- a fight
	Objective(5, TEXT("defeat"), 10.f);
	Do(TEXT("the collector"), [Seen]() { Seen->bAlerted = false; Seen->Punch = 0.f; });
	Until(TEXT("fight the collector with bare hands"), [this, M, Seen, Shot](float Dt)
	{
		if (M()->ObjectiveIndex() != 5 || M()->Guards().Num() == 0)
		{
			return true;
		}
		ANHGuard* G = M()->Guards()[0];
		ANHCharacter* C = Cast<ANHCharacter>(PC->GetPawn());
		if (!G || !C || G->IsDown())
		{
			return false; // the runner moves on by itself
		}
		if (G->IsAlert() && !Seen->bAlerted)
		{
			Seen->bAlerted = true;
			Shot(TEXT("2_the_collector_gives_chase"));
		}
		const FVector Way = (G->GetActorLocation() - C->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
		C->Health = FMath::Max(C->Health, 40.f); // the script is no boxer: the fight is to see blows land both ways, not who wins
		if (Way.Size() > 95.f)
		{
			C->AddMovementInput(Way.GetSafeNormal(), 1.f);
		}
		C->SetActorRotation(FRotator(0.f, Way.Rotation().Yaw, 0.f));
		// T pressed, and let go a few frames later: a fist reaches 1.35 m
		Seen->Punch -= Dt;
		if (Seen->Punch <= 0.f && Way.Size() < 130.f)
		{
			Seen->Punch = 0.5f;
			C->SetTrigger(true);
		}
		else if (Seen->Punch < 0.35f)
		{
			C->SetTrigger(false);
		}
		return false;
	}, 60.f);
	Do(TEXT("after the fight"), [this, Seen]()
	{
		Check(TEXT("facing the player, the collector saw him and came for him"), Seen->bAlerted);
		if (ANHCharacter* C = Cast<ANHCharacter>(PC->GetPawn()))
		{
			C->Health = 100.f;
		}
	});

	// ---- a choice and the plan
	Objective(6, TEXT("choose"), 10.f);
	Do(TEXT("the bag: leave it (1)"), [this, PanelIs]()
	{
		Check(TEXT("the choice card is up with two options"), PanelIs(TEXT("THE BAG")) && Dir()->Panel.Options.Num() == 2);
		PC->Choose(0);
	});
	Objective(7, TEXT("plan"), 10.f);
	Do(TEXT("the plan, step one: Chidi drives (2)"), [this, Seen, PanelIs]()
	{
		Check(TEXT("leaving the bag raised Integrity by 5 and set the flag"), Hustle()->Integrity == FMath::Min(Seen->Integrity + 5, 100) && Hustle()->Flag(TEXT("test_bag")) == 1, FString::Printf(TEXT("%d -> %d"), Seen->Integrity, Hustle()->Integrity));
		Check(TEXT("the heist board's first card"), PanelIs(TEXT("THE PLAN  1 / 2")), Dir()->Panel.Title);
		PC->Choose(1);
	});
	Do(TEXT("the plan, step two: quiet (1)"), [this, PanelIs]()
	{
		Check(TEXT("the heist board's second card"), PanelIs(TEXT("THE PLAN  2 / 2")), Dir()->Panel.Title);
		PC->Choose(0);
	});

	// ---- over to Amaka: Unlock, things to pick up, a star to lose, a wait
	Objective(9, TEXT("unlock"), 30.f);
	Do(TEXT("as Amaka, with the plan made"), [this, L]()
	{
		Check(TEXT("the plan is in the flags"), Hustle()->Flag(TEXT("plan_driver")) == 2 && Hustle()->Flag(TEXT("plan_quiet")) == 1);
		Check(TEXT("the mission put the player in Amaka's shoes"), L()->Current() == TEXT("amaka"));
	});
	Until(TEXT("the marker is on the mission's camera"), [L](float) { return L()->Hud().bTarget; }, 8.f);
	Do(TEXT("Unlock (Z)"), [this, L, M]() { Check(TEXT("Unlock works on the mission's camera"), L()->UseAbility() && M()->UnlockTarget() == nullptr || L()->LastUnlocked == TEXT("camera"), L()->LastUnlocked.ToString()); });
	Objective(10, TEXT("collect"), 10.f);
	Until(TEXT("pick up the three phones"), [this, M, Walk](float)
	{
		if (M()->ObjectiveIndex() != 10)
		{
			return true;
		}
		Hustle()->ClearHeat(); // the fight put stars on; the next objective is to be about its own star
		for (AActor* Thing : M()->Things())
		{
			if (IsValid(Thing))
			{
				Walk(Thing->GetActorLocation(), 40.f);
				break;
			}
		}
		return false;
	}, 40.f);
	Objective(11, TEXT("loseheat"), 10.f);
	Do(TEXT("a star to lose"), [this]() { Check(TEXT("the objective put one star on"), Hustle()->Stars() == 1, FString::Printf(TEXT("%d"), Hustle()->Stars())); });
	Objective(12, TEXT("wait"), 60.f);
	Objective(13, TEXT("enter"), 15.f);

	// ---- the speedboat
	Do(TEXT("to the lagoon, and into the boat (F)"), [this, M]()
	{
		ANHVehicle* Boat = M()->MissionVehicle();
		if (Check(TEXT("a speedboat is waiting on the water"), Boat && Boat->GetSpec().bBoat && Boat->Afloat(Boat->GetActorLocation()), Boat ? Boat->GetSpec().Name : FString()))
		{
			Check(TEXT("it would not float on the road"), !Boat->Afloat(FVector(11400.f, 5650.f, Boat->GetActorLocation().Z)));
			PC->GetPawn()->SetActorLocation(Boat->GetActorLocation() + FVector(0.f, 0.f, 150.f));
			Check(TEXT("the player gets in"), PC->EnterVehicle(Boat) && PC->GetPawn() == Boat);
		}
	});
	Objective(14, TEXT("goto"), 10.f);
	Until(TEXT("across the lagoon"), [this, M, Seen, Shot](float)
	{
		ANHVehicle* Boat = Cast<ANHVehicle>(PC->GetPawn());
		FVector To;
		if (!Boat || !M()->Where(To) || M()->ObjectiveIndex() != 14)
		{
			return true;
		}
		if (FMath::Abs(Boat->Speed) > 300.f && Seen->Punch < 100.f)
		{
			Seen->Punch = 1000.f; // once
			Shot(TEXT("3_speedboat_on_the_lagoon"));
		}
		Drive(Boat, FVector2D(To), 900.f, true);
		return false;
	}, 60.f);

	// ---- the end: the reward card, the night shift skipped
	Until(TEXT("the reward card"), [PanelIs](float) { return PanelIs(TEXT("JOB DONE")); }, 20.f);
	Do(TEXT("job done"), [this, M, Seen]()
	{
		const ANHMissions::FResult& R = M()->Last;
		FString Each;
		for (const float T : R.ObjectiveSeconds)
		{
			Each += FString::Printf(TEXT("%.0f "), T);
		}
		Note(FString::Printf(TEXT("the mission took %.0f s; objectives: %s"), R.Seconds, *Each));
		Check(TEXT("it is recorded as done, timed objective by objective, with its cred and the flag from the last objective"), R.bDone && R.ObjectiveSeconds.Num() == 15 && R.Cred == 5 && R.Restarts == 1
			&& Hustle()->IsDone(TEXT("m00_systems")) && Hustle()->Flag(TEXT("test_done")) == 1, FString::Printf(TEXT("%d objectives timed"), R.ObjectiveSeconds.Num()));
		Check(TEXT("the test mission is inside the length rule's eight minutes"), !R.bTooLong && R.Seconds < 480.f, FString::Printf(TEXT("%.0f s"), R.Seconds));
		PC->Choose(0);
	});
	Do(TEXT("the night shift: skip to the takings (2)"), [this, PanelIs]()
	{
		Check(TEXT("the night shift's card offers the drive or the takings"), PanelIs(TEXT("NIGHT SHIFT")) && Dir()->Panel.Options.Num() == 2);
		PC->Choose(1);
	});
	Do(TEXT("the takings"), [this, M, Seen, PanelIs, L]()
	{
		Check(TEXT("paid as mission 2: N1,000,000, on top of what was there"), PanelIs(TEXT("NIGHT SHIFT TAKINGS")) && M()->Last.Pay == 1000000 && Hustle()->Cash == Seen->Cash + 1000000, NHPlay::N(Hustle()->Cash));
		PC->Choose(0);
		Check(TEXT("the mission is over and switching is free again"), !M()->IsActive() && !L()->IsLocked());
		Seen->Cash = Hustle()->Cash;
	});

	// ---- the second test mission: the night shift driven
	Do(TEXT("out of the boat and back to the road"), [this]()
	{
		PC->LeaveVehicle(true);
		PC->NHAt(11400.f, 6950.f); // on the pavement: Amaka is left standing here, and the night bus comes down the road
	});
	Do(TEXT("start the night-shift mission"), [this, M]()
	{
		Check(TEXT("it starts"), M()->Start(TEXT("m00_night")));
		PC->Choose(0);
	});
	Objective(0, TEXT("talk"), 30.f);
	Do(TEXT("his one line"), [this, L]()
	{
		Check(TEXT("the job card put the player back in Tunde's shoes"), L()->Current() == TEXT("tunde"));
		PC->OnAction();
	});
	Until(TEXT("the reward card"), [PanelIs](float) { return PanelIs(TEXT("JOB DONE")); }, 15.f);
	Do(TEXT("on to the night shift, driven (1)"), [this]()
	{
		// facing back up the road he walked down: the bus is brought round in front of him, with the road clear ahead of it
		PC->GetPawn()->SetActorRotation(FRotator(0.f, 0.f, 0.f));
		PC->Choose(0);
		PC->Choose(0);
	});
	Until(TEXT("the night bus is brought round"), [M](float) { return M()->MissionVehicle() != nullptr; }, 25.f);
	Do(TEXT("into the night bus (F)"), [this, M]()
	{
		ANHVehicle* Bus = M()->MissionVehicle();
		Check(TEXT("it is a danfo, and Tunde gets in"), Bus && Bus->VehicleType == TEXT("danfo") && PC->EnterVehicle(Bus));
	});
	Until(TEXT("drive the night route"), [this, PanelIs](float)
	{
		ANHVehicle* Bus = Cast<ANHVehicle>(PC->GetPawn());
		if (!Bus || PanelIs(TEXT("NIGHT SHIFT TAKINGS")))
		{
			return true;
		}
		Drive(Bus, FVector2D(Bus->GetActorLocation() + Bus->GetActorForwardVector() * 5000.f), 800.f, false);
		return false;
	}, 60.f);
	Do(TEXT("the takings, driven"), [this, M, Seen, PanelIs]()
	{
		if (ANHVehicle* Bus = Cast<ANHVehicle>(PC->GetPawn()))
		{
			Bus->SetDriveInput(0.f, 0.f, 0.f);
		}
		Check(TEXT("paid as mission 3: N1,135,000"), PanelIs(TEXT("NIGHT SHIFT TAKINGS")) && M()->Last.Pay == 1135000 && Hustle()->Cash == Seen->Cash + 1135000, NHPlay::N(M()->Last.Pay));
		PC->Choose(0);
		Check(TEXT("over"), !M()->IsActive());
	});

	// ---- knocked down with no mission running: the clinic
	Do(TEXT("knocked down in the street"), [this, Seen]()
	{
		PC->LeaveVehicle(true);
		Seen->Cash = Hustle()->Cash;
		Hustle()->AddHeat(2.f); // somebody has to have done it: with no stars nobody is coming, and nothing looks at who is down
		if (ANHCharacter* C = Cast<ANHCharacter>(PC->GetPawn()))
		{
			C->Health = 0.f;
		}
	});
	Until(TEXT("the screen goes dark and comes back at the clinic"), [this](float) { const ANHCharacter* C = Cast<ANHCharacter>(PC->GetPawn()); return C && C->Health > 50.f && !PC->IsTravelling(); }, 25.f);
	Do(TEXT("at the clinic"), [this, Seen]()
	{
		const UNHGameData* Data = UNHGameData::Get(PC);
		const FNHBusStop* Stop = Data->Stops.Find(Data->ClinicStop);
		const int32 Gone = Seen->Cash - Hustle()->Cash;
		Check(TEXT("a tenth of the cash is gone, and the stars with it"), Gone == FMath::Max(2000, Seen->Cash / 10) && Hustle()->Stars() == 0, NHPlay::N(Gone));
		Check(TEXT("the player is standing beside the clinic's bus stop"), Stop && FVector2D::Distance(FVector2D(PC->GetPawn()->GetActorLocation()), Stop->Wait) < 700.f
			&& Cast<ANHCharacter>(PC->GetPawn())->GetCharacterMovement()->MovementMode == MOVE_Walking, Data->ClinicName);
	});
	Until(TEXT("the last picture is saved"), [this](float Dt) { Pace += Dt; return Pace > 1.f; }, 5.f);
	Do(TEXT("stop counting frames"), [this]() { PC->ConsoleCommand(TEXT("stopfpschart")); });
}
