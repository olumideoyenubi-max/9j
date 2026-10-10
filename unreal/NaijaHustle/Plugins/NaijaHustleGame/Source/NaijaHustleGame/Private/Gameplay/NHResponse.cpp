#include "Gameplay/NHResponse.h"
#include "Gameplay/NHMissions.h"

#include "Audio/NHAudioSubsystem.h"
#include "Core/NHGameData.h"
#include "Core/NHHustleSubsystem.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Gameplay/NHGameDirector.h"
#include "Gameplay/NHPerson.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "NaijaHustleGame.h"
#include "Player/NHCharacter.h"
#include "Player/NHPlayerController.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UI/NHHUD.h"
#include "Vehicles/NHVehicle.h"

ANHResponse::ANHResponse()
{
	PrimaryActorTick.bCanEverTick = true;
}

ANHResponse* ANHResponse::Get(const UObject* WorldContext)
{
	return WorldContext ? Cast<ANHResponse>(UGameplayStatics::GetActorOfClass(WorldContext, ANHResponse::StaticClass())) : nullptr;
}

void ANHResponse::BeginPlay()
{
	Super::BeginPlay();
	const UNHGameData* Data = UNHGameData::Get(this);
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!Data || !FFileHelper::LoadFileToString(Text, *(UNHGameData::DataDir() / TEXT("response_areas.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
	{
		UE_LOG(LogNHGame, Warning, TEXT("NAIJA HUSTLE: response: Data/response_areas.json could not be read"));
		return;
	}
	const auto Names = [&Root](const TCHAR* Field)
	{
		TArray<FString> Out;
		Root->TryGetStringArrayField(Field, Out);
		return Out;
	};
	HighClass.Append(Names(TEXT("highClass")));
	AreaBoys.Append(Names(TEXT("areaBoys")));
	if (Data->bRealCity)
	{
		for (const FString& Name : Names(TEXT("stations")))
		{
			FVector2D At;
			if (Data->DistrictCentre(Name, At))
			{
				Stations.Add({ Name, At });
			}
		}
	}
	else
	{
		Stations.Add({ TEXT("Motor Park"), Data->Park + FVector2D(6000.f, 0.f) }); // the small city has the one
	}
	double Number = 0.0;
	NearCm = Root->TryGetNumberField(TEXT("stationNearMetres"), Number) ? Number * 100.f : NearCm;
	FarCm = Root->TryGetNumberField(TEXT("stationFarMetres"), Number) ? Number * 100.f : FarCm;
	WitnessSeconds = Root->TryGetNumberField(TEXT("witnessSeconds"), Number) ? Number : WitnessSeconds;
	Root->TryGetNumberField(TEXT("starsNearStation"), StarsNear);
	Root->TryGetNumberField(TEXT("starsFarFromStation"), StarsFar);
	Root->TryGetNumberField(TEXT("starsForAreaBoys"), StarsBoys);
	Root->TryGetNumberField(TEXT("starsForArmy"), StarsArmy);
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: response: %d stations, %d high-class districts, %d area boys' districts"), Stations.Num(), HighClass.Num(), AreaBoys.Num());
}

ENHArea ANHResponse::AreaAt(const FVector& World) const
{
	const UNHGameData* Data = UNHGameData::Get(this);
	const FString District = Data ? Data->DistrictAt(World) : FString();
	return HighClass.Contains(District) ? ENHArea::HighClass : AreaBoys.Contains(District) ? ENHArea::AreaBoys : ENHArea::Ordinary;
}

bool ANHResponse::NearestStation(const FVector& World, FString& OutName, float& OutDistance) const
{
	OutDistance = TNumericLimits<float>::Max();
	for (const TPair<FString, FVector2D>& Station : Stations)
	{
		const float Distance = FVector2D::Distance(Station.Value, FVector2D(World));
		if (Distance < OutDistance)
		{
			OutDistance = Distance;
			OutName = Station.Key;
		}
	}
	return Stations.Num() > 0;
}

FString ANHResponse::AreaReport() const
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const UNHGameData* Data = UNHGameData::Get(this);
	if (!Pawn || !Data)
	{
		return TEXT("response: no player");
	}
	static const TCHAR* Kinds[] = { TEXT("ordinary streets"), TEXT("area boys' streets"), TEXT("high-class streets") };
	FString Station;
	float Distance = 0.f;
	const bool bAny = NearestStation(Pawn->GetActorLocation(), Station, Distance);
	return FString::Printf(TEXT("%s: %s; nearest station %s"), *Data->DistrictAt(Pawn->GetActorLocation()), Kinds[static_cast<int32>(AreaAt(Pawn->GetActorLocation()))],
		bAny ? *FString::Printf(TEXT("%s, %.1f km"), *Station, Distance / 100000.f) : TEXT("none"));
}

FString ANHResponse::Describe() const
{
	const UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	return FString::Printf(TEXT("response: %s; %d stars; %s%s; %d here, %d vehicles with %d still aboard%s"), *AreaReport(), Hustle ? Hustle->Stars() : 0,
		Coming == EComing::Army ? TEXT("the army coming") : Coming == EComing::TaskForce ? TEXT("Task Force coming") : Coming == EComing::Boys ? TEXT("area boys coming") : TEXT("nobody coming"),
		bAlarm ? TEXT(" (a witness called)") : TEXT(""), Units.Num(), Rides.Num(), Aboard(), bSeen ? TEXT(", and they see you") : TEXT(""));
}

bool ANHResponse::CanSee(const ANHPerson* From, const APawn* Pawn) const
{
	FCollisionQueryParams Query(SCENE_QUERY_STAT(NHResponseSee), false, From);
	Query.AddIgnoredActor(Pawn);
	if (const ANHPlayerController* PC = Cast<ANHPlayerController>(Pawn->GetController()))
	{
		Query.AddIgnoredActor(PC->GetOnFootCharacter());
	}
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, From->GetActorLocation() + FVector(0.f, 0.f, 150.f), Pawn->GetActorLocation() + FVector(0.f, 0.f, 40.f), ECC_Visibility, Query);
}

void ANHResponse::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	if (!Pawn || !Hustle)
	{
		return;
	}
	const FVector Player = Pawn->GetActorLocation();
	const int32 Stars = Hustle->Stars();

	Think -= DeltaSeconds;
	if (Think <= 0.f)
	{
		Think = 0.5f;
		Decide(Player, Stars);
	}
	if (Coming != EComing::Nobody)
	{
		NextArrival -= DeltaSeconds;
		const int32 Want = Coming == EComing::Boys ? FMath::Min(Stars + 1, 6) : Coming == EComing::Army ? 9 : FMath::Max(Stars, 2) + 1;
		const int32 Short = Want - Units.Num() - Aboard();
		if (NextArrival <= 0.f && Short > 0)
		{
			if (Coming == EComing::Boys)
			{
				NextArrival = 2.5f;
				Arrive(Player);
			}
			else
			{
				NextArrival = Coming == EComing::Army ? 7.f : 9.f;
				SendRide(Player, FMath::Min(Short, Coming == EComing::Army ? 5 : 4));
			}
		}
	}
	for (int32 I = Rides.Num() - 1; I >= 0; --I)
	{
		if (!Rides[I].Car.IsValid())
		{
			Rides.RemoveAtSwap(I);
			continue;
		}
		DriveRide(Rides[I], Pawn, DeltaSeconds);
	}
	bSeen = false;
	for (int32 I = Units.Num() - 1; I >= 0; --I)
	{
		ANHPerson* Body = Units[I].Body.Get();
		if (!Body || Body->IsDown())
		{
			if (Units[I].Kind != EComing::Boys)
			{
				Hustle->AddHeat(1.5f); // an officer or a soldier down: they send more
			}
			Units.RemoveAtSwap(I);
			continue;
		}
		Act(Units[I], Pawn, DeltaSeconds);
	}
	if (ANHCharacter* Me = Cast<ANHCharacter>(Pawn); Me && Me->Health <= 0.f)
	{
		// in a story mission, going down is the mission's to deal with: back to its checkpoint, nothing taken
		if (ANHMissions* Story = ANHMissions::Get(this); Story && Story->IsActive())
		{
			Me->Health = 1.f;
			Story->Fail(TEXT("You went down."));
		}
		else
		{
			PlayerDown(Pawn);
		}
	}
}

void ANHResponse::Decide(const FVector& Player, int32 Stars)
{
	if (Stars <= 0)
	{
		if (Coming != EComing::Nobody || bAlarm)
		{
			StandDown();
		}
		return;
	}
	const ENHArea Area = AreaAt(Player);
	// high-class streets: somebody who ran from it, within 80 m, has to stay alive long enough to make the call
	if (Area == ENHArea::HighClass && !bAlarm)
	{
		ANHPerson* Who = Caller.Get();
		if (!Who || Who->IsDown())
		{
			Caller.Reset();
			CallerFor = 0.f;
			for (TActorIterator<ANHPerson> It(GetWorld()); It; ++It)
			{
				if (!It->IsDown() && !It->bBrave && It->IsFleeing() && FVector::DistSquared(It->GetActorLocation(), Player) < FMath::Square(8000.f))
				{
					Caller = *It;
					break;
				}
			}
		}
		else
		{
			CallerFor += 0.5f;
			if (CallerFor >= WitnessSeconds)
			{
				bAlarm = true;
				ANHHUD::Floater(this, Who->GetActorLocation() + FVector(0.f, 0.f, 210.f), TEXT("Hello? Task Force?!"));
				ANHHUD::Toast(this, TEXT("Somebody don call Task Force"), 2);
			}
		}
	}
	// five stars: the army, anywhere, whoever was coming before. Those already here stay and fight beside them.
	if (Stars >= StarsArmy && Coming != EComing::Army)
	{
		Coming = EComing::Army;
		NextArrival = 5.f;
		ANHHUD::Toast(this, TEXT("Five stars. Army don enter the matter"), 2);
		UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: %s"), *Describe());
		return;
	}
	if (Coming != EComing::Nobody)
	{
		return; // whoever is coming keeps coming until the stars are gone
	}
	FString Station;
	float Distance = 0.f;
	const bool bStation = NearestStation(Player, Station, Distance);
	if (Area == ENHArea::AreaBoys)
	{
		if (Stars >= StarsBoys)
		{
			Coming = EComing::Boys;
			NextArrival = 3.f;
			ANHHUD::Toast(this, TEXT("No Task Force for this side. Area boys dey come for you"), 2);
		}
	}
	else if (Area == ENHArea::HighClass)
	{
		if (bAlarm)
		{
			Coming = EComing::TaskForce;
			NextArrival = 6.f;
			ANHHUD::Toast(this, TEXT("Task Force dey come"), 2);
		}
	}
	else if (bStation && ((Distance <= NearCm && Stars >= StarsNear) || (Distance <= FarCm && Stars >= StarsFar)))
	{
		Coming = EComing::TaskForce;
		NextArrival = FMath::Clamp(4.f + Distance / 25000.f, 4.f, 16.f); // the further the station, the longer they take
		ANHHUD::Toast(this, FString::Printf(TEXT("Task Force dey come from %s station"), *Station), 2);
	}
	if (Coming != EComing::Nobody)
	{
		UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: %s"), *Describe());
	}
}

void ANHResponse::Arrive(const FVector& Player)
{
	// on foot, from about 50 m off: the area boys. The Task Force and the army are driven in (SendRide).
	PutDown(FVector(ComeFrom(Player, 4500.f, 5800.f), Player.Z + 60.f), Coming);
}

FVector2D ANHResponse::ComeFrom(const FVector& Player, float Near, float Far) const
{
	const UNHGameData* Data = UNHGameData::Get(this);
	const float Angle = FMath::FRandRange(0.f, 2.f * UE_PI);
	FVector2D At = FVector2D(Player) + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * FMath::FRandRange(Near, Far);
	FNHRoadSeg Seg;
	FVector2D OnRoad;
	if (Data && Data->bRealCity && Data->NearestRoad(At, Seg, OnRoad) && FVector2D::Distance(OnRoad, FVector2D(Player)) > Near * 0.65f && FVector2D::Distance(OnRoad, At) < 6000.f)
	{
		At = OnRoad;
	}
	return At;
}

ANHPerson* ANHResponse::PutDown(const FVector& At, EComing Kind)
{
	ANHPerson* Body = GetWorld()->SpawnActor<ANHPerson>(ANHPerson::StaticClass(), At, FRotator::ZeroRotator);
	if (!Body)
	{
		return nullptr;
	}
	// the Task Force in black, soldiers in green; area boys in whatever they had on
	++Made;
	const FLinearColor Top = Kind == EComing::TaskForce ? FLinearColor(0.02f, 0.02f, 0.03f) : Kind == EComing::Army ? FLinearColor(0.12f, 0.17f, 0.08f)
		: FLinearColor::MakeFromHSV8(static_cast<uint8>(Made * 71), 150, 170);
	Body->Init(4000 + Made, Top, ENHCast::Man); // whoever comes for the player is a man
	// area boys: a T-shirt or a singlet, trousers or shorts, and slippers. The Task Force: black shirt, cargo trousers,
	// boots. Soldiers: camouflage, cargo trousers, boots.
	const FString Wearing = Kind == EComing::Army ? Body->Dress(Made, { TEXT("CamoTee") }, { TEXT("CargoTrousers") }, { TEXT("BlackBoots") })
		: Kind == EComing::TaskForce ? Body->Dress(Made, { TEXT("PlainTee"), TEXT("Polo") }, { TEXT("CargoTrousers") }, { TEXT("BlackBoots") })
		: Body->Dress(Made, { TEXT("PlainTee"), TEXT("LogoTee"), TEXT("CamoTee"), TEXT("MuscleShirt"), TEXT("Singlet") },
			{ TEXT("ClassicJeans"), TEXT("CargoTrousers"), TEXT("Shorts"), TEXT("DenimShorts") }, { TEXT("Slippers") });
	UE_LOG(LogNHGame, Verbose, TEXT("NAIJA HUSTLE: %s in %s"), Kind == EComing::Army ? TEXT("soldier") : Kind == EComing::TaskForce ? TEXT("Task Force") : TEXT("area boy"), *Wearing);
	Body->bBrave = true;
	Body->Health = Kind == EComing::Army ? 170.f : Kind == EComing::TaskForce ? 120.f : 90.f;
	FUnit Unit;
	Unit.Body = Body;
	Unit.Kind = Kind;
	Units.Add(Unit);
	return Body;
}

bool ANHResponse::RideAt(FVector& Out) const
{
	for (const FRide& Ride : Rides)
	{
		if (const ANHVehicle* Car = Ride.Car.Get())
		{
			Out = Car->GetActorLocation();
			return true;
		}
	}
	return false;
}

bool ANHResponse::TopAt(const FVector2D& At, float NearZ, const AActor* Ignore, float& OutZ) const
{
	FCollisionQueryParams Query(SCENE_QUERY_STAT(NHResponseTop), false, Ignore);
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Query.AddIgnoredActor(Pawn);
	}
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, FVector(At, NearZ + 6000.f), FVector(At, NearZ - 3000.f), ECC_Visibility, Query))
	{
		return false;
	}
	OutZ = Hit.ImpactPoint.Z;
	return true;
}

int32 ANHResponse::Aboard() const
{
	int32 Sum = 0;
	for (const FRide& Ride : Rides)
	{
		Sum += Ride.Aboard;
	}
	return Sum;
}

void ANHResponse::SendRide(const FVector& Player, int32 Men)
{
	// The Task Force come in a black mini van or a black pickup, turn about; the army in a green pickup or a truck.
	const bool bArmy = Coming == EComing::Army;
	const bool bFirst = RidesMade++ % 2 == 0;
	const FName Type = bArmy ? (bFirst ? FName(TEXT("armypick")) : FName(TEXT("armytruck"))) : (bFirst ? FName(TEXT("tfvan")) : FName(TEXT("tfblack")));
	// It starts on the street: somewhere open to the sky at about the height the player stands at, not on a roof.
	float Street = Player.Z, Top = 0.f;
	TopAt(FVector2D(Player), Player.Z, nullptr, Street);
	FVector2D At = FVector2D::ZeroVector;
	bool bFound = false;
	for (int32 Try = 0; Try < 24 && !bFound; ++Try)
	{
		At = ComeFrom(Player, Try < 12 ? 8000.f : 5000.f, Try < 12 ? 10500.f : 8000.f);
		bFound = TopAt(At, Street, nullptr, Top) && FMath::Abs(Top - Street) < 200.f;
		for (float Along = 300.f; bFound && Along <= 900.f; Along += 300.f) // and with room to set off toward the player
		{
			const FVector2D Ahead = At + (FVector2D(Player) - At).GetSafeNormal() * Along;
			bFound = TopAt(Ahead, Street, nullptr, Top) && FMath::Abs(Top - Street) < 200.f;
		}
	}
	if (!bFound)
	{
		--RidesMade;
		NextArrival = 1.5f; // nowhere to come from just now: try again shortly
		return;
	}
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Player.Y - At.Y, Player.X - At.X));
	const FTransform Where(FRotator(0.f, Yaw, 0.f), FVector(At, Street + 150.f));
	ANHVehicle* Car = GetWorld()->SpawnActorDeferred<ANHVehicle>(ANHVehicle::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Car)
	{
		return;
	}
	Car->VehicleType = Type;
	Car->Paint = bArmy ? FLinearColor(0.1f, 0.14f, 0.07f) : FLinearColor(0.012f, 0.012f, 0.016f);
	Car->Board = bArmy ? TEXT("ARMY") : TEXT("TASK FORCE");
	UGameplayStatics::FinishSpawningActor(Car, Where);
	Car->SetTraffic(true); // carried in by DriveRide, not by its own pedals
	Car->SetHeadlights(true);
	FRide Ride;
	Ride.Car = Car;
	Ride.At = At;
	Ride.Yaw = Yaw;
	Ride.Aboard = Men;
	Ride.Kind = Coming;
	Rides.Add(Ride);
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: response: a %s %s sent with %d aboard, from %.0f m"), bArmy ? TEXT("army") : TEXT("Task Force"), *Type.ToString(), Men,
		FVector2D::Distance(At, FVector2D(Player)) / 100.f);
}

void ANHResponse::DriveRide(FRide& Ride, APawn* Pawn, float DeltaSeconds)
{
	ANHVehicle* Car = Ride.Car.Get();
	if (Car->GetController() || Car->IsWrecked())
	{
		// taken by the player, or burnt out: nobody else gets down from it
		Ride.bStopped = true;
		Ride.Aboard = 0;
		return;
	}
	const FVector Here = Car->GetActorLocation(), Player = Pawn->GetActorLocation();
	if (!Ride.bStopped)
	{
		const FVector2D To = FVector2D(Player) - Ride.At;
		const float Distance = To.Size();
		const float Want = FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X));
		Ride.Yaw = FMath::FixedTurn(Ride.Yaw, Want, 110.f * DeltaSeconds);
		const FVector Fwd = FRotator(0.f, Ride.Yaw, 0.f).Vector();
		// pulls up 17 m short, or where something stands in the way
		FCollisionQueryParams Query(SCENE_QUERY_STAT(NHResponseRide), false, Car);
		Query.AddIgnoredActor(Pawn);
		FHitResult Hit;
		const FVector Nose = Here + FVector(0.f, 0.f, 40.f);
		bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Nose, Nose + Fwd * (Car->GetSpec().Length * 0.5f + 450.f), ECC_Visibility, Query);
		// nor does it climb onto anything, or drive off an edge: the ground just ahead has to be about level with the ground under it
		float Under = 0.f, Ahead = 0.f;
		if (!bBlocked && TopAt(Ride.At, Here.Z, Car, Under))
		{
			bBlocked = !TopAt(Ride.At + FVector2D(Fwd) * (Car->GetSpec().Length * 0.5f + 250.f), Here.Z, Car, Ahead) || FMath::Abs(Ahead - Under) > 90.f;
		}
		if (Distance < 1700.f || bBlocked)
		{
			Ride.bStopped = true;
			Ride.NextDown = 0.4f;
			Car->TrafficMove(Ride.At, Ride.Yaw, 0.f, DeltaSeconds);
			return;
		}
		const float Speed = FMath::Clamp((Distance - 1500.f) * 1.2f, 500.f, 1700.f); // braking as it comes up
		Ride.At += FVector2D(Fwd) * Speed * DeltaSeconds;
		Car->TrafficMove(Ride.At, Ride.Yaw, Speed, DeltaSeconds);
		return;
	}
	if (Ride.Aboard <= 0)
	{
		return;
	}
	Ride.NextDown -= DeltaSeconds;
	if (Ride.NextDown > 0.f)
	{
		return;
	}
	// the men get down one after another, from either side
	Ride.NextDown = 0.55f;
	const FRotator Facing(0.f, Ride.Yaw, 0.f);
	const float Side = Ride.Aboard % 2 ? 1.f : -1.f;
	const FVector Door = Here + Facing.RotateVector(FVector(FMath::FRandRange(-0.3f, 0.2f) * Car->GetSpec().Length, Side * (Car->GetSpec().Width * 0.5f + 110.f), 60.f));
	--Ride.Aboard;
	PutDown(Door, Ride.Kind);
}

void ANHResponse::Act(FUnit& Unit, APawn* Pawn, float DeltaSeconds)
{
	ANHPerson* Body = Unit.Body.Get();
	UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this);
	const FVector Player = Pawn->GetActorLocation(), Here = Body->GetActorLocation();
	const float Distance = FVector::Dist2D(Player, Here);
	const bool bSees = Distance < 6000.f && CanSee(Body, Pawn);
	bSeen |= bSees;
	const bool bArmy = Unit.Kind == EComing::Army, bForce = Unit.Kind != EComing::Boys;
	const float Reach = bArmy ? 2200.f : bForce ? 1400.f : 150.f;
	Unit.Wait -= DeltaSeconds;
	if (Distance > Reach || !bSees)
	{
		// closing in at a run; a moving car is chased as far as legs go
		if (Distance > (bForce ? 350.f : 120.f))
		{
			Body->WalkTo(Player, bForce ? 430.f : 470.f);
		}
		return;
	}
	Body->StopWalking();
	Body->FaceTowards(Player);
	if (Unit.Wait > 0.f)
	{
		return;
	}
	ANHCharacter* Me = Cast<ANHCharacter>(Pawn);
	ANHVehicle* Car = Cast<ANHVehicle>(Pawn);
	bool bHit = false;
	float Damage = 0.f;
	if (bArmy)
	{
		// rifles: quicker, from further off, and each round tells less than a pistol's
		Unit.Wait = FMath::FRandRange(0.3f, 0.65f);
		bHit = FMath::FRand() < (Distance < 1000.f ? 0.34f : 0.24f);
		Damage = 9.f;
		if (Audio)
		{
			const FVector Muzzle = Here + FVector(0.f, 0.f, 140.f);
			Audio->PlayShot(ENHShot::Rifle, Muzzle);
			Audio->PlayShot(ENHShot::RifleTail, Muzzle, ENHSoundKind::WeaponTail, 0.7f);
		}
	}
	else if (bForce)
	{
		Unit.Wait = FMath::FRandRange(0.9f, 1.5f);
		bHit = FMath::FRand() < (Distance < 800.f ? 0.4f : 0.28f);
		Damage = 10.f;
		if (Audio)
		{
			const FVector Muzzle = Here + FVector(0.f, 0.f, 140.f);
			Audio->PlayShot(ENHShot::Pistol, Muzzle);
			Audio->PlayShot(ENHShot::PistolTail, Muzzle, ENHSoundKind::WeaponTail, 0.7f);
		}
	}
	else
	{
		Unit.Wait = FMath::FRandRange(0.8f, 1.1f);
		bHit = FMath::FRand() < 0.6f;
		Damage = 16.f;
		if (Audio)
		{
			Audio->PlayShot(ENHShot::MacheteSwing, Here + FVector(0.f, 0.f, 120.f));
		}
	}
	if (!bHit)
	{
		return;
	}
	if (Me)
	{
		Me->Hurt(Damage);
		if (Audio)
		{
			Audio->PlayShot(ENHShot::BodyHit, Player, ENHSoundKind::Impact);
		}
	}
	else if (Car)
	{
		Car->Health = FMath::Max(0.f, Car->Health - Damage * 0.6f); // the car takes it for you
		if (Audio)
		{
			Audio->PlayShot(bForce ? ENHShot::BulletHit : ENHShot::MacheteHit, Player, ENHSoundKind::Impact);
		}
	}
}

void ANHResponse::StandDown()
{
	for (FUnit& Unit : Units)
	{
		if (ANHPerson* Body = Unit.Body.Get())
		{
			// they lose interest, walk off and are gone
			Body->WalkTo(Body->GetActorLocation() + (Body->GetActorLocation() - UGameplayStatics::GetPlayerPawn(this, 0)->GetActorLocation()).GetSafeNormal2D() * 4000.f, 160.f);
			Body->LifeLeft = 12.f;
		}
	}
	if (Coming != EComing::Nobody)
	{
		ANHHUD::Toast(this, Coming == EComing::Army ? TEXT("Army don lose you") : Coming == EComing::TaskForce ? TEXT("Task Force don lose you") : TEXT("Area boys don leave you"), 1);
	}
	for (FRide& Ride : Rides)
	{
		// what they came in is left where it stopped for a while: the player may be at its wheel
		if (ANHVehicle* Car = Ride.Car.Get(); Car && !Car->GetController())
		{
			Car->SetTraffic(false);
			Car->SetLifeSpan(45.f);
		}
	}
	Rides.Reset();
	Units.Reset();
	Coming = EComing::Nobody;
	bAlarm = false;
	bSeen = false;
	Caller.Reset();
	CallerFor = 0.f;
}

void ANHResponse::PlayerDown(APawn* Pawn)
{
	ANHCharacter* Me = Cast<ANHCharacter>(Pawn);
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const UNHGameData* Data = UNHGameData::Get(this);
	if (!Me || !Hustle)
	{
		return;
	}
	// a tenth of what you carry, 2,000 at least, or all of it if that is less
	const int32 Taken = FMath::Min(Hustle->Cash, FMath::Max(2000, Hustle->Cash / 10));
	const bool bForce = Coming != EComing::Boys;
	if (Taken > 0)
	{
		Hustle->Earn(-Taken, bForce ? TEXT("Bail") : TEXT("Robbed by area boys"));
	}
	ANHHUD::Toast(this, Coming == EComing::Army ? FString::Printf(TEXT("Army carry you go barracks. Bail: %s"), *UNHHustleSubsystem::Naira(Taken))
		: bForce ? FString::Printf(TEXT("Task Force arrest you. Bail: %s"), *UNHHustleSubsystem::Naira(Taken))
		: FString::Printf(TEXT("Area boys beat you and carry %s"), *UNHHustleSubsystem::Naira(Taken)), 2);
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: response: the player is down (%s), %d naira taken"), bForce ? TEXT("arrested") : TEXT("beaten"), Taken);
	for (FUnit& Unit : Units)
	{
		if (ANHPerson* Body = Unit.Body.Get())
		{
			Body->Destroy();
		}
	}
	Units.Reset();
	for (FRide& Ride : Rides)
	{
		if (ANHVehicle* Car = Ride.Car.Get(); Car && !Car->GetController())
		{
			Car->Destroy();
		}
	}
	Rides.Reset();
	Hustle->ClearHeat();
	StandDown();
	Me->Health = 100.f;
	if (!Me->Equipped().IsNone())
	{
		Me->Equip(Me->Equipped()); // whatever was in the hand is put away
	}
	if (Data)
	{
		// knocked down, not dead: they wake at the clinic (beside its bus stop; where the day began, in a level without that stop)
		FVector2D At = Data->Home;
		float Yaw = 0.f;
		if (const FNHBusStop* Stop = Data->Stops.Find(Data->ClinicStop))
		{
			const FVector2D Back = (Stop->Wait - Stop->Kerb).GetSafeNormal();
			At = Stop->Wait + Back * 500.f;
			Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Back.Y, -Back.X));
		}
		LastWokeAt = FVector(At, Me->GetActorLocation().Z);
		if (ANHPlayerController* PC = Cast<ANHPlayerController>(Me->GetController()))
		{
			PC->TravelTo(LastWokeAt, Yaw, FString::Printf(TEXT("You wake up at %s."), *Data->ClinicName));
		}
		else
		{
			Me->SetActorLocation(LastWokeAt, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}
