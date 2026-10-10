#include "Gameplay/NHLeads.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/NHGameData.h"
#include "Core/NHHustleSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/NHGameDirector.h"
#include "Gameplay/NHMissions.h"
#include "Gameplay/NHPerson.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "NaijaHustleGame.h"
#include "Player/NHCharacter.h"
#include "Player/NHPlayerController.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UI/NHHUD.h"
#include "Vehicles/NHTraffic.h"
#include "Vehicles/NHVehicle.h"
#include "Vehicles/NHVehicleDynamicsComponent.h"
#include "World/NHShapes.h"

namespace
{
	/** A lead left standing is only a body while the player is this near (cm); the city beyond is not loaded to stand on */
	const float StandInNear = 15000.f, StandInFar = 20000.f;
	/** Cameras and lights are kept within this of the player, and no more than this many */
	const float HackPointFar = 25000.f;
	const int32 HackPointMax = 4;

	FLinearColor Hex(const FString& Text, const FLinearColor& Default)
	{
		return Text.StartsWith(TEXT("#")) && Text.Len() == 7 ? FLinearColor::FromSRGBColor(FColor::FromHex(Text)) : Default;
	}

	FNHSurface Paint(const FLinearColor& Color, float Glow = 0.f)
	{
		FNHSurface S;
		S.Color = Color;
		S.Glow = Glow;
		return S;
	}
}

// ---------------------------------------------------------------------------------------------------- cameras and lights
ANHHackPoint::ANHHackPoint()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ANHHackPoint::Build(ENHHackKind InKind)
{
	Kind = InKind;
	const FNHSurface Steel = Paint(FLinearColor(0.16f, 0.17f, 0.18f)), Dark = Paint(FLinearColor(0.03f, 0.03f, 0.035f));
	if (Kind == ENHHackKind::Camera)
	{
		NHShapes::AddPiece(this, RootComponent, ENHShape::Cylinder, FVector(0.f, 0.f, 210.f), FVector(14.f, 14.f, 420.f), Steel);
		NHShapes::AddPiece(this, RootComponent, ENHShape::Box, FVector(22.f, 0.f, 420.f), FVector(62.f, 26.f, 24.f), Paint(FLinearColor(0.85f, 0.85f, 0.82f)), FRotator(-18.f, 0.f, 0.f));
		// the lamp that shows it is recording
		Lamp = NHShapes::AddPiece(this, RootComponent, ENHShape::Sphere, FVector(54.f, 0.f, 412.f), FVector(9.f, 9.f, 9.f), Paint(FLinearColor(1.f, 0.05f, 0.03f), 6.f));
	}
	else
	{
		NHShapes::AddPiece(this, RootComponent, ENHShape::Cylinder, FVector(0.f, 0.f, 230.f), FVector(16.f, 16.f, 460.f), Steel);
		NHShapes::AddPiece(this, RootComponent, ENHShape::Box, FVector(0.f, 0.f, 515.f), FVector(38.f, 40.f, 112.f), Dark);
		Lamp = NHShapes::AddPiece(this, RootComponent, ENHShape::Sphere, FVector(20.f, 0.f, 548.f), FVector(24.f, 24.f, 24.f), Paint(FLinearColor(0.25f, 0.02f, 0.02f)));
		NHShapes::AddPiece(this, RootComponent, ENHShape::Sphere, FVector(20.f, 0.f, 515.f), FVector(24.f, 24.f, 24.f), Paint(FLinearColor(0.25f, 0.17f, 0.02f)));
		Green = NHShapes::AddPiece(this, RootComponent, ENHShape::Sphere, FVector(20.f, 0.f, 482.f), FVector(24.f, 24.f, 24.f), Paint(FLinearColor(0.05f, 1.f, 0.2f), 5.f));
	}
	// pieces for looking at: nothing drives into them or stands on them
	TInlineComponentArray<UStaticMeshComponent*> Pieces(this);
	for (UStaticMeshComponent* Piece : Pieces)
	{
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

FVector ANHHackPoint::Head() const
{
	return GetActorLocation() + FVector(0.f, 0.f, Kind == ENHHackKind::Camera ? 420.f : 515.f);
}

bool ANHHackPoint::IsHacked() const
{
	return GetWorld() && GetWorld()->GetTimeSeconds() < HackedUntil;
}

void ANHHackPoint::Hack(float Seconds)
{
	HackedUntil = GetWorld()->GetTimeSeconds() + Seconds;
	Show(true);
}

void ANHHackPoint::Show(bool bHacked)
{
	bShownHacked = bHacked;
	if (Kind == ENHHackKind::Camera)
	{
		// recording: the red lamp. Unlocked: out.
		NHShapes::SetSurface(Lamp, bHacked ? Paint(FLinearColor(0.05f, 0.05f, 0.05f)) : Paint(FLinearColor(1.f, 0.05f, 0.03f), 6.f));
	}
	else
	{
		NHShapes::SetSurface(Lamp, bHacked ? Paint(FLinearColor(1.f, 0.04f, 0.03f), 6.f) : Paint(FLinearColor(0.25f, 0.02f, 0.02f)));
		NHShapes::SetSurface(Green, bHacked ? Paint(FLinearColor(0.02f, 0.2f, 0.05f)) : Paint(FLinearColor(0.05f, 1.f, 0.2f), 5.f));
	}
}

void ANHHackPoint::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bShownHacked && !IsHacked())
	{
		Show(false);
	}
}

// ---------------------------------------------------------------------------------------------------- the cast
ANHLeads::ANHLeads()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ANHLeads* ANHLeads::Get(const UObject* WorldContext)
{
	return WorldContext ? ::Cast<ANHLeads>(UGameplayStatics::GetActorOfClass(WorldContext, ANHLeads::StaticClass())) : nullptr;
}

bool ANHLeads::Load()
{
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *(UNHGameData::DataDir() / TEXT("characters.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
	{
		UE_LOG(LogNHGame, Warning, TEXT("NAIJA HUSTLE: leads: Data/characters.json could not be read; nobody to switch to"));
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (Root->TryGetArrayField(TEXT("characters"), List))
	{
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject>* J = nullptr;
			if (!V->TryGetObject(J))
			{
				continue;
			}
			FMember M;
			M.Id = FName(*(*J)->GetStringField(TEXT("id")));
			M.Name = (*J)->GetStringField(TEXT("name"));
			M.Age = static_cast<int32>((*J)->GetNumberField(TEXT("age")));
			M.From = (*J)->GetStringField(TEXT("from"));
			M.Role = (*J)->GetStringField(TEXT("role"));
			M.Stop = FName(*(*J)->GetStringField(TEXT("stop")));
			M.Skin = FName(*(*J)->GetStringField(TEXT("skin")));
			const FString Ability = (*J)->GetStringField(TEXT("ability"));
			M.Ability = Ability.IsEmpty() ? NAME_None : FName(*Ability);
			M.AbilityName = (*J)->GetStringField(TEXT("abilityName"));
			M.Unlock = (*J)->GetStringField(TEXT("unlock"));
			M.Colour = Hex((*J)->GetStringField(TEXT("colour")), FLinearColor::White);
			(*J)->TryGetStringArrayField(TEXT("districts"), M.Districts);
			Crew.Add(M);
		}
	}
	TArray<FString> LeadIds;
	Root->TryGetStringArrayField(TEXT("leads"), LeadIds);
	for (const FString& Id : LeadIds)
	{
		if (Find(FName(*Id)))
		{
			Leads.Add(FName(*Id));
		}
	}

	// the numbers: naija_rules.json "abilities"
	TSharedPtr<FJsonObject> Rules;
	if (FFileHelper::LoadFileToString(Text, *(UNHGameData::DataDir() / TEXT("naija_rules.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Rules) && Rules)
	{
		const TSharedPtr<FJsonObject>* A = nullptr;
		if (Rules->TryGetObjectField(TEXT("abilities"), A))
		{
			const auto Num = [](const TSharedPtr<FJsonObject>& O, const TCHAR* Key, float& Out) { double V = 0.0; if (O->TryGetNumberField(Key, V)) { Out = static_cast<float>(V); } };
			Num(*A, TEXT("switchSeconds"), SwitchSeconds);
			Num(*A, TEXT("switchHeight"), SwitchHeight);
			const TSharedPtr<FJsonObject>* R = nullptr;
			if ((*A)->TryGetObjectField(TEXT("hustleRush"), R))
			{
				Num(*R, TEXT("fillSeconds"), RushFill);
				Num(*R, TEXT("seconds"), RushSeconds);
				Num(*R, TEXT("speed"), RushSpeed);
				Num(*R, TEXT("damageTaken"), RushDamage);
				Num(*R, TEXT("vehiclePull"), RushPull);
				Num(*R, TEXT("vehicleTop"), RushTop);
				Num(*R, TEXT("vehicleGrip"), RushGrip);
			}
			const TSharedPtr<FJsonObject>* U = nullptr;
			if ((*A)->TryGetObjectField(TEXT("unlock"), U))
			{
				Num(*U, TEXT("fillSeconds"), UnlockFill);
				Num(*U, TEXT("range"), UnlockRange);
				Num(*U, TEXT("cctvStars"), CameraStars);
				Num(*U, TEXT("cctvSeconds"), CameraSeconds);
				Num(*U, TEXT("lightSeconds"), LightSeconds);
				Num(*U, TEXT("lightRadius"), LightRadius);
				float Loss = static_cast<float>(PhoneIntegrity);
				Num(*U, TEXT("phoneIntegrity"), Loss);
				PhoneIntegrity = FMath::RoundToInt(Loss);
				const TArray<TSharedPtr<FJsonValue>>* Cash = nullptr;
				if ((*U)->TryGetArrayField(TEXT("phoneCash"), Cash) && Cash->Num() >= 2)
				{
					PhoneCashMin = static_cast<int32>((*Cash)[0]->AsNumber());
					PhoneCashMax = FMath::Max(PhoneCashMin, static_cast<int32>((*Cash)[1]->AsNumber()));
				}
			}
		}
	}
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: leads: %d in the cast, %d leads; rush %.0f s x%.2f, unlock within %.0f m"), Crew.Num(), Leads.Num(), RushSeconds, RushSpeed, UnlockRange / 100.f);
	return Leads.Num() > 0;
}

void ANHLeads::BeginPlay()
{
	Super::BeginPlay();
	Load();
}

FName ANHLeads::LevelKey() const
{
	const UNHGameData* Data = UNHGameData::Get(this);
	return Data && Data->bRealCity ? FName(TEXT("city")) : FName(TEXT("small"));
}

FName ANHLeads::Current() const
{
	const UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const FName Lead = Hustle ? Hustle->Lead : NAME_None;
	return Find(Lead) ? Lead : Leads.Num() > 0 ? Leads[0] : NAME_None;
}

bool ANHLeads::IsUnlocked(FName Id) const
{
	const FMember* M = Find(Id);
	const UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	if (!M || !Hustle)
	{
		return false;
	}
	// every part of the rule has to hold: "mission:m12+flag:zainab_alive", "premium:2500+story"
	TArray<FString> Parts;
	M->Unlock.ParseIntoArray(Parts, TEXT("+"));
	for (const FString& Part : Parts)
	{
		FString Kind = Part, What;
		Part.Split(TEXT(":"), &Kind, &What);
		const bool bHolds = Kind == TEXT("start") ? true
			: Kind == TEXT("mission") ? Hustle->IsDone(FName(*What))
			: Kind == TEXT("flag") ? Hustle->Flag(FName(*What)) != 0
			: Kind == TEXT("story") ? Hustle->IsDone(FName(TEXT("m12")))
			: Kind == TEXT("premium") ? Hustle->Bought.Contains(Id)
			: false; // "never", and anything not understood
		if (!bHolds)
		{
			return false;
		}
	}
	return Parts.Num() > 0;
}

FNHLeadSpot* ANHLeads::Spot(FName Id, bool bAdd) const
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	if (!Hustle)
	{
		return nullptr;
	}
	const FName Level = LevelKey();
	if (FNHLeadSpot* Have = Hustle->LeadSpots.FindByPredicate([Id, Level](const FNHLeadSpot& S) { return S.Id == Id && S.Level == Level; }))
	{
		return Have;
	}
	if (!bAdd)
	{
		return nullptr;
	}
	FNHLeadSpot New;
	New.Id = Id;
	New.Level = Level;
	// the meter belongs to the person, not the level: carry it over from the other level's record
	if (const FNHLeadSpot* Other = Hustle->LeadSpots.FindByPredicate([Id](const FNHLeadSpot& S) { return S.Id == Id; }))
	{
		New.Meter = Other->Meter;
	}
	return &Hustle->LeadSpots.Add_GetRef(New);
}

bool ANHLeads::Ground(const FVector2D& At, float& OutZ) const
{
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, FVector(At.X, At.Y, 30000.f), FVector(At.X, At.Y, -3000.f), FCollisionObjectQueryParams(ECC_WorldStatic)))
	{
		OutZ = Hit.ImpactPoint.Z;
		return true;
	}
	return false;
}

FVector ANHLeads::HomeOf(const FMember& M) const
{
	const UNHGameData* Data = UNHGameData::Get(this);
	FVector2D At = Data ? Data->Home : FVector2D::ZeroVector;
	if (const FNHBusStop* Stop = Data ? Data->Stops.Find(M.Stop) : nullptr)
	{
		// on the pavement, a few steps back from where the passengers wait
		const FVector2D Back = (Stop->Wait - Stop->Kerb).GetSafeNormal();
		At = Stop->Wait + Back * 250.f;
	}
	else if (Data)
	{
		for (const FString& District : M.Districts)
		{
			if (Data->DistrictCentre(District, At))
			{
				break;
			}
		}
	}
	float Z = 0.f;
	Ground(At, Z);
	return FVector(At.X, At.Y, Z);
}

FVector ANHLeads::SpotOf(FName Id) const
{
	if (const FNHLeadSpot* S = Spot(Id, false); S && S->bPlaced)
	{
		return S->At;
	}
	const FMember* M = Find(Id);
	return M ? HomeOf(*M) : FVector::ZeroVector;
}

bool ANHLeads::WasPlaced(FName Id) const
{
	const FNHLeadSpot* S = Spot(Id, false);
	return S && S->bPlaced;
}

ANHCharacter* ANHLeads::StandIn(FName Id) const
{
	const TObjectPtr<ANHCharacter>* Body = StandIns.Find(Id);
	return Body && IsValid(*Body) ? Body->Get() : nullptr;
}

ANHCharacter* ANHLeads::Player() const
{
	const ANHPlayerController* PC = ::Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!PC)
	{
		return nullptr;
	}
	ANHCharacter* OnFoot = ::Cast<ANHCharacter>(PC->GetPawn());
	return OnFoot ? OnFoot : PC->GetOnFootCharacter();
}

void ANHLeads::Wear(ANHCharacter* Body, const FMember& M, bool bPlayer) const
{
	// a body the project lacks: keep whatever it has on (the blockout rule in characters.json)
	if (Body && !Body->WearSkin(M.Skin, false))
	{
		UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: leads: no body '%s' for %s here; %s"), *M.Skin.ToString(), *M.Name, bPlayer ? TEXT("the player keeps the one on") : TEXT("the stand-in keeps its own"));
	}
}

// ---------------------------------------------------------------------------------------------------- switching
void ANHLeads::Freeze(bool bOn) const
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetIgnoreMoveInput(bOn);
		PC->SetIgnoreLookInput(bOn);
	}
}

bool ANHLeads::Switch(FName To, bool bForce)
{
	ANHPlayerController* PC = ::Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const FName From = Current();
	// somebody rich is being played (ANHEstate): Tab goes back to the lead's own life, and nobody is left standing where the rich one was
	const bool bPersona = Hustle && !Hustle->Persona.IsNone();
	if (To.IsNone() && bPersona)
	{
		To = From;
	}
	if (To.IsNone())
	{
		// the other lead: the next one in the list that can be played
		const int32 At = Leads.IndexOfByKey(From);
		for (int32 K = 1; K <= Leads.Num() && To.IsNone(); ++K)
		{
			const FName Next = Leads[(FMath::Max(At, 0) + K) % Leads.Num()];
			To = Next != From && IsUnlocked(Next) ? Next : NAME_None;
		}
	}
	const auto No = [this](const FString& Why)
	{
		ANHHUD::Toast(this, Why, 0);
		UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: leads: no switch: %s"), *Why);
		return false;
	};
	const FMember* Who = Find(To);
	if (!PC || !Hustle || !Who || (To == From && !bPersona) || IsSwitching())
	{
		return No(IsSwitching() ? TEXT("Already switching") : TEXT("Nobody to switch to"));
	}
	if (!IsUnlocked(To))
	{
		return No(Who->Name + TEXT(" is not yours to play yet"));
	}
	if (!bForce)
	{
		const ANHGameDirector* Dir = ANHGameDirector::Get(this);
		if (bLocked)
		{
			return No(LockWhy.IsEmpty() ? TEXT("Not now") : LockWhy);
		}
		if (Dir && (Dir->IsBusy() || Dir->WantedVehicle()))
		{
			UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: leads: busy: panel %s, talk %s, in a job's vehicle %s"), Dir->Panel.bOpen ? *Dir->Panel.Title : TEXT("none"),
				Dir->Dialogue.bOpen ? *Dir->Dialogue.Speaker : TEXT("none"), Dir->WantedVehicle() ? TEXT("yes") : TEXT("no"));
			return No(TEXT("Finish the job first"));
		}
		if (Hustle->Stars() > 0)
		{
			return No(TEXT("Lose them first"));
		}
	}
	if (::Cast<ANHVehicle>(PC->GetPawn()) && !PC->LeaveVehicle(bForce))
	{
		return No(TEXT("Stop the car first"));
	}
	ANHCharacter* Me = ::Cast<ANHCharacter>(PC->GetPawn());
	if (!Me)
	{
		return No(TEXT("Nobody to switch from"));
	}
	if (RushOn())
	{
		RushLeft = 0.f;
		ApplyRush(false);
	}
	// where the one being left stays
	if (FNHLeadSpot* Left = bPersona ? nullptr : Spot(From, true))
	{
		Left->At = Me->GetActorLocation() - FVector(0.f, 0.f, Me->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		Left->Yaw = Me->GetActorRotation().Yaw;
		Left->bPlaced = true;
	}
	SwitchFrom = From;
	SwitchTo = To;
	if (const FNHLeadSpot* Dest = Spot(To, false); Dest && Dest->bPlaced)
	{
		LandAt = Dest->At;
		LandYaw = Dest->Yaw;
	}
	else
	{
		LandAt = HomeOf(*Who);
		LandYaw = 0.f;
	}
	StartRise();
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: leads: %s -> %s, to %.0f, %.0f (%.0f m away)%s"), *From.ToString(), *To.ToString(), LandAt.X, LandAt.Y,
		FVector::Dist2D(Me->GetActorLocation(), LandAt) / 100.f, bForce ? TEXT(", forced") : TEXT(""));
	return true;
}

void ANHLeads::StartRise()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Me = PC->GetPawn();
	Freeze(true);
	if (!Sky)
	{
		Sky = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
		Sky->GetCameraComponent()->bConstrainAspectRatio = false;
	}
	// straight up over the head, looking down, the top of the picture the way the player was looking
	Sky->SetActorLocationAndRotation(Me->GetActorLocation() + FVector(0.f, 0.f, SwitchHeight), FRotator(-89.f, PC->GetControlRotation().Yaw, 0.f));
	PC->SetViewTargetWithBlend(Sky, SwitchSeconds, VTBlend_Cubic);
	Stage = EStage::Rise;
	StageT = 0.f;
}

void ANHLeads::DoLand()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	ANHCharacter* Me = ::Cast<ANHCharacter>(PC->GetPawn());
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const FMember* Who = Find(SwitchTo);
	if (!Me || !Hustle || !Who)
	{
		EndSwitch();
		return;
	}
	// the body waiting there goes: the player is that person now
	if (ANHCharacter* Waiting = StandIn(SwitchTo))
	{
		Waiting->Destroy();
	}
	StandIns.Remove(SwitchTo);
	Hustle->Lead = SwitchTo;
	Hustle->Persona = NAME_None; // back in the story's cast, whoever was being played
	Wear(Me, *Who, true);
	Me->Health = 100.f;
	// held in the air over the place until there is ground to stand on: a part of the city far off takes a moment to load
	Me->GetCharacterMovement()->StopMovementImmediately();
	Me->GetCharacterMovement()->SetMovementMode(MOVE_None);
	Me->SetActorLocationAndRotation(LandAt + FVector(0.f, 0.f, 400.f), FRotator(0.f, LandYaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	PC->SetControlRotation(FRotator(-10.f, LandYaw, 0.f));
	Sky->SetActorLocationAndRotation(LandAt + FVector(0.f, 0.f, SwitchHeight), FRotator(-89.f, LandYaw, 0.f));
	Stage = EStage::Land;
	StageT = 0.f;
}

void ANHLeads::EndSwitch()
{
	Freeze(false);
	Stage = EStage::Idle;
	StageT = 0.f;
	StandInWait = 0.f; // the one left behind gets a body straight away
	if (UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this))
	{
		Hustle->Save();
	}
	if (const FMember* Who = Find(SwitchTo))
	{
		ANHHUD::Toast(this, FString::Printf(TEXT("%s, %s"), *Who->Name, *Who->From), 1);
	}
}

void ANHLeads::UpdateStandIns()
{
	const ANHCharacter* Me = Player();
	const FName Now = Current();
	if (!Me)
	{
		return;
	}
	for (const FName& Id : Leads)
	{
		ANHCharacter* Body = StandIn(Id);
		const FNHLeadSpot* S = Spot(Id, false);
		const bool bWanted = Id != Now && Id != SwitchTo && S && S->bPlaced && IsUnlocked(Id);
		const float Far = S ? FVector::Dist2D(Me->GetActorLocation(), S->At) : 0.f;
		if (Body && (!bWanted || Far > StandInFar))
		{
			Body->Destroy();
			StandIns.Remove(Id);
		}
		else if (!Body && bWanted && Far < StandInNear)
		{
			float Z = 0.f;
			if (!Ground(FVector2D(S->At), Z))
			{
				continue; // nothing loaded to stand on yet
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ANHCharacter* New = GetWorld()->SpawnActor<ANHCharacter>(ANHCharacter::StaticClass(), FVector(S->At.X, S->At.Y, FMath::Max(Z, S->At.Z) + 96.f), FRotator(0.f, S->Yaw, 0.f), Params);
			if (New)
			{
				Wear(New, *Find(Id), false);
				StandIns.Add(Id, New);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------- abilities
float ANHLeads::Meter(FName Id) const
{
	const UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	float Best = 0.f;
	if (Hustle)
	{
		for (const FNHLeadSpot& S : Hustle->LeadSpots)
		{
			Best = S.Id == Id ? FMath::Max(Best, S.Meter) : Best;
		}
	}
	return Best;
}

void ANHLeads::SetMeter(FName Id, float Value)
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	if (!Hustle || !Spot(Id, true))
	{
		return;
	}
	for (FNHLeadSpot& S : Hustle->LeadSpots)
	{
		if (S.Id == Id)
		{
			S.Meter = FMath::Clamp(Value, 0.f, 1.f); // one meter a person, whichever level's record it is read from
		}
	}
}

void ANHLeads::ApplyRush(bool bOn)
{
	ANHPlayerController* PC = ::Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	if (ANHCharacter* Me = Player())
	{
		Me->SpeedBoost = bOn ? RushSpeed : 1.f;
		Me->DamageTaken = bOn ? RushDamage : 1.f;
		Me->SetSprinting(Me->IsSprinting()); // takes the new speed up now
	}
	// the car he is in (or gets into while it lasts); the one he was in goes back to how it was made
	ANHVehicle* Car = bOn && PC ? ::Cast<ANHVehicle>(PC->GetPawn()) : nullptr;
	if (ANHVehicle* Was = RushCar.Get(); Was && Was != Car)
	{
		Was->PullBoost = Was->TopBoost = 1.f;
		Was->GetDynamics()->GripAcceleration = RushCarGrip;
		RushCar = nullptr;
	}
	if (Car && RushCar.Get() != Car)
	{
		RushCar = Car;
		RushCarGrip = Car->GetDynamics()->GripAcceleration;
		Car->PullBoost = RushPull;
		Car->TopBoost = RushTop;
		Car->GetDynamics()->GripAcceleration = RushCarGrip * RushGrip;
	}
}

bool ANHLeads::CamerasDown() const
{
	return GetWorld()->GetTimeSeconds() < CamerasDownUntil;
}

void ANHLeads::FindTarget()
{
	Target = nullptr;
	const FMember* Who = Find(Current());
	const ANHPlayerController* PC = ::Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Who || Who->Ability != TEXT("unlock") || !Me)
	{
		return;
	}
	float Best = UnlockRange;
	const FVector Here = Me->GetActorLocation();
	// the one a story mission is asking for comes before anything else in reach
	if (const ANHMissions* Story = ANHMissions::Get(this))
	{
		if (ANHHackPoint* Wanted = Story->UnlockTarget(); Wanted && !Wanted->IsHacked() && FVector::Dist(Here, Wanted->GetActorLocation()) < UnlockRange)
		{
			Target = Wanted;
			return;
		}
	}
	for (ANHHackPoint* Point : HackPoints)
	{
		if (const float D = IsValid(Point) && !Point->IsHacked() ? FVector::Dist(Here, Point->GetActorLocation()) : UnlockRange; D < Best)
		{
			Best = D;
			Target = Point;
		}
	}
	// a phone: anybody standing near who is not part of the job in hand
	for (TActorIterator<ANHPerson> It(GetWorld()); It; ++It)
	{
		if (const float D = !It->IsDown() && !It->bEssential ? FVector::Dist(Here, It->GetActorLocation()) : UnlockRange; D < Best)
		{
			Best = D;
			Target = *It;
		}
	}
}

bool ANHLeads::UseAbility()
{
	const FName Id = Current();
	const FMember* Who = Find(Id);
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	if (!Who || !Hustle || Who->Ability.IsNone() || IsSwitching())
	{
		return false;
	}
	if (Meter(Id) < 1.f)
	{
		ANHHUD::Toast(this, FString::Printf(TEXT("%s is not ready (%d%%)"), *Who->AbilityName, FMath::FloorToInt(Meter(Id) * 100.f)), 0);
		return false;
	}
	if (Who->Ability == TEXT("hustleRush"))
	{
		RushLeft = RushSeconds;
		ApplyRush(true);
		SetMeter(Id, 0.f);
		ANHHUD::Toast(this, TEXT("Hustle Rush!"), 1);
		UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: leads: Hustle Rush for %.0f s (speed x%.2f, blows x%.2f)"), RushSeconds, RushSpeed, RushDamage);
		return true;
	}
	if (Who->Ability == TEXT("unlock"))
	{
		FindTarget();
		AActor* What = Target.Get();
		if (!What)
		{
			ANHHUD::Toast(this, FString::Printf(TEXT("Nothing to unlock within %d m"), FMath::RoundToInt(UnlockRange / 100.f)), 0);
			return false;
		}
		if (ANHHackPoint* Point = ::Cast<ANHHackPoint>(What))
		{
			if (Point->GetKind() == ENHHackKind::Camera)
			{
				Point->Hack(CameraSeconds);
				CamerasDownUntil = GetWorld()->GetTimeSeconds() + CameraSeconds;
				const int32 Before = Hustle->Stars();
				Hustle->Heat = FMath::Max(0.f, Hustle->Heat - CameraStars);
				LastUnlocked = TEXT("camera");
				ANHHUD::Toast(this, Before > Hustle->Stars() ? TEXT("Camera wiped. One star gone.") : FString::Printf(TEXT("Camera down for %d seconds"), FMath::RoundToInt(CameraSeconds)), 1);
			}
			else
			{
				Point->Hack(LightSeconds);
				if (ANHTraffic* Traffic = ANHTraffic::Get(this))
				{
					Traffic->StopAt(FVector2D(Point->GetActorLocation()), LightRadius, LightSeconds);
				}
				LastUnlocked = TEXT("light");
				ANHHUD::Toast(this, FString::Printf(TEXT("Light don red for %d seconds"), FMath::RoundToInt(LightSeconds)), 1);
			}
		}
		else
		{
			const int32 Got = FMath::RandRange(PhoneCashMin, PhoneCashMax);
			Hustle->Earn(Got, TEXT("unlocked a phone"));
			Hustle->Integrity = FMath::Clamp(Hustle->Integrity + PhoneIntegrity, -100, 100);
			LastUnlocked = TEXT("phone");
			ANHHUD::Floater(this, What->GetActorLocation() + FVector(0.f, 0.f, 190.f), TEXT("phone unlocked"));
		}
		SetMeter(Id, 0.f);
		Target = nullptr;
		UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: leads: Unlock on a %s, %.0f m away"), *LastUnlocked.ToString(), FVector::Dist(What->GetActorLocation(), Player() ? Player()->GetActorLocation() : What->GetActorLocation()) / 100.f);
		return true;
	}
	ANHHUD::Toast(this, Who->AbilityName + TEXT(" comes later"), 0);
	return false;
}

ANHHackPoint* ANHLeads::PlaceHackPoint(ENHHackKind Kind, float Ahead)
{
	const ANHCharacter* Me = Player();
	if (!Me)
	{
		return nullptr;
	}
	const FVector At = Me->GetActorLocation() + Me->GetActorForwardVector() * Ahead;
	float Z = At.Z - 96.f;
	Ground(FVector2D(At), Z);
	ANHHackPoint* Point = GetWorld()->SpawnActor<ANHHackPoint>(ANHHackPoint::StaticClass(), FVector(At.X, At.Y, Z), FRotator(0.f, Me->GetActorRotation().Yaw + 180.f, 0.f));
	if (Point)
	{
		Point->Build(Kind);
		HackPoints.Add(Point);
	}
	return Point;
}

void ANHLeads::UpdateHackPoints()
{
	const ANHCharacter* Me = Player();
	const UNHGameData* Data = UNHGameData::Get(this);
	if (!Me || !Data)
	{
		return;
	}
	const FVector2D Here(Me->GetActorLocation());
	for (int32 I = HackPoints.Num() - 1; I >= 0; --I)
	{
		if (!IsValid(HackPoints[I]) || FVector2D::Distance(Here, FVector2D(HackPoints[I]->GetActorLocation())) > HackPointFar)
		{
			if (IsValid(HackPoints[I]))
			{
				HackPoints[I]->Destroy();
			}
			HackPoints.RemoveAt(I);
		}
	}
	if (HackPoints.Num() >= HackPointMax)
	{
		return;
	}
	// where they go: beside the junctions of the real city's roads; in the small city, beside the bus stops
	TArray<TPair<FVector2D, float>> Places; // the place, and which way the thing faces
	if (Data->bRealCity)
	{
		TArray<FNHRoadSeg> Near;
		Data->RoadsNear(Here, 12000.f, Near);
		TSet<int32> Seen;
		for (const FNHRoadSeg& Seg : Near)
		{
			if (!Data->RoadWays.IsValidIndex(Seg.Way) || !Data->RoadWays[Seg.Way].Nodes.IsValidIndex(Seg.Index + 1))
			{
				continue;
			}
			const FNHRoadWay& Way = Data->RoadWays[Seg.Way];
			for (const int32 K : { Seg.Index, Seg.Index + 1 })
			{
				const int32 Node = Way.Nodes[K];
				const TArray<FNHRoadSeg>* Joins = Data->RoadJoins.Find(Node);
				if (Seen.Contains(Node) || !Joins || Joins->Num() < 3 || !Data->RoadNodes.IsValidIndex(Node))
				{
					continue;
				}
				Seen.Add(Node);
				const FVector2D A = Data->RoadNodes[Way.Nodes[Seg.Index]], B = Data->RoadNodes[Way.Nodes[Seg.Index + 1]];
				const FVector2D Along = (B - A).GetSafeNormal(), Right(-Along.Y, Along.X);
				const FVector2D Corner = Data->RoadNodes[Node] + (Right + Along * (K == Seg.Index ? -1.f : 1.f)) * (Data->HalfWidth(Way) + 250.f);
				Places.Add({ Corner, FMath::RadiansToDegrees(FMath::Atan2(-Right.Y, -Right.X)) });
			}
		}
	}
	else
	{
		for (const TPair<FName, FNHBusStop>& Stop : Data->Stops)
		{
			const FVector2D Back = (Stop.Value.Wait - Stop.Value.Kerb).GetSafeNormal(), Along(-Back.Y, Back.X);
			Places.Add({ Stop.Value.Wait + Along * 700.f + Back * 120.f, FMath::RadiansToDegrees(FMath::Atan2(-Back.Y, -Back.X)) });
		}
	}
	Places.Sort([&Here](const TPair<FVector2D, float>& L, const TPair<FVector2D, float>& R) { return FVector2D::DistSquared(L.Key, Here) < FVector2D::DistSquared(R.Key, Here); });
	for (const TPair<FVector2D, float>& Place : Places)
	{
		if (HackPoints.Num() >= HackPointMax || FVector2D::Distance(Place.Key, Here) > 15000.f)
		{
			break;
		}
		const bool bTaken = HackPoints.ContainsByPredicate([&Place](const ANHHackPoint* P) { return FVector2D::Distance(FVector2D(P->GetActorLocation()), Place.Key) < 2500.f; });
		float Z = 0.f;
		if (bTaken || !Ground(Place.Key, Z))
		{
			continue;
		}
		ANHHackPoint* Point = GetWorld()->SpawnActor<ANHHackPoint>(ANHHackPoint::StaticClass(), FVector(Place.Key.X, Place.Key.Y, Z), FRotator(0.f, Place.Value, 0.f));
		if (Point)
		{
			// which it is depends on where it stands, so the same corner always has the same thing
			Point->Build((FMath::FloorToInt(Place.Key.X / 500.f) + FMath::FloorToInt(Place.Key.Y / 500.f)) % 2 == 0 ? ENHHackKind::Camera : ENHHackKind::Light);
			HackPoints.Add(Point);
		}
	}
}

ANHLeads::FHud ANHLeads::Hud() const
{
	FHud H;
	const FName Id = Current();
	if (const FMember* Who = Find(Id))
	{
		H.Name = Who->Name;
		H.Colour = Who->Colour;
		H.AbilityName = Who->AbilityName;
		H.bHasAbility = Who->Ability == TEXT("hustleRush") || Who->Ability == TEXT("unlock");
		H.Meter = Meter(Id);
	}
	H.RushLeft = RushLeft;
	H.bSwitching = IsSwitching();
	H.bCanSwitch = Leads.Num() > 1 && !bLocked;
	if (const FMember* To = Find(SwitchTo); To && H.bSwitching)
	{
		H.SwitchTo = To->Name + TEXT(", ") + To->From;
	}
	if (const AActor* What = Target.Get(); What && !H.bSwitching)
	{
		const ANHHackPoint* Point = ::Cast<ANHHackPoint>(What);
		H.bTarget = true;
		H.TargetAt = Point ? Point->Head() : What->GetActorLocation() + FVector(0.f, 0.f, 150.f);
		H.TargetLabel = Point ? (Point->GetKind() == ENHHackKind::Camera ? TEXT("CCTV") : TEXT("Traffic light")) : TEXT("Phone");
	}
	return H;
}

void ANHLeads::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	if (!PC || !Hustle || Leads.Num() == 0)
	{
		return;
	}

	// once the player is in: the lead's own body, unless somebody rich is being played (ANHEstate dresses those)
	if (!bStarted)
	{
		StartWait -= DeltaSeconds;
		ANHCharacter* Me = ::Cast<ANHCharacter>(PC->GetPawn());
		if (StartWait > 0.f || !Me)
		{
			return;
		}
		bStarted = true;
		if (const FMember* Who = Find(Current()); Who && Hustle->Persona.IsNone())
		{
			Wear(Me, *Who, true);
		}
	}

	// ---- the switch
	StageT += DeltaSeconds;
	if (Stage == EStage::Rise && StageT >= SwitchSeconds)
	{
		DoLand();
	}
	else if (Stage == EStage::Land)
	{
		ANHCharacter* Me = ::Cast<ANHCharacter>(PC->GetPawn());
		float Z = 0.f;
		const bool bGround = Ground(FVector2D(LandAt), Z);
		if (Me && (bGround || StageT > 10.f))
		{
			// down onto it, and the camera comes back to the shoulder
			Me->SetActorLocation(FVector(LandAt.X, LandAt.Y, (bGround ? Z : LandAt.Z) + Me->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 4.f), false, nullptr, ETeleportType::TeleportPhysics);
			Me->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			PC->SetViewTargetWithBlend(Me, SwitchSeconds, VTBlend_Cubic);
			Stage = EStage::Drop;
			StageT = 0.f;
		}
	}
	else if (Stage == EStage::Drop && StageT >= SwitchSeconds)
	{
		EndSwitch();
	}

	// ---- the leads left standing
	StandInWait -= DeltaSeconds;
	if (StandInWait <= 0.f && !IsSwitching())
	{
		StandInWait = 1.f;
		UpdateStandIns();
	}

	// ---- the ability of whoever is being played
	const FName Id = Current();
	const FMember* Who = Find(Id);
	if (RushLeft > 0.f)
	{
		RushLeft -= DeltaSeconds;
		ApplyRush(RushLeft > 0.f); // also catches getting into or out of a car while it lasts
	}
	else if (Who && !IsSwitching())
	{
		const float Fill = Who->Ability == TEXT("hustleRush") ? RushFill : Who->Ability == TEXT("unlock") ? UnlockFill : 0.f;
		if (Fill > 0.f && Meter(Id) < 1.f)
		{
			// it fills with play: twice as fast on the move as standing about
			const APawn* Me = PC->GetPawn();
			const bool bMoving = Me && Me->GetVelocity().SizeSquared2D() > 100.f * 100.f;
			SetMeter(Id, Meter(Id) + DeltaSeconds / Fill * (bMoving ? 1.f : 0.5f));
		}
	}
	HackWait -= DeltaSeconds;
	if (HackWait <= 0.f && !IsSwitching())
	{
		HackWait = 1.5f;
		UpdateHackPoints();
	}
	TargetWait -= DeltaSeconds;
	if (TargetWait <= 0.f)
	{
		TargetWait = 0.25f;
		FindTarget();
	}
}
