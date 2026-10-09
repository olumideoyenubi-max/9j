#include "Gameplay/NHCrowd.h"

#include "Core/NHGameData.h"
#include "Gameplay/NHPerson.h"
#include "HAL/PlatformMemory.h"
#include "Kismet/GameplayStatics.h"
#include "NaijaHustleGame.h"
#include "Vehicles/NHTraffic.h"

ANHCrowd::ANHCrowd()
{
	PrimaryActorTick.bCanEverTick = true;
}

ANHCrowd* ANHCrowd::Get(const UObject* WorldContext)
{
	return WorldContext ? Cast<ANHCrowd>(UGameplayStatics::GetActorOfClass(WorldContext, ANHCrowd::StaticClass())) : nullptr;
}

bool ANHCrowd::FindSpot(const FVector& Player, float Near, float Far, FVector& OutAt, FVector2D& OutAlong) const
{
	const UNHGameData* Data = UNHGameData::Get(this);
	TArray<FNHRoadSeg> Around;
	Data->RoadsNear(FVector2D(Player), Far, Around);
	for (int32 Try = 0; Try < 10 && Around.Num() > 0; ++Try)
	{
		const FNHRoadSeg& Seg = Around[FMath::RandRange(0, Around.Num() - 1)];
		const FNHRoadWay& Way = Data->RoadWays[Seg.Way];
		if (Way.Class < 2 || Way.Class > 4 || Way.bBridge || !Way.Nodes.IsValidIndex(Seg.Index + 1))
		{
			continue; // nobody walks an expressway, a slip road or a bridge
		}
		const FVector2D A = Data->RoadNodes[Way.Nodes[Seg.Index]], B = Data->RoadNodes[Way.Nodes[Seg.Index + 1]];
		const FVector2D Along = (B - A).GetSafeNormal();
		const FVector2D Side(-Along.Y, Along.X);
		// on the edge of the road, either side, a little way in from the kerb
		const FVector2D At = FMath::Lerp(A, B, FMath::FRand()) + Side * (FMath::RandBool() ? 1.f : -1.f) * (Data->HalfWidth(Way) + FMath::FRandRange(120.f, 260.f));
		const float Distance = FVector2D::Distance(At, FVector2D(Player));
		if (Distance < Near || Distance > Far)
		{
			continue;
		}
		OutAt = FVector(At, Player.Z + 80.f);
		OutAlong = FMath::RandBool() ? Along : -Along;
		return true;
	}
	return false;
}

void ANHCrowd::SendOn(ANHPerson* Body, const FVector2D& Along) const
{
	Body->WalkTo(Body->GetActorLocation() + FVector(Along, 0.f) * FMath::FRandRange(3000.f, 9000.f), FMath::FRandRange(105.f, 165.f));
}

void ANHCrowd::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UNHGameData* Data = UNHGameData::Get(this);
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const ANHTraffic* Traffic = ANHTraffic::Get(this);
	if (!Data || !Data->bRealCity || !Pawn || !Traffic)
	{
		return;
	}
	Think -= DeltaSeconds;
	if (Think > 0.f)
	{
		return;
	}
	Think = 0.3f;
	const FVector Player = Pawn->GetActorLocation();
	float ZoneShare = 1.f, Women = 0.45f;
	Traffic->ZonePeople(Traffic->ZoneAt(FVector2D(Player)), ZoneShare, Women);
	const int32 Want = FMath::RoundToInt(MaxPeople * ZoneShare * Traffic->HourShare(1));

	for (int32 I = Walkers.Num() - 1; I >= 0; --I)
	{
		ANHPerson* Body = Walkers[I].Body.Get();
		if (!Body || Body->IsDown())
		{
			Walkers.RemoveAtSwap(I); // gone, or down and on their own clock now
			continue;
		}
		const float Distance = FVector::Dist2D(Body->GetActorLocation(), Player);
		FVector At;
		FVector2D Along;
		if (Distance > 9000.f || Walkers.Num() > Want)
		{
			// left behind: stood somewhere new ahead, not made again. One too many for the hour is let go instead.
			if (Walkers.Num() > Want)
			{
				if (Distance > 4000.f)
				{
					Body->Destroy();
					Walkers.RemoveAtSwap(I);
				}
			}
			else if (!Body->IsFleeing() && FindSpot(Player, 4500.f, 8000.f, At, Along))
			{
				Body->StopWalking();
				Body->SetActorLocation(At);
				SendOn(Body, Along);
				++Moved;
			}
		}
		else if (!Body->IsWalking() && !Body->IsFleeing())
		{
			// reached where they were going: stand a moment, then on, the same way or back
			Walkers[I].Idle -= 0.3f;
			if (Walkers[I].Idle <= 0.f)
			{
				Walkers[I].Idle = FMath::FRandRange(1.f, 6.f);
				SendOn(Body, FVector2D(Body->GetActorForwardVector()) * (FMath::FRand() < 0.75f ? 1.f : -1.f));
			}
		}
	}

	// one more at a time, out of the way; the first fill may stand them nearer, since nobody has looked yet
	FVector At;
	FVector2D Along;
	for (int32 N = 0; N < (bFilled ? 1 : 3) && Walkers.Num() < Want; ++N)
	{
		if (!FindSpot(Player, bFilled ? 4500.f : 1500.f, 8000.f, At, Along))
		{
			break;
		}
		ANHPerson* Body = GetWorld()->SpawnActor<ANHPerson>(ANHPerson::StaticClass(), At, FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), 0.f));
		if (!Body)
		{
			break;
		}
		++Made;
		const bool bWoman = FMath::FRand() < Women;
		Body->Init(9000 + Made * 13, FLinearColor::MakeFromHSV8(static_cast<uint8>(Made * 47), 140, 190),
			bWoman ? ENHCast::Woman : Made % 7 == 0 ? ENHCast::Anyone : ENHCast::Lagosian);
		SendOn(Body, Along);
		Walkers.Add({ Body, FMath::FRandRange(1.f, 5.f) });
	}
	bFilled |= Walkers.Num() >= Want / 2;
}

// ---------------------------------------------------------------------------------------------- the count
ANHPopulationTest::ANHPopulationTest()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ANHPopulationTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Clock == 0.f)
	{
		// -NHDensity=3: the test at another of the pause menu's traffic settings (0 none .. 3 heavy)
		int32 Level = -1;
		if (ANHTraffic* Traffic = ANHTraffic::Get(this); Traffic && FParse::Value(FCommandLine::Get(), TEXT("NHDensity="), Level))
		{
			Traffic->SetDensity(Level);
		}
	}
	Clock += DeltaSeconds;
	if (bDone || Clock < 10.f)
	{
		return; // ten seconds for the level and the first fill
	}
	++Frames;
	FrameSeconds += FApp::GetDeltaTime();
	if (Clock < NextLine)
	{
		return;
	}
	NextLine += 10.f;
	const ANHTraffic* Traffic = ANHTraffic::Get(this);
	const ANHCrowd* Crowd = ANHCrowd::Get(this);
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const float Ms = Frames > 0 ? 1000.0 * FrameSeconds / Frames : 0.f;
	UE_LOG(LogNHGame, Log, TEXT("[populationtest] %2.0f s: %s zone, %d of 3 density; vehicles %d moving + %d parked, %d models, %d in the pool, %d reused; pedestrians %d (%d made, %d moved on); %.1f ms a frame (%.0f fps); memory %.0f MB"),
		Clock, Traffic && Pawn ? *Traffic->ZoneAt(FVector2D(Pawn->GetActorLocation())).ToString() : TEXT("?"), Traffic ? Traffic->GetDensity() : -1,
		Traffic ? Traffic->NumMoving() : 0, Traffic ? Traffic->NumParked() : 0, Traffic ? Traffic->NumModels() : 0, Traffic ? Traffic->NumPooled() : 0, Traffic ? Traffic->NumReused() : 0,
		Crowd ? Crowd->NumPeople() : 0, Crowd ? Crowd->NumMade() : 0, Crowd ? Crowd->NumMoved() : 0, Ms, Ms > 0.f ? 1000.f / Ms : 0.f, FPlatformMemory::GetStats().UsedPhysical / 1048576.0);
	Frames = 0;
	FrameSeconds = 0.0;
	if (Clock >= 49.f)
	{
		bDone = true;
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			PC->ConsoleCommand(TEXT("shot showui"));
			FTimerHandle Quit;
			GetWorldTimerManager().SetTimer(Quit, FTimerDelegate::CreateWeakLambda(PC, [PC] { PC->ConsoleCommand(TEXT("quit")); }), 2.5f, false);
		}
	}
}
