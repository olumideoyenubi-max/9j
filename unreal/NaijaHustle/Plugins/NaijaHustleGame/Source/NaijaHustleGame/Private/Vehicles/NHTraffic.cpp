#include "Vehicles/NHTraffic.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NaijaHustleGame.h"
#include "Vehicles/NHVehicle.h"

ANHTraffic::ANHTraffic()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ANHTraffic* ANHTraffic::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	TActorIterator<ANHTraffic> It(World);
	return World && It ? *It : nullptr;
}

void ANHTraffic::SetDensity(int32 Level)
{
	static const int32 Moving[] = { 0, 5, 9, 14 }, Parked[] = { 0, 4, 6, 8 };
	Level = FMath::Clamp(Level, 0, 3);
	MaxMoving = Moving[Level];
	MaxParked = Parked[Level];
	// fewer wanted than there are: the furthest go at once (one the player is in is not in Cars)
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const FVector2D Player = PC && PC->GetPawn() ? FVector2D(PC->GetPawn()->GetActorLocation()) : FVector2D::ZeroVector;
	Cars.Sort([&Player](const FCar& A, const FCar& B) { return FVector2D::DistSquared(A.At, Player) < FVector2D::DistSquared(B.At, Player); });
	for (int32 I = Cars.Num() - 1; I >= 0 && (NumMoving() > MaxMoving || NumParked() > MaxParked); --I)
	{
		if ((Cars[I].bParked ? NumParked() > MaxParked : NumMoving() > MaxMoving))
		{
			if (Cars[I].Vehicle.IsValid())
			{
				Cars[I].Vehicle->Destroy();
			}
			Cars.RemoveAt(I);
		}
	}
}

FString ANHTraffic::Hail(const FVector& Player)
{
	FCar* Best = nullptr;
	float BestDist = 8000.f;
	for (FCar& Car : Cars)
	{
		const float Dist = FVector2D::Distance(Car.At, FVector2D(Player));
		if (!Car.bParked && Car.Vehicle.IsValid() && Dist < BestDist)
		{
			BestDist = Dist;
			Best = &Car;
		}
	}
	if (!Best)
	{
		return FString();
	}
	Best->Wait = 15.f;
	return Best->Vehicle->DisplayName();
}

USkeletalMesh* ANHTraffic::DriverFor(FName Type)
{
	if (!bDriversLoaded)
	{
		bDriversLoaded = true;
		for (const TCHAR* Path : { TEXT("/Game/Characters/Player/Runner/Runner"), TEXT("/Game/Characters/Player/Runner/Runner_Suit"), TEXT("/Game/Characters/Player/Runner/Runner_Dispatch") })
		{
			Drivers.Add(FPackageName::DoesPackageExist(Path) ? LoadObject<USkeletalMesh>(nullptr, Path) : nullptr);
		}
	}
	// a dispatch rider on the okada, a suit in the luxury cars, the street outfit in everything else
	const FString Name = Type.ToString();
	const int32 Want = Type == TEXT("okada") ? 2 : (Name.StartsWith(TEXT("lux")) || (Name.Contains(TEXT("suv")) && Name != TEXT("suv")) || Type == TEXT("sports")) ? 1 : 0;
	return Drivers[Want] ? Drivers[Want].Get() : Drivers[0].Get();
}

int32 ANHTraffic::NumMoving() const
{
	int32 N = 0;
	for (const FCar& Car : Cars)
	{
		N += Car.bParked ? 0 : 1;
	}
	return N;
}

FVector2D ANHTraffic::NodeAt(const FNHRoadSeg& Seg, int32 Dir, bool bEnd) const
{
	const TArray<int32>& N = Data->RoadWays[Seg.Way].Nodes;
	return Data->RoadNodes[N[Seg.Index + ((Dir > 0) == bEnd ? 1 : 0)]];
}

void ANHTraffic::Rail(const FCar& Car, FVector2D& OutAt, float& OutYaw) const
{
	const FNHRoadWay& Way = Data->RoadWays[Car.Seg.Way];
	const FVector2D A = NodeAt(Car.Seg, Car.Dir, false), B = NodeAt(Car.Seg, Car.Dir, true);
	const FVector2D Along = (B - A).GetSafeNormal();
	const FVector2D Right(-Along.Y, Along.X);
	// parked: against the kerb; moving: the middle of its half of the carriageway
	const float Off = Car.bParked ? Data->HalfWidth(Way) - 140.f : Data->HalfWidth(Way) * 0.5f;
	OutAt = A + Along * Car.Along + Right * Off * Car.Side;
	OutYaw = FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X));
}

float ANHTraffic::Cruise(const FNHRoadWay& Way) const
{
	static const float ByClass[] = { 2000.f, 1700.f, 1400.f, 1150.f, 950.f, 800.f }; // cm/s: 72 km/h on a motorway down to 29 on a slip road
	return ByClass[FMath::Min<int32>(Way.Class, 5)];
}

bool ANHTraffic::NextSegment(FCar& Car) const
{
	const FNHRoadWay& Way = Data->RoadWays[Car.Seg.Way];
	const int32 EndNode = Way.Nodes[Car.Seg.Index + (Car.Dir > 0 ? 1 : 0)];
	const FVector2D Here = Data->RoadNodes[EndNode];
	const FVector2D Was = (Here - NodeAt(Car.Seg, Car.Dir, false)).GetSafeNormal();

	// every stretch of road that leaves this node, but not back the way it came and not the wrong way up a one-way
	struct FWayOn { FNHRoadSeg Seg; int32 Dir; float Straight; };
	TArray<FWayOn, TInlineAllocator<8>> Ways;
	if (const TArray<FNHRoadSeg>* Joins = Data->RoadJoins.Find(EndNode))
	{
		for (const FNHRoadSeg& S : *Joins)
		{
			const FNHRoadWay& W = Data->RoadWays[S.Way];
			const int32 Dir = W.Nodes[S.Index] == EndNode ? 1 : -1;
			if ((S == Car.Seg && Dir != Car.Dir) || (W.bOneWay && Dir < 0))
			{
				continue;
			}
			if (S == Car.Seg)
			{
				continue; // a loop of one segment
			}
			const FVector2D To = (NodeAt(S, Dir, true) - Here).GetSafeNormal();
			Ways.Add({ S, Dir, static_cast<float>(Was | To) });
		}
	}
	if (Ways.Num() == 0)
	{
		return false;
	}
	// mostly straight on; one time in four, any of the turns that is not a hairpin
	int32 Pick = 0;
	for (int32 I = 1; I < Ways.Num(); ++I)
	{
		if (Ways[I].Straight > Ways[Pick].Straight)
		{
			Pick = I;
		}
	}
	if (Ways.Num() > 1 && FMath::FRand() < 0.25f)
	{
		const int32 Other = FMath::RandRange(0, Ways.Num() - 1);
		if (Ways[Other].Straight > -0.3f)
		{
			Pick = Other;
		}
	}
	const bool bWasOneWay = Way.bOneWay;
	Car.Seg = Ways[Pick].Seg;
	Car.Dir = Ways[Pick].Dir;
	if (!Data->RoadWays[Car.Seg.Way].bOneWay)
	{
		Car.Side = 1.f; // two-way: keep right
	}
	else if (!bWasOneWay)
	{
		Car.Side = FMath::RandBool() ? 1.f : -1.f; // onto a one-way carriageway: either lane
	}
	return true;
}

bool ANHTraffic::Blocked(const FCar& Car, const FVector& Player, float& OutGap) const
{
	const float YawRad = FMath::DegreesToRadians(Car.Yaw);
	const FVector2D Fwd(FMath::Cos(YawRad), FMath::Sin(YawRad)), Right(-Fwd.Y, Fwd.X);
	const float Look = 900.f + Car.Speed * 1.2f;
	OutGap = Look;
	const auto Ahead = [&](const FVector2D& P, float HalfWide)
	{
		const FVector2D D = P - Car.At;
		const float F = D | Fwd;
		if (F > 50.f && F < OutGap && FMath::Abs(D | Right) < HalfWide)
		{
			OutGap = F;
		}
	};
	Ahead(FVector2D(Player), 220.f);
	for (const FCar& Other : Cars)
	{
		if (&Other != &Car && Other.Vehicle.IsValid())
		{
			Ahead(Other.At, Other.bParked ? 200.f : 240.f);
		}
	}
	for (const TWeakObjectPtr<ANHVehicle>& V : Taken)
	{
		if (V.IsValid())
		{
			Ahead(FVector2D(V->GetActorLocation()), 260.f);
		}
	}
	return OutGap < Look;
}

void ANHTraffic::Step(FCar& Car, const FVector& Player, float DeltaSeconds)
{
	ANHVehicle* V = Car.Vehicle.Get();
	if (Car.bParked)
	{
		return;
	}
	float Gap = 0.f;
	float Want = Cruise(Data->RoadWays[Car.Seg.Way]);
	if (Car.Wait > 0.f)
	{
		Car.Wait -= DeltaSeconds; // flagged down: pulled up for the player
		Want = 0.f;
	}
	else if (Blocked(Car, Player, Gap))
	{
		Want = Gap < 650.f ? 0.f : FMath::Min(Want, (Gap - 650.f) * 0.8f); // stop a car's length short
	}
	Car.Speed = FMath::FInterpConstantTo(Car.Speed, Want, DeltaSeconds, Want < Car.Speed ? 1400.f : 450.f);
	Car.Along += Car.Speed * DeltaSeconds;
	Driven += Car.Speed * DeltaSeconds;
	for (int32 Guard = 0; Guard < 8; ++Guard)
	{
		const float Length = FVector2D::Distance(NodeAt(Car.Seg, Car.Dir, false), NodeAt(Car.Seg, Car.Dir, true));
		if (Car.Along < Length)
		{
			break;
		}
		Car.Along -= Length;
		const FNHRoadWay& Way = Data->RoadWays[Car.Seg.Way];
		const int32 Next = Car.Seg.Index + Car.Dir;
		if (Next >= 0 && Next + 1 < Way.Nodes.Num() && FMath::FRand() < 0.92f)
		{
			Car.Seg.Index = Next; // further along the same road (now and then it turns off at a side road instead)
		}
		else if (!NextSegment(Car))
		{
			if (Next >= 0 && Next + 1 < Way.Nodes.Num())
			{
				Car.Seg.Index = Next;
			}
			else
			{
				V->Destroy(); // the edge of the map or a dead end: gone, and another is made elsewhere
				Car.Vehicle = nullptr;
				return;
			}
		}
	}
	FVector2D Target;
	float TargetYaw = 0.f;
	Rail(Car, Target, TargetYaw);
	// ease onto the lane's line and round to its heading, so junctions and bends are turned rather than snapped
	Car.At = FMath::Vector2DInterpTo(Car.At, Target, DeltaSeconds, 5.f);
	Car.Yaw = FMath::RInterpTo(FRotator(0.f, Car.Yaw, 0.f), FRotator(0.f, TargetYaw, 0.f), DeltaSeconds, 3.5f).Yaw;
	V->TrafficMove(Car.At, Car.Yaw, Car.Speed, DeltaSeconds);
}

FName ANHTraffic::RandomType(bool bParked) const
{
	// what is on a Lagos road: danfos, kekes and okadas most of all, then ordinary cars, a few big ones and the odd luxury car
	static const TPair<const TCHAR*, int32> Mix[] = { { TEXT("danfo"), 22 }, { TEXT("sedan"), 20 }, { TEXT("keke"), 12 }, { TEXT("okada"), 12 }, { TEXT("suv"), 12 },
		{ TEXT("truck"), 5 }, { TEXT("tfpick"), 2 }, { TEXT("luxsedan"), 4 }, { TEXT("luxsuv"), 4 }, { TEXT("royalsuv"), 2 }, { TEXT("coupesuv"), 2 }, { TEXT("supersuv"), 1 },
		{ TEXT("sports"), 1 }, { TEXT("luxcoupe"), 1 } };
	int32 Total = 0;
	for (const auto& M : Mix)
	{
		Total += M.Value;
	}
	for (int32 Try = 0; Try < 6; ++Try)
	{
		int32 Roll = FMath::RandRange(0, Total - 1);
		for (const auto& M : Mix)
		{
			Roll -= M.Value;
			if (Roll < 0)
			{
				const FName Type(M.Key);
				if (Data->Vehicles.Contains(Type) && !(bParked && Type == TEXT("truck")))
				{
					return Type;
				}
				break;
			}
		}
	}
	return TEXT("sedan");
}

ANHVehicle* ANHTraffic::Make(FName Type, const FVector2D& At, float Yaw, bool bBridge)
{
	// on a bridge the deck is whatever a ray from above meets; elsewhere the road is at ground level, under any flyover
	float Z = 150.f;
	if (bBridge)
	{
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByObjectType(Hit, FVector(At.X, At.Y, 8000.f), FVector(At.X, At.Y, -500.f), FCollisionObjectQueryParams(ECC_WorldStatic)))
		{
			Z = Hit.ImpactPoint.Z + 150.f;
		}
	}
	const FTransform T(FRotator(0.f, Yaw, 0.f), FVector(At.X, At.Y, Z));
	ANHVehicle* V = GetWorld()->SpawnActorDeferred<ANHVehicle>(ANHVehicle::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!V)
	{
		return nullptr;
	}
	const FNHVehicleSpec& Spec = Data->Spec(Type);
	V->VehicleType = Type;
	V->Paint = Spec.Colors.Num() ? Spec.Colors[FMath::RandRange(0, Spec.Colors.Num() - 1)] : FLinearColor(0.5f, 0.5f, 0.5f);
	V->Board = Type == TEXT("danfo") ? TEXT("OSHODI") : TEXT("");
	UGameplayStatics::FinishSpawningActor(V, T);
	return V;
}

ANHVehicle* ANHTraffic::MakeForHire(FName Type, const FVector2D& At, float Yaw)
{
	Data = UNHGameData::Get(this);
	ANHVehicle* V = Data ? Make(Type, At, Yaw, false) : nullptr;
	if (V)
	{
		V->SetTraffic(true);
		V->SetNpcDriver(DriverFor(Type));
	}
	return V;
}

bool ANHTraffic::TrySpawn(const FVector2D& Player, bool bParked, float Near)
{
	TArray<FNHRoadSeg> Around;
	Data->RoadsNear(Player, SpawnFar, Around);
	if (Around.Num() == 0)
	{
		return false;
	}
	for (int32 Try = 0; Try < 12; ++Try)
	{
		FCar Car;
		Car.bParked = bParked;
		Car.Seg = Around[FMath::RandRange(0, Around.Num() - 1)];
		const FNHRoadWay& Way = Data->RoadWays[Car.Seg.Way];
		// parked cars stand on ordinary streets, not on expressways, slip roads or bridges
		if (bParked && (Way.Class < 2 || Way.Class > 4 || Way.bBridge))
		{
			continue;
		}
		Car.Dir = Way.bOneWay || FMath::RandBool() ? 1 : -1;
		Car.Side = Way.bOneWay && !bParked && FMath::RandBool() ? -1.f : 1.f;
		Car.Along = FMath::FRand() * FVector2D::Distance(NodeAt(Car.Seg, Car.Dir, false), NodeAt(Car.Seg, Car.Dir, true));
		Rail(Car, Car.At, Car.Yaw);
		const float Dist = FVector2D::Distance(Car.At, Player);
		if (Dist < Near || Dist > SpawnFar)
		{
			continue;
		}
		// room for it: clear of the other traffic, the stops and the motor park's bays
		bool bClear = true;
		for (const FCar& Other : Cars)
		{
			bClear &= FVector2D::Distance(Other.At, Car.At) > (bParked && Other.bParked ? 900.f : 1500.f);
		}
		for (const TPair<FName, FNHBusStop>& Stop : Data->Stops)
		{
			bClear &= FVector2D::Distance(Stop.Value.Kerb, Car.At) > 2500.f;
		}
		for (const FNHParkBay& Bay : Data->ParkBays)
		{
			bClear &= FVector2D::Distance(Bay.Pos, Car.At) > 1500.f;
		}
		if (!bClear)
		{
			continue;
		}
		ANHVehicle* V = Make(RandomType(bParked), Car.At, Car.Yaw, Way.bBridge);
		if (!V)
		{
			return false;
		}
		V->SetTraffic(!bParked);
		if (!bParked)
		{
			USkeletalMesh* Driver = DriverFor(V->VehicleType);
			V->SetNpcDriver(Driver);
			Seated += Driver ? 1 : 0;
		}
		Car.Vehicle = V;
		Car.Speed = bParked ? 0.f : Cruise(Way) * 0.7f;
		Cars.Add(Car);
		++Made;
		return true;
	}
	return false;
}

void ANHTraffic::Tidy(const FVector2D& Player)
{
	for (int32 I = Cars.Num() - 1; I >= 0; --I)
	{
		ANHVehicle* V = Cars[I].Vehicle.Get();
		if (!V)
		{
			Cars.RemoveAtSwap(I);
		}
		else if (V->GetController())
		{
			V->SetTraffic(false); // the player is driving it: theirs now
			Taken.Add(V);
			Cars.RemoveAtSwap(I);
		}
		else if (FVector2D::Distance(Cars[I].At, Player) > RemoveBeyond)
		{
			V->Destroy();
			Cars.RemoveAtSwap(I);
		}
	}
	for (int32 I = Taken.Num() - 1; I >= 0; --I)
	{
		ANHVehicle* V = Taken[I].Get();
		if (!V)
		{
			Taken.RemoveAtSwap(I);
		}
		else if (!V->GetController() && !V->bPlayerOwned && FVector2D::Distance(FVector2D(V->GetActorLocation()), Player) > RemoveBeyond * 1.5f)
		{
			V->Destroy();
			Taken.RemoveAtSwap(I);
		}
	}
}

void ANHTraffic::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Data = UNHGameData::Get(this);
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Data || !Data->bRealCity || !Pawn)
	{
		return;
	}
	const FVector Player = Pawn->GetActorLocation();
	Tidy(FVector2D(Player));
	for (FCar& Car : Cars)
	{
		if (Car.Vehicle.IsValid())
		{
			Step(Car, Player, DeltaSeconds);
		}
	}

	// top up a few at a time; the first fill may put them closer, since nobody has looked yet
	SpawnTimer -= DeltaSeconds;
	if (SpawnTimer <= 0.f)
	{
		SpawnTimer = 0.4f;
		const float Near = bFilled ? SpawnNear : 3500.f;
		for (int32 N = 0; N < (bFilled ? 1 : 4) && NumMoving() < MaxMoving; ++N)
		{
			TrySpawn(FVector2D(Player), false, Near);
		}
		for (int32 N = 0; N < (bFilled ? 1 : 4) && NumParked() < MaxParked; ++N)
		{
			TrySpawn(FVector2D(Player), true, bFilled ? 6000.f : 2500.f);
		}
		if (bFilled && ReportTime >= 0.f && (ReportTime += 0.4f) >= 20.f)
		{
			float Sum = 0.f;
			int32 Stopped = 0;
			for (const FCar& Car : Cars)
			{
				Sum += Car.bParked ? 0.f : Car.Speed;
				Stopped += !Car.bParked && Car.Speed < 50.f ? 1 : 0;
			}
			UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: traffic after 20 s: %d moving (%d of them held up) at %.0f km/h on average, %d parked; %.1f km driven in all, %d vehicles made, %d of them with a driver at the wheel"),
				NumMoving(), Stopped, NumMoving() ? Sum / NumMoving() * 0.036f : 0.f, NumParked(), Driven / 100000.0, Made, Seated);
			ReportTime = -1.f;
		}
		if (!bFilled && NumMoving() >= MaxMoving / 2)
		{
			bFilled = true;
			UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: traffic: %d moving and %d parked vehicles around the player"), NumMoving(), NumParked());
		}
	}
}
