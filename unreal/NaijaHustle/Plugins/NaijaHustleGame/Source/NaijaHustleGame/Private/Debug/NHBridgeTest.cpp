#include "Debug/NHBridgeTest.h"

#include "Core/NHGameData.h"
#include "Engine/World.h"
#include "Gameplay/NHGameDirector.h"
#include "Misc/CommandLine.h"
#include "NaijaHustleGame.h"
#include "Player/NHCharacter.h"
#include "Player/NHPlayerController.h"
#include "Vehicles/NHTraffic.h"
#include "Vehicles/NHVehicle.h"

ANHBridgeTest::ANHBridgeTest()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ANHBridgeTest::BeginPlay()
{
	Super::BeginPlay();
	FString List = TEXT("danfo,sedan,keke");
	FParse::Value(FCommandLine::Get(), TEXT("NHBridgeTypes="), List, false);
	TArray<FString> Names;
	List.ParseIntoArray(Names, TEXT(","));
	for (const FString& Name : Names)
	{
		Types.Add(FName(*Name));
	}
}

bool ANHBridgeTest::FindBridge(int32 Skip)
{
	const UNHGameData* Data = UNHGameData::Get(this);
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!Data || !Data->bRealCity || !PC || !PC->GetPawn())
	{
		return false;
	}
	// bridges by distance from the player; the first that can be driven over one way and back the other
	const FVector2D Here(PC->GetPawn()->GetActorLocation());
	TArray<int32> Ways;
	for (int32 I = 0; I < Data->RoadWays.Num(); ++I)
	{
		const FNHRoadWay& W = Data->RoadWays[I];
		FString Only; // -NHBridgeName="Third Mainland": only bridges whose name has that in it
		FParse::Value(FCommandLine::Get(), TEXT("NHBridgeName="), Only, false);
		if (!Only.IsEmpty() && !W.Name.Contains(Only))
		{
			continue;
		}
		if (W.bBridge && W.Class <= 4 && FVector2D::Distance(Data->RoadNodes[W.Nodes[0]], Data->RoadNodes[W.Nodes.Last()]) > 12000.f)
		{
			Ways.Add(I);
		}
	}
	Ways.Sort([&](int32 A, int32 B) { return FVector2D::DistSquared(Data->RoadNodes[Data->RoadWays[A].Nodes[0]], Here) < FVector2D::DistSquared(Data->RoadNodes[Data->RoadWays[B].Nodes[0]], Here); });
	for (const int32 WayIndex : Ways)
	{
		const FNHRoadWay& W = Data->RoadWays[WayIndex];
		const FVector2D A = Data->RoadNodes[W.Nodes[0]], B = Data->RoadNodes[W.Nodes.Last()], Mid = Data->RoadNodes[W.Nodes[W.Nodes.Num() / 2]];
		const FVector2D Dir = (B - A).GetSafeNormal();
		const auto Crosses = [&Mid](const TArray<FVector2D>& L)
		{
			for (const FVector2D& P : L)
			{
				if (FVector2D::Distance(P, Mid) < 3000.f)
				{
					return true;
				}
			}
			return false;
		};
		TArray<FVector2D> There;
		if (Data->RoadRoute(A - Dir * 20000.f, B + Dir * 20000.f, There) && Crosses(There) && Skip-- <= 0)
		{
			// Back the way it came, over the same bridge. On a one-way flyover that is against the traffic, which the
			// test switches off: what is being tested is the ramps in both directions, not the highway code.
			Out = There;
			Back.Reset();
			for (int32 I = There.Num() - 1; I >= 0; --I)
			{
				Back.Add(There[I]);
			}
			BridgeName = W.Name.IsEmpty() ? TEXT("an unnamed bridge") : W.Name;
			float Length = 0.f;
			for (int32 I = 1; I < Out.Num(); ++I)
			{
				Length += FVector2D::Distance(Out[I - 1], Out[I]);
			}
			UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: [bridgetest] bridge: %s, %.0f m from the start; the drive is %.0f m over and the same back"), *BridgeName, FVector2D::Distance(A, Here) / 100.f, Length / 100.f);
			return true;
		}
	}
	return false;
}

void ANHBridgeTest::NextLeg()
{
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	ANHGameDirector* Dir = ANHGameDirector::Get(this);
	if (Leg == 1 && Car.IsValid())
	{
		// the way back, in the same vehicle, from where it is
	}
	else
	{
		if (Car.IsValid())
		{
			PC->LeaveVehicle(true);
			Car->Destroy();
		}
		if (++TypeIndex >= Types.Num())
		{
			UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: [bridgetest] RESULT: %s. %d legs passed, %d failed (%s; steepest slope met %.1f%%)"), Failed ? TEXT("FAILED") : TEXT("COMPLETED CLEANLY"), Passed, Failed, *BridgeName, SteepestSeen);
			PC->ConsoleCommand(TEXT("quit"));
			SetActorTickEnabled(false);
			return;
		}
		Leg = 0;
		const FVector2D Facing = (Out[1] - Out[0]).GetSafeNormal();
		ANHVehicle* V = Dir ? Dir->SpawnVehicle(Types[TypeIndex], Out[0] + FVector2D(-Facing.Y, Facing.X) * 250.f, FMath::RadiansToDegrees(FMath::Atan2(Facing.Y, Facing.X)), FLinearColor(0.8f, 0.6f, 0.1f), TEXT("TEST")) : nullptr;
		if (!V || !Cast<ANHCharacter>(PC->GetPawn()))
		{
			EndLeg(false, TEXT("no vehicle of that type could be made"));
			return;
		}
		PC->GetPawn()->SetActorLocation(V->ExitPoint(), false, nullptr, ETeleportType::TeleportPhysics);
		PC->EnterVehicle(V);
		Car = V;
	}
	Along = 0.f;
	LegTime = StuckTime = Top = 0.f;
	Slowest = 1e6f;
	LastAt = Car->GetActorLocation();
}

void ANHBridgeTest::EndLeg(bool bPass, const FString& Why)
{
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: [bridgetest] off the ground: at most %.0f cm from its middle to what is under it (about 100 standing), %.0f m along; %.1f s in the air"), HighestOff, HighestOffAlong / 100.f, AirTime);
	if (PutBack > 0)
	{
		UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: [bridgetest] the driver was out of the vehicle and put back %d times"), PutBack);
	}
	HighestOff = HighestOffAlong = AirTime = 0.f;
	PutBack = 0;
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: [bridgetest] %s  %s %s: %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), TypeIndex < Types.Num() ? *Types[TypeIndex].ToString() : TEXT("?"), Leg == 0 ? TEXT("over") : TEXT("back"), *Why);
	(bPass ? Passed : Failed) += 1;
	if (Leg == 0 && bPass)
	{
		Leg = 1;
	}
	else
	{
		Leg = 2; // next vehicle
	}
	NextLeg();
}

void ANHBridgeTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!PC || !PC->GetPawn())
	{
		return;
	}
	if (!bStarted)
	{
		// the level settles and the director makes its own vehicles first
		if ((StartDelay -= DeltaSeconds) > 0.f)
		{
			return;
		}
		bStarted = true;
		int32 Skip = 0;
		FParse::Value(FCommandLine::Get(), TEXT("NHBridgeIndex="), Skip);
		if (ANHGameDirector* Dir = ANHGameDirector::Get(this))
		{
			Dir->Dialogue = ANHGameDirector::FDialogue();
		}
		if (ANHTraffic* Traffic = ANHTraffic::Get(this))
		{
			Traffic->SetDensity(0); // the bridge and the vehicle alone: other traffic would only confuse a failure
		}
		// -NHBridgeProfile="Third Mainland Bridge": no driving. Every 100 m along each of that road's ways, what a ray from
		// above meets at the middle of the carriageway and 3 m and 6 m to either side: its height and what it is.
		FString Profile;
		if (FParse::Value(FCommandLine::Get(), TEXT("NHBridgeProfile="), Profile, false))
		{
			const UNHGameData* Data = UNHGameData::Get(this);
			for (int32 WayIndex = 0; Data && WayIndex < Data->RoadWays.Num(); ++WayIndex)
			{
				const FNHRoadWay& W = Data->RoadWays[WayIndex];
				if (!W.Name.Contains(Profile))
				{
					continue;
				}
				UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: [bridgeprofile] way %d: %s, class %d, %s, %d points"), WayIndex, *W.Name, W.Class, W.bBridge ? TEXT("bridge") : TEXT("not a bridge"), W.Nodes.Num());
				float Run = 0.f, Next = 0.f, Step = 10000.f;
				FParse::Value(FCommandLine::Get(), TEXT("NHBridgeStep="), Step); // cm between samples
				for (int32 I = 1; I < W.Nodes.Num(); ++I)
				{
					const FVector2D P0 = Data->RoadNodes[W.Nodes[I - 1]], P1 = Data->RoadNodes[W.Nodes[I]];
					const float Seg = FVector2D::Distance(P0, P1);
					const FVector2D Dir = (P1 - P0).GetSafeNormal(), Side(-Dir.Y, Dir.X);
					for (; Next <= Run + Seg; Next += Step)
					{
						const FVector2D At = P0 + Dir * (Next - Run);
						FString Line;
						FString What;
						for (const float Off : { -600.f, -300.f, 0.f, 300.f, 600.f })
						{
							const FVector2D Q = At + Side * Off;
							FHitResult Hit;
							const bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, FVector(Q, 12000.f), FVector(Q, -3000.f), FCollisionObjectQueryParams(ECC_WorldStatic));
							Line += bHit ? FString::Printf(TEXT("%6.0f"), Hit.ImpactPoint.Z) : FString(TEXT("  none"));
							if (Off == 0.f)
							{
								What = bHit && Hit.GetActor() ? Hit.GetActor()->GetActorNameOrLabel() : FString(TEXT("-"));
							}
						}
						UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: [bridgeprofile] %5.0f m  at %8.0f %8.0f  heights%s  %s"), Next / 100.f, At.X, At.Y, *Line, *What);
					}
					Run += Seg;
				}
			}
			PC->ConsoleCommand(TEXT("quit"));
			SetActorTickEnabled(false);
			return;
		}
		if (!FindBridge(Skip))
		{
			UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: [bridgetest] RESULT: FAILED. No bridge the road graph can route over was found."));
			PC->ConsoleCommand(TEXT("quit"));
			SetActorTickEnabled(false);
			return;
		}
		Leg = 2;
		NextLeg();
		return;
	}
	ANHVehicle* V = Car.Get();
	if (V && V->IsWrecked())
	{
		EndLeg(false, FString::Printf(TEXT("wrecked after %.0f s and %.0f m, at height %.0f cm (highest %.0f cm)"), LegTime, Along / 100.f, V->GetActorLocation().Z, Top));
		return;
	}
	if (V && PC->GetPawn() != V && PutBack < 5)
	{
		// Something pressed the get-out key (seen 150 m into a drive, twice, from the input system): the driver is sat
		// back at the wheel and the drive goes on. It is counted, and said at the end of the leg.
		++PutBack;
		if (APawn* Walker = PC->GetPawn())
		{
			Walker->SetActorLocation(V->ExitPoint(), false, nullptr, ETeleportType::TeleportPhysics);
		}
		PC->EnterVehicle(V);
		return;
	}
	if (!V || PC->GetPawn() != V)
	{
		EndLeg(false, TEXT("the driver is no longer in the vehicle"));
		return;
	}
	const TArray<FVector2D>& L = Line();
	const FVector At = V->GetActorLocation();
	// how far along the line: the nearest point not behind where it already got to
	float Run = 0.f, Best = 1e12f, BestAlong = Along, Total = 0.f;
	for (int32 I = 1; I < L.Num(); ++I)
	{
		const float Seg = FVector2D::Distance(L[I - 1], L[I]);
		const FVector P = FMath::ClosestPointOnSegment(FVector(At.X, At.Y, 0.f), FVector(L[I - 1], 0.f), FVector(L[I], 0.f));
		const float Here = Run + FVector2D::Distance(L[I - 1], FVector2D(P)), D = FVector2D::DistSquared(FVector2D(P), FVector2D(At));
		if (Here >= Along - 500.f && Here <= Along + 6000.f && D < Best)
		{
			Best = D;
			BestAlong = Here;
		}
		Run += Seg;
	}
	Total = Run;
	Along = FMath::Max(Along, BestAlong);
	// a driver's eye: aim for a point a little way ahead on the right-hand side of the line
	const float Ahead = Along + 700.f + FMath::Abs(V->Speed) * 0.5f;
	FVector2D Aim = L.Last();
	Run = 0.f;
	for (int32 I = 1; I < L.Num(); ++I)
	{
		const float Seg = FVector2D::Distance(L[I - 1], L[I]);
		if (Run + Seg >= Ahead)
		{
			const FVector2D Dir = (L[I] - L[I - 1]).GetSafeNormal();
			Aim = L[I - 1] + Dir * (Ahead - Run) + FVector2D(-Dir.Y, Dir.X) * 250.f;
			break;
		}
		Run += Seg;
	}
	const float Want = FMath::RadiansToDegrees(FMath::Atan2(Aim.Y - At.Y, Aim.X - At.X));
	const float Turn = FMath::FindDeltaAngleDegrees(V->GetActorRotation().Yaw, Want);
	float Fast = 1500.f; // -NHBridgeSpeed=3300: how fast it is driven where the road is straight, cm/s
	FParse::Value(FCommandLine::Get(), TEXT("NHBridgeSpeed="), Fast);
	const float Cruise = FMath::Abs(Turn) > 25.f ? 700.f : Fast;
	// how far its underside is off whatever is beneath it: a vehicle in the air
	FHitResult Below;
	FCollisionQueryParams NotIt(SCENE_QUERY_STAT(NHBridgeBelow), false, V);
	if (GetWorld()->LineTraceSingleByObjectType(Below, V->GetActorLocation(), V->GetActorLocation() - FVector(0.f, 0.f, 20000.f), FCollisionObjectQueryParams(ECC_WorldStatic), NotIt))
	{
		if (Below.Distance > HighestOff)
		{
			HighestOff = Below.Distance;
			HighestOffAlong = Along;
		}
		AirTime += Below.Distance > 220.f ? DeltaSeconds : 0.f;
	}
	V->SetDriveInput(V->Speed < Cruise ? 1.f : 0.f, V->Speed > Cruise + 300.f ? 0.5f : 0.f, FMath::Clamp(Turn / 30.f, -1.f, 1.f));

	LegTime += DeltaSeconds;
	Top = FMath::Max(Top, static_cast<float>(At.Z));
	if (LegTime > 3.f)
	{
		Slowest = FMath::Min(Slowest, V->Speed);
	}
	// the slope under it, from how far it rose over the ground it covered
	const float Flat = FVector::Dist2D(At, LastAt);
	float Slope = 0.f;
	if (Flat > 150.f)
	{
		Slope = static_cast<float>((At.Z - LastAt.Z) / Flat * 100.f);
		SteepestSeen = FMath::Max(SteepestSeen, FMath::Abs(Slope));
		LastAt = At;
	}
	StuckTime = FMath::Abs(V->Speed) < 60.f && LegTime > 3.f ? StuckTime + DeltaSeconds : 0.f;

	const FString Where = FString::Printf(TEXT("%.0f m of %.0f m along, at %.0f, %.0f, height %.0f cm (highest %.0f cm), health %.0f of %.0f"), Along / 100.f, Total / 100.f, At.X, At.Y, At.Z, Top, V->Health, V->MaxHealth);
	if (At.Z < -600.f)
	{
		EndLeg(false, TEXT("fell through the world, ") + Where);
	}
	else if (StuckTime > 4.f)
	{
		EndLeg(false, TEXT("stuck: not moving for 4 s, ") + Where);
	}
	else if (V->IsWrecked())
	{
		EndLeg(false, TEXT("wrecked, ") + Where);
	}
	else if (LegTime > 180.f)
	{
		EndLeg(false, TEXT("took over 3 minutes, ") + Where);
	}
	else if (Along >= Total - 1200.f)
	{
		EndLeg(true, FString::Printf(TEXT("%.0f m in %.0f s, never slower than %.0f km/h after setting off, highest point %.0f cm, health %.0f of %.0f"), Total / 100.f, LegTime, Slowest * 0.036f, Top, V->Health, V->MaxHealth));
	}
}
