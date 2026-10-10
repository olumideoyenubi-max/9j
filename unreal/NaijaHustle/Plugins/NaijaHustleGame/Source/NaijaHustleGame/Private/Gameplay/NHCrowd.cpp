#include "Gameplay/NHCrowd.h"

#include "Core/NHGameData.h"
#include "EngineUtils.h"
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

bool ANHCrowd::OpenGround(const FVector2D& At, float& OutZ) const
{
	// every solid thing on a line from the sky down: the ground is the lowest, and anything well above it is a deck or a roof
	TArray<FHitResult> Hits;
	GetWorld()->LineTraceMultiByObjectType(Hits, FVector(At, 20000.f), FVector(At, -3000.f), FCollisionObjectQueryParams(ECC_WorldStatic));
	FHitResult One;
	if (Hits.Num() == 0 && GetWorld()->LineTraceSingleByObjectType(One, FVector(At, 20000.f), FVector(At, -3000.f), FCollisionObjectQueryParams(ECC_WorldStatic)))
	{
		Hits.Add(One);
	}
	if (Hits.Num() == 0)
	{
		return false;
	}
	float Low = TNumericLimits<float>::Max(), High = TNumericLimits<float>::Lowest();
	for (const FHitResult& Hit : Hits)
	{
		Low = FMath::Min(Low, Hit.ImpactPoint.Z);
		High = FMath::Max(High, Hit.ImpactPoint.Z);
	}
	OutZ = Low;
	return High - Low < 250.f;
}

bool ANHCrowd::FindSpot(const FVector& Player, float Near, float Far, FVector& OutAt, FVector2D& OutA, FVector2D& OutB) const
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
		if (FVector2D::Distance(A, B) < 600.f)
		{
			continue;
		}
		const FVector2D Along = (B - A).GetSafeNormal();
		// out at the side of the road, either side, a little way in from the kerb; the whole stretch keeps that distance
		const FVector2D Out = FVector2D(-Along.Y, Along.X) * (FMath::RandBool() ? 1.f : -1.f) * (Data->HalfWidth(Way) + FMath::FRandRange(120.f, 260.f));
		const FVector2D At = FMath::Lerp(A, B, FMath::FRandRange(0.1f, 0.9f)) + Out;
		const float Distance = FVector2D::Distance(At, FVector2D(Player));
		if (Distance < Near || Distance > Far)
		{
			continue;
		}
		// no bridge or expressway within 15 m of the spot or of either end: a flyover runs over and beside the street below it
		bool bClear = true;
		for (const FVector2D& Point : { At, A + Out, B + Out })
		{
			TArray<FNHRoadSeg> Close;
			Data->RoadsNear(Point, 1500.f, Close);
			for (const FNHRoadSeg& Other : Close)
			{
				// RoadsNear gives every stretch in the cells round the point, so measure to each one
				const FNHRoadWay& Near1 = Data->RoadWays[Other.Way];
				if ((Near1.bBridge || Near1.Class < 2) && Near1.Nodes.IsValidIndex(Other.Index + 1)
					&& FMath::PointDistToSegment(FVector(Point, 0.f), FVector(Data->RoadNodes[Near1.Nodes[Other.Index]], 0.f), FVector(Data->RoadNodes[Near1.Nodes[Other.Index + 1]], 0.f)) < 1500.f)
				{
					bClear = false;
				}
			}
		}
		float Z = 0.f, ZA = 0.f, ZB = 0.f;
		if (!bClear || !OpenGround(At, Z) || !OpenGround(A + Out, ZA) || !OpenGround(B + Out, ZB))
		{
			continue;
		}
		OutAt = FVector(At, Z + 5.f);
		OutA = A + Out;
		OutB = B + Out;
		return true;
	}
	return false;
}

void ANHCrowd::SendOn(FWalker& Walker) const
{
	// to one end of their stretch of the road's edge; next time, back to the other
	ANHPerson* Body = Walker.Body.Get();
	const FVector2D To = Walker.bToB ? Walker.EndB : Walker.EndA;
	Walker.bToB = !Walker.bToB;
	Body->WalkTo(FVector(To, Body->GetActorLocation().Z), FMath::FRandRange(105.f, 165.f));
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
		FVector2D EndA, EndB;
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
			else if (!Body->IsFleeing() && FindSpot(Player, 4500.f, 8000.f, At, EndA, EndB))
			{
				Body->StopWalking();
				Body->SetActorLocation(At);
				Walkers[I].EndA = EndA;
				Walkers[I].EndB = EndB;
				Walkers[I].bToB = FMath::RandBool();
				SendOn(Walkers[I]);
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
				SendOn(Walkers[I]);
			}
		}
	}

	// one more at a time, out of the way; the first fill may stand them nearer, since nobody has looked yet
	FVector At;
	FVector2D EndA, EndB;
	for (int32 N = 0; N < (bFilled ? 1 : 3) && Walkers.Num() < Want; ++N)
	{
		if (!FindSpot(Player, bFilled ? 4500.f : 1500.f, 8000.f, At, EndA, EndB))
		{
			break;
		}
		ANHPerson* Body = GetWorld()->SpawnActor<ANHPerson>(ANHPerson::StaticClass(), At, FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(EndB.Y - EndA.Y, EndB.X - EndA.X)), 0.f));
		if (!Body)
		{
			break;
		}
		++Made;
		const bool bWoman = FMath::FRand() < Women;
		Body->Init(9000 + Made * 13, FLinearColor::MakeFromHSV8(static_cast<uint8>(Made * 47), 140, 190),
			bWoman ? ENHCast::Woman : Made % 7 == 0 ? ENHCast::Anyone : ENHCast::Lagosian);
		FWalker Walker;
		Walker.Body = Body;
		Walker.Idle = FMath::FRandRange(1.f, 5.f);
		Walker.EndA = EndA;
		Walker.EndB = EndB;
		Walker.bToB = FMath::RandBool();
		SendOn(Walker);
		Walkers.Add(Walker);
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
		// -NHPopulationDistrict=Iganmu: the test from the road nearest the middle of that district, not from the start
		FString District;
		const UNHGameData* Data = UNHGameData::Get(this);
		APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
		FVector2D Centre, OnRoad;
		FNHRoadSeg Seg;
		if (Data && Pawn && FParse::Value(FCommandLine::Get(), TEXT("NHPopulationDistrict="), District) && Data->DistrictCentre(District, Centre) && Data->NearestRoad(Centre, Seg, OnRoad))
		{
			FHitResult Floor;
			const bool bFloor = GetWorld()->LineTraceSingleByObjectType(Floor, FVector(OnRoad, 20000.f), FVector(OnRoad, -3000.f), FCollisionObjectQueryParams(ECC_WorldStatic));
			Pawn->SetActorLocation(FVector(OnRoad, (bFloor ? Floor.ImpactPoint.Z : 0.f) + 120.f), false, nullptr, ETeleportType::TeleportPhysics);
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
	// anybody on foot within 12 m of a bridge's line or an expressway's, or standing more than 1.5 m above the ground: there should be none
	int32 OnFoot = 0, Misplaced = 0;
	if (const UNHGameData* Data = UNHGameData::Get(this); Data && Crowd)
	{
		for (TActorIterator<ANHPerson> It(GetWorld()); It; ++It)
		{
			++OnFoot;
			TArray<FNHRoadSeg> Close;
			Data->RoadsNear(FVector2D(It->GetActorLocation()), 1200.f, Close);
			bool bBad = false;
			for (const FNHRoadSeg& Seg : Close)
			{
				const FNHRoadWay& Way = Data->RoadWays[Seg.Way];
				bBad |= (Way.bBridge || Way.Class < 2) && Way.Nodes.IsValidIndex(Seg.Index + 1)
					&& FMath::PointDistToSegment(FVector(FVector2D(It->GetActorLocation()), 0.f), FVector(Data->RoadNodes[Way.Nodes[Seg.Index]], 0.f), FVector(Data->RoadNodes[Way.Nodes[Seg.Index + 1]], 0.f)) < 1200.f;
			}
			float Z = 0.f;
			bBad |= Crowd->OpenGround(FVector2D(It->GetActorLocation()), Z) && It->GetActorLocation().Z > Z + 150.f && !It->IsDown();
			Misplaced += bBad ? 1 : 0;
		}
	}
	UE_LOG(LogNHGame, Log, TEXT("[populationtest] %d people on foot in all, %d of them by a bridge or expressway or off the ground"), OnFoot, Misplaced);
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
