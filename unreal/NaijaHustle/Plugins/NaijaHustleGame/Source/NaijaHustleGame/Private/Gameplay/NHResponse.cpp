#include "Gameplay/NHResponse.h"

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
	return FString::Printf(TEXT("response: %s; %d stars; %s%s; %d here%s"), *AreaReport(), Hustle ? Hustle->Stars() : 0,
		Coming == EComing::TaskForce ? TEXT("Task Force coming") : Coming == EComing::Boys ? TEXT("area boys coming") : TEXT("nobody coming"),
		bAlarm ? TEXT(" (a witness called)") : TEXT(""), Units.Num(), bSeen ? TEXT(", and they see you") : TEXT(""));
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
		const int32 Want = Coming == EComing::Boys ? FMath::Min(Stars + 1, 6) : Stars >= 5 ? 6 : Stars;
		if (NextArrival <= 0.f && Units.Num() < Want)
		{
			NextArrival = 2.5f;
			Arrive(Player);
		}
	}
	bSeen = false;
	for (int32 I = Units.Num() - 1; I >= 0; --I)
	{
		ANHPerson* Body = Units[I].Body.Get();
		if (!Body || Body->IsDown())
		{
			if (Coming == EComing::TaskForce)
			{
				Hustle->AddHeat(1.5f); // an officer down: they send more
			}
			Units.RemoveAtSwap(I);
			continue;
		}
		Act(Units[I], Pawn, DeltaSeconds);
	}
	if (const ANHCharacter* Me = Cast<ANHCharacter>(Pawn); Me && Me->Health <= 0.f)
	{
		PlayerDown(Pawn);
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
	// from about 50 m off, on a road where there is one that way
	const UNHGameData* Data = UNHGameData::Get(this);
	const float Angle = FMath::FRandRange(0.f, 2.f * UE_PI);
	FVector2D At = FVector2D(Player) + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * FMath::FRandRange(4500.f, 5800.f);
	FNHRoadSeg Seg;
	FVector2D OnRoad;
	if (Data && Data->bRealCity && Data->NearestRoad(At, Seg, OnRoad) && FVector2D::Distance(OnRoad, FVector2D(Player)) > 3000.f && FVector2D::Distance(OnRoad, At) < 6000.f)
	{
		At = OnRoad;
	}
	ANHPerson* Body = GetWorld()->SpawnActor<ANHPerson>(ANHPerson::StaticClass(), FVector(At, Player.Z + 60.f), FRotator::ZeroRotator);
	if (!Body)
	{
		return;
	}
	const bool bForce = Coming == EComing::TaskForce;
	// the Task Force in black; area boys in whatever they had on
	++Made;
	Body->Init(4001 + 61 * Made, bForce ? FLinearColor(0.02f, 0.02f, 0.03f) : FLinearColor::MakeFromHSV8(static_cast<uint8>(Made * 71), 150, 170));
	Body->bBrave = true;
	Body->Health = bForce ? 120.f : 90.f;
	Units.Add({ Body, 1.f });
}

void ANHResponse::Act(FUnit& Unit, APawn* Pawn, float DeltaSeconds)
{
	ANHPerson* Body = Unit.Body.Get();
	UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this);
	const FVector Player = Pawn->GetActorLocation(), Here = Body->GetActorLocation();
	const float Distance = FVector::Dist2D(Player, Here);
	const bool bSees = Distance < 6000.f && CanSee(Body, Pawn);
	bSeen |= bSees;
	const bool bForce = Coming == EComing::TaskForce;
	const float Reach = bForce ? 1400.f : 150.f;
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
	if (bForce)
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
		ANHHUD::Toast(this, Coming == EComing::TaskForce ? TEXT("Task Force don lose you") : TEXT("Area boys don leave you"), 1);
	}
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
	const bool bForce = Coming == EComing::TaskForce;
	if (Taken > 0)
	{
		Hustle->Earn(-Taken, bForce ? TEXT("Bail") : TEXT("Robbed by area boys"));
	}
	ANHHUD::Toast(this, bForce ? FString::Printf(TEXT("Task Force arrest you. Bail: %s"), *UNHHustleSubsystem::Naira(Taken))
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
	Hustle->ClearHeat();
	StandDown();
	Me->Health = 100.f;
	if (!Me->Equipped().IsNone())
	{
		Me->Equip(Me->Equipped()); // whatever was in the hand is put away
	}
	if (Data)
	{
		// back where the day began, on whatever ground is there
		FHitResult Floor;
		const FVector Above(Data->Home, Me->GetActorLocation().Z + 5000.f);
		const bool bFloor = GetWorld()->LineTraceSingleByChannel(Floor, Above, Above - FVector(0.f, 0.f, 20000.f), ECC_Visibility);
		Me->SetActorLocation(bFloor ? Floor.ImpactPoint + FVector(0.f, 0.f, 100.f) : FVector(Data->Home, Me->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
	}
}
