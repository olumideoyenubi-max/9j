#include "Vehicles/NHCarTheft.h"

#include "Core/NHGameData.h"
#include "Core/NHHustleSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gameplay/NHGameDirector.h"
#include "Gameplay/NHPerson.h"
#include "NaijaHustleGame.h"
#include "Phone/NHPhone.h"
#include "Player/NHCharacter.h"
#include "Player/NHPlayerController.h"
#include "UI/NHHUD.h"
#include "Vehicles/NHVehicle.h"

ANHCarTheft::ANHCarTheft()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ANHCarTheft* ANHCarTheft::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	TActorIterator<ANHCarTheft> It(World);
	return World && It ? *It : nullptr;
}

bool ANHCarTheft::Guards(const ANHVehicle* V) const
{
	return V && !V->bOwned && (V->HasNpcDriver() || V->Lock != ENHLock::Open);
}

FString ANHCarTheft::Prompt(const ANHVehicle* V) const
{
	if (V->HasNpcDriver())
	{
		return FString::Printf(TEXT("F  Pull the driver out: %s"), *V->DisplayName());
	}
	return FString::Printf(TEXT("F  Try the handle: %s"), *V->DisplayName());
}

int32 ANHCarTheft::Witnesses(const FVector& At, const AActor* Except)
{
	int32 N = 0, Filming = 0;
	for (TActorIterator<ANHPerson> It(GetWorld()); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), At) < 3500.f)
		{
			++N;
			if (Filming++ < 2)
			{
				ANHHUD::Floater(this, It->GetActorLocation() + FVector(0, 0, 210.f), TEXT("*holds up phone*"));
			}
		}
	}
	for (TActorIterator<ANHVehicle> It(GetWorld()); It; ++It)
	{
		if (*It != Except && It->HasNpcDriver() && FVector::Dist2D(It->GetActorLocation(), At) < 4500.f)
		{
			++N;
		}
	}
	return N;
}

void ANHCarTheft::Report(ANHVehicle* V, const TCHAR* What, float BaseHeat)
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const UNHGameData* Data = UNHGameData::Get(this);
	const int32 Seen = Witnesses(V->GetActorLocation(), V);
	// nobody saw: next to nothing. A crowd, or a car worth more than a house: a lot.
	const float Worth = V->Value() >= 100000000 ? 1.5f : V->Value() >= 40000000 ? 1.f : V->Value() >= 8000000 ? 0.5f : 0.f;
	const float Heat = Seen == 0 ? BaseHeat * 0.3f : BaseHeat + FMath::Min(Seen, 6) * 0.3f + Worth;
	if (Hustle)
	{
		Hustle->AddHeat(Heat);
	}
	if (Seen > 0)
	{
		const FString Where = Data ? Data->DistrictAt(V->GetActorLocation()) : TEXT("Lagos");
		if (ANHPhone* Phone = ANHPhone::Get(this))
		{
			static const TCHAR* Posts[] = { TEXT("Person just collect {car} for {where}! Broad daylight! I get video."), TEXT("{car} don disappear for {where} like magic. Owner still dey shout."),
				TEXT("If you see one {car} wey dey run from {where}, no be the owner dey drive am o.") };
			Phone->Post(TEXT("Lagos Street Gist"), TEXT("@LagosStreetGist"), FString(Posts[FMath::RandRange(0, 2)]).Replace(TEXT("{car}"), *V->DisplayName()).Replace(TEXT("{where}"), *Where), true);
		}
		ANHHUD::Toast(this, FString::Printf(TEXT("%d %s saw you %s"), Seen, Seen == 1 ? TEXT("person") : TEXT("people"), What), 2);
	}
}

void ANHCarTheft::Take(ANHVehicle* V)
{
	V->bStolen = true;
	V->HotLeft = 240.f + (V->Value() >= 40000000 ? 180.f : 0.f);
	V->Lock = ENHLock::Open; // yours to get in and out of now
	bToldTracker = false;
}

void ANHCarTheft::Approach(ANHVehicle* V)
{
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	ANHGameDirector* Dir = ANHGameDirector::Get(this);
	if (!PC || !V || !Dir || bHotwiring)
	{
		return;
	}
	if (V->HasNpcDriver())
	{
		if (FMath::Abs(V->Speed) > 350.f)
		{
			ANHHUD::Toast(this, TEXT("It is moving too fast. Stand in its way, or hail it."), 0);
			return;
		}
		Carjack(V);
		return;
	}
	switch (V->Lock)
	{
	case ENHLock::KeysIn:
		ANHHUD::Toast(this, TEXT("The engine is running and the keys dey inside. Somebody go cry today."), 1);
		Report(V, TEXT("drive off in it"), 0.6f);
		Take(V);
		PC->EnterVehicle(V);
		break;
	case ENHLock::Unlocked:
		ANHHUD::Toast(this, TEXT("The door is open, but no keys."), 0);
		if (PC->EnterVehicle(V))
		{
			StartHotwire(V);
		}
		break;
	case ENHLock::Locked:
	{
		const TWeakObjectPtr<ANHVehicle> Car(V);
		Dir->Panel = ANHGameDirector::FPanel();
		Dir->Panel.bOpen = true;
		Dir->Panel.Title = V->DisplayName() + TEXT(": locked");
		Dir->Panel.Lines = { V->bWindowBroken ? TEXT("The window is already broken.") : TEXT("The handle no gree open."), V->bTracker ? TEXT("A car like this will have a tracker.") : TEXT("") };
		Dir->Panel.Options = { V->bWindowBroken ? TEXT("Reach in and open it") : TEXT("Break the window (loud)"), TEXT("Leave it") };
		Dir->Panel.OnChoose = [this, Car](int32 Choice)
		{
			if (Choice == 0 && Car.IsValid())
			{
				BreakIn(Car.Get());
			}
		};
		break;
	}
	default:
		PC->EnterVehicle(V);
		break;
	}
}

void ANHCarTheft::BreakIn(ANHVehicle* V)
{
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!V->bWindowBroken)
	{
		V->bWindowBroken = true;
		ANHHUD::Floater(this, V->GetActorLocation() + FVector(0, 0, 180.f), TEXT("KRSSSH!"));
		V->SetAlarm(25.f);
		Report(V, TEXT("break into it"), 0.8f);
	}
	V->Lock = ENHLock::Unlocked;
	if (PC && PC->EnterVehicle(V))
	{
		StartHotwire(V);
	}
}

void ANHCarTheft::StartHotwire(ANHVehicle* V)
{
	Wiring = V;
	V->SetHeld(true); // sitting in it, going nowhere until the engine catches
	bHotwiring = true;
	Hits = 0;
	Marker = 0.f;
	MarkerDir = 1.f;
	TimeLeft = 12.f;
	ZoneLo = FMath::FRandRange(0.25f, 0.6f);
	ZoneHi = ZoneLo + 0.2f;
}

void ANHCarTheft::EndHotwire(bool bStarted)
{
	ANHVehicle* V = Wiring.Get();
	bHotwiring = false;
	Wiring = nullptr;
	if (!V)
	{
		return;
	}
	V->SetHeld(false);
	if (bStarted)
	{
		V->SetAlarm(0.f);
		Take(V);
		Report(V, TEXT("start it without a key"), 0.5f);
		ANHHUD::Toast(this, FString::Printf(TEXT("The %s don start. Move!"), *V->DisplayName()), 1);
	}
	else
	{
		// fumbled it: the alarm, and out you get
		V->SetAlarm(20.f);
		V->Lock = ENHLock::Unlocked;
		Report(V, TEXT("fumble under the dashboard"), 0.4f);
		if (ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController()))
		{
			PC->LeaveVehicle(true);
		}
		ANHHUD::Toast(this, TEXT("The wires spark, the alarm starts. E no gree."), 2);
	}
}

bool ANHCarTheft::Action()
{
	if (bHotwiring)
	{
		if (Marker >= ZoneLo && Marker <= ZoneHi)
		{
			if (++Hits >= 3)
			{
				EndHotwire(true);
			}
			else
			{
				// the next wire: a narrower gap somewhere else
				ZoneLo = FMath::FRandRange(0.15f, 0.7f);
				ZoneHi = ZoneLo + 0.2f - 0.035f * Hits;
			}
		}
		else
		{
			TimeLeft -= 2.5f; // a wrong wire costs time
			ANHHUD::Toast(this, TEXT("Wrong wire!"), 2);
		}
		return true;
	}
	const ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	ANHVehicle* Car = PC ? Cast<ANHVehicle>(PC->GetPawn()) : nullptr;
	const FNHPlace* Place = PlaceHere();
	const ANHGameDirector* Dir = ANHGameDirector::Get(this);
	if (Place && Car && Dir && !Dir->IsBusy())
	{
		OpenPlace(*Place, Car);
		return true;
	}
	return false;
}

const FNHPlace* ANHCarTheft::PlaceHere() const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const ANHVehicle* Car = PC ? Cast<ANHVehicle>(PC->GetPawn()) : nullptr;
	const UNHGameData* Data = UNHGameData::Get(this);
	if (!Car || !Data || FMath::Abs(Car->Speed) > 200.f)
	{
		return nullptr;
	}
	for (const FNHPlace& Place : Data->Places)
	{
		if (FVector2D::Distance(Place.Pos, FVector2D(Car->GetActorLocation())) < 2500.f)
		{
			return &Place;
		}
	}
	return nullptr;
}

FString ANHCarTheft::ActionPrompt() const
{
	if (bHotwiring)
	{
		return TEXT("E  Join the wires when the marker is in the green");
	}
	const FNHPlace* Place = PlaceHere();
	return Place ? TEXT("E  ") + Place->Name : FString();
}

void ANHCarTheft::OpenPlace(const FNHPlace& Place, ANHVehicle* CarIn)
{
	ANHGameDirector* Dir = ANHGameDirector::Get(this);
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const TWeakObjectPtr<ANHVehicle> Car(CarIn);
	const FString Name = CarIn->DisplayName();
	Dir->Panel = ANHGameDirector::FPanel();
	Dir->Panel.bOpen = true;
	Dir->Panel.Title = Place.Name;
	if (Place.Id == TEXT("mechanic"))
	{
		Dir->Panel.Lines = { CarIn->bTracker ? TEXT("\"This one get tracker. I fit comot am, but e no cheap.\"") : TEXT("\"No tracker for this one. Wetin else?\"") };
		Dir->Panel.Options = { CarIn->bTracker ? TEXT("Take the tracker out (N15,000)") : TEXT("Fix it up (N4,000)"), TEXT("Leave") };
		Dir->Panel.OnChoose = [this, Car, Hustle](int32 Choice)
		{
			if (Choice != 0 || !Car.IsValid() || !Hustle)
			{
				return;
			}
			const int32 Fee = Car->bTracker ? 15000 : 4000;
			if (Hustle->Cash < Fee)
			{
				ANHHUD::Toast(this, TEXT("\"Come back when you get money.\""), 2);
				return;
			}
			Hustle->Earn(-Fee, Car->bTracker ? TEXT("Mechanic: tracker removed") : TEXT("Mechanic: repairs"));
			if (Car->bTracker)
			{
				Car->bTracker = false;
				ANHHUD::Toast(this, TEXT("The tracker is out. Nobody is following this car any more."), 1);
			}
			else
			{
				Car->Repair();
				ANHHUD::Toast(this, TEXT("Good as new. Almost."), 1);
			}
		};
	}
	else if (Place.Id == TEXT("paint"))
	{
		Dir->Panel.Lines = { CarIn->bStolen ? TEXT("\"New colour, new plate number, no question. By evening nobody go know am.\"") : TEXT("\"You wan change colour? No wahala.\"") };
		Dir->Panel.Options = { TEXT("Respray and new plates (N8,000)"), TEXT("Leave") };
		Dir->Panel.OnChoose = [this, Car, Hustle](int32 Choice)
		{
			if (Choice != 0 || !Car.IsValid() || !Hustle)
			{
				return;
			}
			if (Hustle->Cash < 8000)
			{
				ANHHUD::Toast(this, TEXT("\"Paint no be water. Eight thousand.\""), 2);
				return;
			}
			Hustle->Earn(-8000, TEXT("Baba Colour: respray"));
			const bool bWasHot = Car->bStolen;
			Car->bStolen = false;
			Car->HotLeft = 0.f;
			Car->bOwned = true;
			Car->bPlayerOwned = true;
			Car->bWindowBroken = false;
			if (bWasHot)
			{
				Hustle->ClearHeat();
			}
			ANHHUD::Toast(this, FString::Printf(TEXT("Resprayed. The %s is yours now: the car keys lock and unlock it."), *Car->DisplayName()), 1);
		};
	}
	else
	{
		// the chop shop: by what it is worth and what state it is in; not while it is hot
		const bool bTooHot = CarIn->bStolen && (CarIn->bTracker || CarIn->HotLeft > 120.f);
		const int32 Offer = FMath::RoundToInt(CarIn->Value() * 0.02f * FMath::Clamp(CarIn->Health / FMath::Max(CarIn->MaxHealth, 1.f), 0.15f, 1.f) * (CarIn->bWindowBroken ? 0.85f : 1.f) / 1000.f) * 1000;
		Dir->Panel.Lines = { bTooHot ? (CarIn->bTracker ? TEXT("\"Tracker dey this motor! Carry am comot before dem follow you come here.\"") : TEXT("\"This one still dey hot. Police dey find am. Come back later, or respray am.\""))
			: FString::Printf(TEXT("\"%s... I go give you %s. Take am or leave am.\""), *Name, *UNHHustleSubsystem::Naira(Offer)) };
		Dir->Panel.Options = bTooHot ? TArray<FString>{ TEXT("Leave") } : TArray<FString>{ FString::Printf(TEXT("Sell it for %s"), *UNHHustleSubsystem::Naira(Offer)), TEXT("Leave") };
		Dir->Panel.OnChoose = [this, Car, Hustle, Offer, bTooHot](int32 Choice)
		{
			ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
			if (Choice != 0 || bTooHot || !Car.IsValid() || !Hustle || !PC)
			{
				return;
			}
			const FString Sold = Car->DisplayName();
			PC->LeaveVehicle(true);
			Car->Destroy();
			Hustle->Earn(Offer, TEXT("Chop shop: ") + Sold);
			ANHHUD::Toast(this, FString::Printf(TEXT("Sold the %s. By tomorrow e don turn to spare parts."), *Sold), 1);
		};
	}
}

bool ANHCarTheft::UseKeys(ANHVehicle* V)
{
	const APawn* Pawn = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
	if (!V || !V->bOwned || V == Pawn || !Pawn || FVector::Dist2D(Pawn->GetActorLocation(), V->GetActorLocation()) > 6000.f)
	{
		return false;
	}
	// the remote: a beep and a flash of the lights; locked, nobody else gets in, and your F opens it again
	const bool bLock = V->Lock == ENHLock::Open;
	V->Lock = bLock ? ENHLock::Locked : ENHLock::Open;
	V->SetHeadlights(true);
	V->SetAlarm(0.f);
	FTimerHandle Off;
	GetWorldTimerManager().SetTimer(Off, FTimerDelegate::CreateWeakLambda(V, [V]() { V->SetHeadlights(false); }), 0.5f, false);
	ANHHUD::Floater(this, V->GetActorLocation() + FVector(0, 0, 200.f), bLock ? TEXT("BIP!") : TEXT("BIP BIP!"));
	ANHHUD::Toast(this, FString::Printf(TEXT("%s %s"), *V->DisplayName(), bLock ? TEXT("locked") : TEXT("unlocked")), 0);
	return true;
}

// ----------------------------------------------------------------------------------------------- carjacking
void ANHCarTheft::Throw(ANHVehicle* Car, EAfter Kind, const FString& Name, const FLinearColor& Shirt)
{
	const FVector Out = Car->ExitPoint() + Car->GetActorRightVector() * FMath::FRandRange(-60.f, 60.f);
	ANHPerson* P = GetWorld()->SpawnActor<ANHPerson>(ANHPerson::StaticClass(), Out, FRotator::ZeroRotator);
	if (!P)
	{
		return;
	}
	P->Init(FMath::RandRange(1, 500), Shirt, Kind == EAfter::Chases ? ENHCast::Man : ENHCast::Anyone); // an owner who comes after you is a man
	FAngry A;
	A.Person = P;
	A.Car = Car;
	A.Kind = Kind;
	A.Name = Name;
	A.T = Kind == EAfter::Chases ? 14.f : Kind == EAfter::Calls ? 5.f : 8.f;
	Angry.Add(A);
	static const TCHAR* Shouts[] = { TEXT("THIEF! OLE!"), TEXT("My motor! Ah!"), TEXT("Jesu! Somebody help!"), TEXT("You dey craze?!") };
	ANHHUD::Floater(this, Out + FVector(0, 0, 200.f), Shouts[FMath::RandRange(0, 3)]);
	if (Kind == EAfter::Runs)
	{
		P->WalkTo(Out - Car->GetActorForwardVector() * 3000.f + Car->GetActorRightVector() * FMath::FRandRange(-1500.f, 1500.f), 430.f);
		P->LifeLeft = 9.f;
	}
}

void ANHCarTheft::Carjack(ANHVehicle* V)
{
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!PC)
	{
		return;
	}
	// who was driving decides what they do about it
	const FName Type = V->VehicleType;
	const bool bCommercial = Type == TEXT("danfo") || Type == TEXT("keke") || Type == TEXT("okada");
	const bool bLuxury = V->Value() >= 40000000;
	const float Roll = FMath::FRand();
	const EAfter Kind = bLuxury ? (Roll < 0.6f ? EAfter::Calls : EAfter::Runs) : bCommercial ? (Roll < 0.7f ? EAfter::Chases : EAfter::Calls) : (Roll < 0.4f ? EAfter::Runs : Roll < 0.75f ? EAfter::Chases : EAfter::Calls);
	V->bTracker = bLuxury;
	Report(V, TEXT("drag the driver out"), 1.f);
	Throw(V, Kind, TEXT("The driver"), FLinearColor(0.85f, 0.83f, 0.78f));
	if (Type == TEXT("danfo") || Type == TEXT("keke"))
	{
		Throw(V, EAfter::Chases, TEXT("The conductor"), FLinearColor(0.75f, 0.14f, 0.05f)); // he comes after his bus whatever the driver does
	}
	Take(V);
	PC->EnterVehicle(V); // the seat is the player's: the driver's body goes with SetOccupied
}

void ANHCarTheft::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	ANHVehicle* Mine = PC ? Cast<ANHVehicle>(PC->GetPawn()) : nullptr;

	if (bHotwiring)
	{
		if (!Wiring.IsValid() || Mine != Wiring.Get())
		{
			// got out, or the car is gone
			if (Wiring.IsValid())
			{
				Wiring->SetHeld(false);
			}
			bHotwiring = false;
			Wiring = nullptr;
		}
		else
		{
			// the marker sweeps back and forth, quicker with each wire joined
			Marker += MarkerDir * DeltaSeconds * (0.9f + 0.25f * Hits);
			if (Marker >= 1.f || Marker <= 0.f)
			{
				Marker = FMath::Clamp(Marker, 0.f, 1.f);
				MarkerDir = -MarkerDir;
			}
			TimeLeft -= DeltaSeconds;
			if (TimeLeft <= 0.f)
			{
				EndHotwire(false);
			}
		}
	}

	// in a stolen car: it cools with time, unless it carries a tracker, which keeps the wanted level topped up
	if (Mine && Mine->bStolen)
	{
		Mine->HotLeft = FMath::Max(0.f, Mine->HotLeft - DeltaSeconds);
		if (Mine->bTracker && Hustle)
		{
			if (!bToldTracker)
			{
				bToldTracker = true;
				ANHHUD::Toast(this, TEXT("This car has a tracker: the Task Force can follow it. The mechanic in the Mechanic Village can take it out."), 2);
			}
			TrackerBeat -= DeltaSeconds;
			if (TrackerBeat <= 0.f)
			{
				TrackerBeat = 12.f;
				if (Hustle->Stars() < 3)
				{
					Hustle->AddHeat(1.f);
				}
			}
		}
	}

	for (int32 I = Angry.Num() - 1; I >= 0; --I)
	{
		FAngry& A = Angry[I];
		ANHPerson* P = A.Person.Get();
		ANHVehicle* Car = A.Car.Get();
		A.T -= DeltaSeconds;
		if (!P || !Car)
		{
			Angry.RemoveAtSwap(I);
			continue;
		}
		if (A.Kind == EAfter::Chases)
		{
			A.Think -= DeltaSeconds;
			if (A.Think <= 0.f)
			{
				A.Think = 0.3f;
				P->WalkTo(Car->GetActorLocation(), 520.f);
			}
			if (Car == Mine && FVector::Dist2D(P->GetActorLocation(), Car->GetActorLocation()) < Car->GetSpec().Length * 0.5f + 160.f && FMath::Abs(Car->Speed) < 250.f && !bHotwiring)
			{
				// caught up with it standing: out you come, and the door is locked behind you
				PC->LeaveVehicle(true);
				Car->Lock = ENHLock::Locked;
				Car->bStolen = false;
				ANHHUD::Toast(this, FString::Printf(TEXT("%s drag you comot from the %s!"), *A.Name, *Car->DisplayName()), 2);
				ANHHUD::Floater(this, P->GetActorLocation() + FVector(0, 0, 200.f), TEXT("Comot for my motor!"));
				P->StopWalking();
				P->LifeLeft = 8.f;
				Angry.RemoveAtSwap(I);
				continue;
			}
			if (A.T <= 0.f)
			{
				// lost him: he reports it instead
				if (Hustle)
				{
					Hustle->AddHeat(0.5f);
				}
				P->StopWalking();
				P->LifeLeft = 6.f;
				Angry.RemoveAtSwap(I);
			}
		}
		else if (A.T <= 0.f)
		{
			if (A.Kind == EAfter::Calls && Hustle)
			{
				Hustle->AddHeat(1.5f);
				ANHHUD::Toast(this, FString::Printf(TEXT("%s don call the Task Force"), *A.Name), 2);
				ANHHUD::Floater(this, P->GetActorLocation() + FVector(0, 0, 200.f), TEXT("Hello? Task Force?!"));
				P->LifeLeft = 8.f;
			}
			Angry.RemoveAtSwap(I);
		}
	}
}

void ANHCarTheft::Debug(const FString& What)
{
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	ANHCharacter* Char = PC ? Cast<ANHCharacter>(PC->GetPawn()) : nullptr;
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const UNHGameData* Data = UNHGameData::Get(this);
	if (!Char || !Hustle)
	{
		return;
	}
	if (ANHGameDirector* Dir = ANHGameDirector::Get(this))
	{
		Dir->Dialogue = ANHGameDirector::FDialogue(); // Baba's greeting would cover what is being looked at
	}
	const auto Nearest = [this, Char](TFunctionRef<bool(const ANHVehicle*)> Want)
	{
		ANHVehicle* Best = nullptr;
		for (TActorIterator<ANHVehicle> It(GetWorld()); It; ++It)
		{
			if (Want(*It) && (!Best || FVector::DistSquared(It->GetActorLocation(), Char->GetActorLocation()) < FVector::DistSquared(Best->GetActorLocation(), Char->GetActorLocation())))
			{
				Best = *It;
			}
		}
		return Best;
	};
	const float Before = Hustle->Heat;
	if (What == TEXT("roll"))
	{
		Char->Roll();
		UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: test roll: rolling %d"), Char->IsRolling());
		return;
	}
	if (What == TEXT("climb"))
	{
		ANHVehicle* V = Nearest([](const ANHVehicle* X) { return !X->IsTraffic() && !X->GetSpec().bBike; });
		if (V)
		{
			const FVector Side = V->GetActorLocation() - V->GetActorRightVector() * (V->GetSpec().Width * 0.5f + 70.f);
			Char->SetActorLocationAndRotation(FVector(Side.X, Side.Y, V->GetActorLocation().Z + 60.f), FRotator(0.f, V->GetActorRightVector().Rotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation(FRotator(-15.f, V->GetActorRightVector().Rotation().Yaw, 0.f));
			const float Z = Char->GetActorLocation().Z;
			Char->Jump();
			UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: test climb: beside a %s at z %.0f, climbing %d"), *V->DisplayName(), Z, Char->IsClimbing());
			FTimerHandle Done;
			GetWorldTimerManager().SetTimer(Done, FTimerDelegate::CreateWeakLambda(this, [Char, Z]() { UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: test climb: now at z %.0f, %.0f cm higher"), Char->GetActorLocation().Z, Char->GetActorLocation().Z - Z); }), 1.6f, false);
		}
		return;
	}
	if (What == TEXT("carjack"))
	{
		if (ANHVehicle* V = Nearest([](const ANHVehicle* X) { return X->HasNpcDriver(); }))
		{
			Char->SetActorLocation(V->ExitPoint(), false, nullptr, ETeleportType::TeleportPhysics);
			const FString Name = V->DisplayName();
			Carjack(V);
			UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: test carjack: %s, player driving %d, stolen %d, tracker %d, %d people after you, heat %.1f -> %.1f"), *Name, PC->GetPawn() == V, V->bStolen, V->bTracker, Angry.Num(), Before, Hustle->Heat);
		}
		return;
	}
	ANHVehicle* V = Nearest([](const ANHVehicle* X) { return X->Lock == ENHLock::Locked; });
	if (!V)
	{
		UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: test %s: no locked car about"), *What);
		return;
	}
	Char->SetActorLocation(V->ExitPoint(), false, nullptr, ETeleportType::TeleportPhysics);
	BreakIn(V);
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: test steal: %s window broken %d, alarm %d, hotwiring %d, player inside %d, heat %.1f -> %.1f"), *V->DisplayName(), V->bWindowBroken, V->AlarmOn(), bHotwiring, PC->GetPawn() == V, Before, Hustle->Heat);
	if (What == TEXT("hotwin") || What == TEXT("sell"))
	{
		EndHotwire(true);
		UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: test hotwire: started, stolen %d, hot for %.0f s, alarm %d, heat %.1f"), V->bStolen, V->HotLeft, V->AlarmOn(), Hustle->Heat);
	}
	if (What == TEXT("sell") && Data && Data->FindPlace(TEXT("chop")))
	{
		const FNHPlace* Chop = Data->FindPlace(TEXT("chop"));
		V->HotLeft = 60.f; // cooled enough for him to take it
		V->bTracker = false;
		V->SetActorLocation(FVector(Chop->Pos.X, Chop->Pos.Y, 200.f), false, nullptr, ETeleportType::TeleportPhysics);
		const int32 Cash = Hustle->Cash;
		OpenPlace(*Chop, V);
		if (ANHGameDirector* Dir = ANHGameDirector::Get(this))
		{
			UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: test sell: at %s: %s / option: %s"), *Dir->Panel.Title, *Dir->Panel.Lines[0], *Dir->Panel.Options[0]);
			FTimerHandle Sell;
			GetWorldTimerManager().SetTimer(Sell, FTimerDelegate::CreateWeakLambda(this, [this, Hustle, Cash]()
			{
				if (ANHGameDirector* D = ANHGameDirector::Get(this))
				{
					D->OnChoice(0);
				}
				UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: test sell: cash %d -> %d, player on foot %d"), Cash, Hustle->Cash, Cast<ANHCharacter>(GetWorld()->GetFirstPlayerController()->GetPawn()) != nullptr);
			}), 4.f, false);
		}
	}
}
