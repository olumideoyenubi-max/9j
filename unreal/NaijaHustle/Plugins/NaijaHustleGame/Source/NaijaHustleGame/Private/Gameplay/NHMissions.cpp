#include "Gameplay/NHMissions.h"

#include "Components/StaticMeshComponent.h"
#include "Core/NHGameData.h"
#include "Core/NHHustleSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/NHGameDirector.h"
#include "Gameplay/NHGuard.h"
#include "Gameplay/NHLeads.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NaijaHustleGame.h"
#include "Player/NHCharacter.h"
#include "Player/NHPlayerController.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UI/NHHUD.h"
#include "Vehicles/NHVehicle.h"
#include "World/NHShapes.h"

namespace
{
	FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, const FString& Default = FString())
	{
		FString V;
		return O.IsValid() && O->TryGetStringField(Key, V) ? V : Default;
	}

	double Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double Default = 0.0)
	{
		double V = 0.0;
		return O.IsValid() && O->TryGetNumberField(Key, V) ? V : Default;
	}

	bool Flag(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, bool bDefault = false)
	{
		bool V = false;
		return O.IsValid() && O->TryGetBoolField(Key, V) ? V : bDefault;
	}

	TSharedPtr<FJsonObject> Sub(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TSharedPtr<FJsonObject>* V = nullptr;
		return O.IsValid() && O->TryGetObjectField(Key, V) ? *V : TSharedPtr<FJsonObject>();
	}

	TArray<TSharedPtr<FJsonObject>> List(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		TArray<TSharedPtr<FJsonObject>> Out;
		const TArray<TSharedPtr<FJsonValue>>* V = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Key, V))
		{
			for (const TSharedPtr<FJsonValue>& Item : *V)
			{
				const TSharedPtr<FJsonObject>* J = nullptr;
				if (Item->TryGetObject(J))
				{
					Out.Add(*J);
				}
			}
		}
		return Out;
	}

	TArray<FString> Lines(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		TArray<FString> Out;
		if (O.IsValid())
		{
			O->TryGetStringArrayField(Key, Out);
		}
		return Out;
	}

	FString Clock(float Seconds)
	{
		const int32 S = FMath::RoundToInt(Seconds);
		return FString::Printf(TEXT("%d:%02d"), S / 60, S % 60);
	}

}

ANHMissions::ANHMissions()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ANHMissions* ANHMissions::Get(const UObject* WorldContext)
{
	return WorldContext ? Cast<ANHMissions>(UGameplayStatics::GetActorOfClass(WorldContext, ANHMissions::StaticClass())) : nullptr;
}

ANHCharacter* ANHMissions::Player() const
{
	const ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	ANHCharacter* OnFoot = PC ? Cast<ANHCharacter>(PC->GetPawn()) : nullptr;
	return OnFoot ? OnFoot : PC ? PC->GetOnFootCharacter() : nullptr;
}

ANHGameDirector* ANHMissions::Dir() const
{
	return ANHGameDirector::Get(this);
}

void ANHMissions::BeginPlay()
{
	Super::BeginPlay();
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(UNHGameData::DataDir() / TEXT("missions") / TEXT("*.json")), true, false);
	Files.Sort();
	for (const FString& File : Files)
	{
		Ids.Add(FName(*FPaths::GetBaseFilename(File)));
	}
	LoadRules();
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: missions: %d in Data/missions; the night shift pays %.0f from mission 2, %.1f%% more each mission after"), Ids.Num(), PayBase, PayGrowth * 100.f);
}

void ANHMissions::LoadRules()
{
	FString Text;
	TSharedPtr<FJsonObject> Rules;
	if (FFileHelper::LoadFileToString(Text, *(UNHGameData::DataDir() / TEXT("naija_rules.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Rules) && Rules)
	{
		const TSharedPtr<FJsonObject> Economy = Sub(Rules, TEXT("economy"));
		PayBase = static_cast<float>(Num(Economy, TEXT("missionPayBase"), PayBase));
		PayGrowth = static_cast<float>(Num(Economy, TEXT("missionPayGrowth"), PayGrowth));
	}
}

int32 ANHMissions::PayFor(int32 MissionNumber) const
{
	// mission 1's own shift pays its fares and tips; the bag is found on the second night, and it grows from there
	return MissionNumber < 2 ? 0 : static_cast<int32>(FMath::RoundToInt64(static_cast<double>(PayBase) * FMath::Pow(1.0 + static_cast<double>(PayGrowth), static_cast<double>(MissionNumber - 2))));
}

bool ANHMissions::LoadFile(FName MissionId, TSharedPtr<FJsonObject>& Out) const
{
	FString Text;
	return FFileHelper::LoadFileToString(Text, *(UNHGameData::DataDir() / TEXT("missions") / (MissionId.ToString() + TEXT(".json")))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out) && Out.IsValid();
}

FString ANHMissions::ObjectiveType() const
{
	return bActive && Objectives.IsValidIndex(Index) ? Type : FString();
}

ANHHackPoint* ANHMissions::UnlockTarget() const
{
	return HackTarget;
}

ANHVehicle* ANHMissions::MissionVehicle() const
{
	return Night == ENight::Drive ? NightBus : Car;
}

bool ANHMissions::Where(FVector& Out) const
{
	Out = PlaceAt;
	return bActive && bHasPlace;
}

// ---------------------------------------------------------------------------------------------------- places
bool ANHMissions::Place(const TSharedPtr<FJsonObject>& At, FVector& Out) const
{
	if (!At.IsValid())
	{
		return false;
	}
	const UNHGameData* Data = UNHGameData::Get(this);
	FVector2D P = FVector2D(Anchor);
	const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
	FString StopId, LeadId;
	if (At->TryGetStringField(TEXT("stop"), StopId))
	{
		const FNHBusStop* Stop = Data ? Data->Stops.Find(FName(*StopId)) : nullptr;
		if (!Stop)
		{
			UE_LOG(LogNHGame, Warning, TEXT("NAIJA HUSTLE: missions: %s: no bus stop '%s' in this level"), *Id.ToString(), *StopId);
			return false;
		}
		// along the kerb (the way the buses go) and back from it (onto the pavement and beyond)
		const FVector2D Back = (Stop->Wait - Stop->Kerb).GetSafeNormal(), Along(-Back.Y, Back.X);
		P = Stop->Wait + Along * static_cast<float>(Num(At, TEXT("along"))) + Back * static_cast<float>(Num(At, TEXT("back")));
	}
	else if (At->TryGetArrayField(TEXT("xy"), Pair) && Pair->Num() >= 2)
	{
		P = FVector2D((*Pair)[0]->AsNumber(), (*Pair)[1]->AsNumber());
	}
	else if (At->TryGetArrayField(TEXT("player"), Pair) && Pair->Num() >= 2)
	{
		const FVector2D Ahead(FMath::Cos(FMath::DegreesToRadians(AnchorYaw)), FMath::Sin(FMath::DegreesToRadians(AnchorYaw))), Right(-Ahead.Y, Ahead.X);
		P = FVector2D(Anchor) + Ahead * static_cast<float>((*Pair)[0]->AsNumber()) + Right * static_cast<float>((*Pair)[1]->AsNumber());
	}
	else if (At->TryGetStringField(TEXT("lead"), LeadId))
	{
		const ANHLeads* Leads = ANHLeads::Get(this);
		P = FVector2D(Leads ? Leads->SpotOf(FName(*LeadId)) : Anchor);
	}
	else
	{
		return false;
	}
	FHitResult Hit;
	const bool bGround = GetWorld()->LineTraceSingleByObjectType(Hit, FVector(P.X, P.Y, 30000.f), FVector(P.X, P.Y, -3000.f), FCollisionObjectQueryParams(ECC_WorldStatic));
	Out = FVector(P.X, P.Y, bGround ? Hit.ImpactPoint.Z : Anchor.Z - 92.f);
	return true;
}

// ---------------------------------------------------------------------------------------------------- start and stop
bool ANHMissions::Start(FName MissionId)
{
	ANHGameDirector* D = Dir();
	TSharedPtr<FJsonObject> File;
	if (bActive || !D || D->IsBusy() || !LoadFile(MissionId, File))
	{
		UE_LOG(LogNHGame, Warning, TEXT("NAIJA HUSTLE: missions: cannot start '%s' (%s)"), *MissionId.ToString(), bActive ? TEXT("one is running") : !D || D->IsBusy() ? TEXT("busy") : TEXT("no such file in Data/missions"));
		return false;
	}
	Root = File;
	Id = MissionId;
	Title = Str(Root, TEXT("title"), MissionId.ToString());
	Number = static_cast<int32>(Num(Root, TEXT("number")));
	Objectives = List(Root, TEXT("objectives"));
	// the length rule: five to eight minutes, twelve for the two big ones
	LengthLimit = static_cast<float>(Num(Root, TEXT("lengthLimit"), Number == 8 || Number == 12 ? 720.0 : 480.0));
	const bool bTest = Str(Root, TEXT("kind")) == TEXT("test");
	if (Objectives.Num() == 0)
	{
		UE_LOG(LogNHGame, Warning, TEXT("NAIJA HUSTLE: missions: %s has no objectives"), *Id.ToString());
		return false;
	}
	if (Objectives.Num() > 5 && !bTest)
	{
		UE_LOG(LogNHGame, Warning, TEXT("NAIJA HUSTLE: missions: %s has %d objectives; the rule is five at most"), *Id.ToString(), Objectives.Num());
	}
	for (const TSharedPtr<FJsonObject>& O : Objectives)
	{
		if (const int32 Said = Lines(Sub(O, TEXT("say")), TEXT("lines")).Num(); Said > 8)
		{
			UE_LOG(LogNHGame, Warning, TEXT("NAIJA HUSTLE: missions: %s: a scene of %d lines; the rule is eight at most"), *Id.ToString(), Said);
		}
	}
	bActive = true;
	Index = -1;
	MissionT = 0.f;
	RestartCount = 0;
	Times.Reset();
	bFailed = false;
	Night = ENight::None;
	Disguise = NAME_None;
	Last = FResult();
	Last.Id = Id;
	bLockedSwitch = Flag(Root, TEXT("lockSwitch"), true);

	// the job card
	const FString PlayAs = Str(Root, TEXT("playAs"));
	const ANHLeads* Leads = ANHLeads::Get(this);
	const ANHLeads::FMember* Who = Leads ? Leads->Find(FName(*PlayAs)) : nullptr;
	TArray<FString> CardLines;
	CardLines.Add(Str(Root, TEXT("brief")));
	CardLines.Add(FString::Printf(TEXT("You play: %s"), Who ? *Who->Name : PlayAs == TEXT("both") ? TEXT("Tunde and Amaka") : TEXT("whoever you are")));
	bCardOpen = true;
	D->OpenPanel(Title.ToUpper(), CardLines, { TEXT("Start") }, [this](int32)
	{
		bCardOpen = false;
		// in the right shoes before it begins
		const FString As = Str(Root, TEXT("playAs"));
		if (ANHLeads* L = ANHLeads::Get(this); L && L->Find(FName(*As)) && L->Current() != FName(*As))
		{
			L->Switch(FName(*As), true);
		}
	});
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: missions: %s \"%s\" (mission %d, %d objectives, as %s)"), *Id.ToString(), *Title, Number, Objectives.Num(), *PlayAs);
	return true;
}

void ANHMissions::Clear()
{
	for (AActor* Thing : Spawned)
	{
		if (IsValid(Thing))
		{
			// a vehicle the player is sitting in stays; the mission's end is not a reason to be thrown out of it
			const ANHVehicle* Vehicle = Cast<ANHVehicle>(Thing);
			if (!Vehicle || !Vehicle->GetController())
			{
				Thing->Destroy();
			}
		}
	}
	Spawned.Reset();
	Pickups.Reset();
	GuardList.Reset();
	HackTarget = nullptr;
	Car = nullptr;
	bHasPlace = false;
}

void ANHMissions::Abort(const FString& Why)
{
	if (!bActive)
	{
		return;
	}
	Clear();
	if (IsValid(NightBus) && !NightBus->GetController())
	{
		NightBus->Destroy();
	}
	NightBus = nullptr;
	if (ANHGameDirector* D = Dir())
	{
		D->Panel = ANHGameDirector::FPanel();
		D->Dialogue = ANHGameDirector::FDialogue();
		D->bMarker = false;
	}
	if (ANHLeads* Leads = ANHLeads::Get(this))
	{
		Leads->SetLocked(false);
	}
	bActive = false;
	bCardOpen = false;
	Night = ENight::None;
	Disguise = NAME_None;
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: missions: %s given up after %s: %s"), *Id.ToString(), *Clock(MissionT), *Why);
}

// ---------------------------------------------------------------------------------------------------- objectives
void ANHMissions::SaveCheckpoint()
{
	const ANHCharacter* Me = Player();
	const ANHLeads* Leads = ANHLeads::Get(this);
	Checkpoint.Index = Index;
	Checkpoint.At = Me ? Me->GetActorLocation() : FVector::ZeroVector;
	Checkpoint.Yaw = Me ? Me->GetActorRotation().Yaw : 0.f;
	Checkpoint.Lead = Leads ? Leads->Current() : NAME_None;
	Checkpoint.Disguise = Disguise;
}

void ANHMissions::Begin(int32 NewIndex)
{
	Index = NewIndex;
	ObjectiveT = 0.f;
	bObjectiveReady = false;
	bChose = false;
	bSaid = true;
	PlanStep = 0;
	Left = 0;
	NoiseWait = 0.f;
	const ANHCharacter* Me = Player();
	Anchor = Me ? Me->GetActorLocation() : FVector::ZeroVector;
	AnchorYaw = Me ? Me->GetActorRotation().Yaw : 0.f;
	ShotsHeard = Me ? Me->Attacks : 0;
	Type = Str(Obj(), TEXT("type"));
	if (Index == 0 || Flag(Obj(), TEXT("checkpoint")))
	{
		SaveCheckpoint();
	}
	if (ANHLeads* Leads = ANHLeads::Get(this))
	{
		Leads->SetLocked(bLockedSwitch, TEXT("Finish the job first"));
	}
	// the scene's lines first; the objective is there to be done once they have been heard or skipped
	if (const TSharedPtr<FJsonObject> Say = Sub(Obj(), TEXT("say")); Say.IsValid() && Lines(Say, TEXT("lines")).Num() > 0)
	{
		bSaid = false;
		Dir()->Say(Str(Say, TEXT("speaker")), Lines(Say, TEXT("lines")), [this]() { bSaid = true; });
	}
	Arm();
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: missions: %s objective %d of %d (%s): %s"), *Id.ToString(), Index + 1, Objectives.Num(), *Type, *Str(Obj(), TEXT("text")));
}

AActor* ANHMissions::SpawnPickup(const FVector& At, const FLinearColor& Colour)
{
	AActor* Thing = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(At));
	if (Thing)
	{
		USceneComponent* Root0 = NewObject<USceneComponent>(Thing, TEXT("Root"));
		Thing->SetRootComponent(Root0);
		Root0->RegisterComponent();
		Thing->SetActorLocation(At);
		FNHSurface Surface;
		Surface.Color = Colour;
		Surface.Glow = 1.5f;
		if (UStaticMeshComponent* Box = NHShapes::AddPiece(Thing, Root0, ENHShape::Box, FVector(0.f, 0.f, 30.f), FVector(50.f, 40.f, 40.f), Surface))
		{
			Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		Spawned.Add(Thing);
		Pickups.Add(Thing);
	}
	return Thing;
}

void ANHMissions::SpawnGuards()
{
	int32 Seed = 100 * (Index + 1);
	for (const TSharedPtr<FJsonObject>& G : List(Obj(), TEXT("guards")))
	{
		FVector At;
		if (!Place(Sub(G, TEXT("at")), At))
		{
			continue;
		}
		float Yaw = static_cast<float>(Num(G, TEXT("yaw"), AnchorYaw + 180.0));
		if (FVector Look; Place(Sub(G, TEXT("look")), Look))
		{
			Yaw = static_cast<float>((Look - At).Rotation().Yaw); // "look": the place it faces
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ANHGuard* Guard = GetWorld()->SpawnActor<ANHGuard>(ANHGuard::StaticClass(), At, FRotator(0.f, Yaw, 0.f), Params);
		if (!Guard)
		{
			continue;
		}
		Guard->Init(++Seed, FLinearColor(0.08f, 0.08f, 0.1f), ENHCast::Man);
		Guard->SetActorRotation(FRotator(0.f, Yaw, 0.f));
		Guard->bArmed = Flag(G, TEXT("armed"));
		Guard->bCoward = Flag(G, TEXT("coward"));
		Guard->SightRange = static_cast<float>(Num(G, TEXT("sight"), Guard->SightRange));
		Guard->Health = static_cast<float>(Num(G, TEXT("health"), 100.0));
		for (const FString& Outfit : Lines(G, TEXT("fooledBy")))
		{
			Guard->FooledBy.Add(FName(*Outfit));
		}
		TArray<FVector> Beat;
		for (const TSharedPtr<FJsonObject>& B : List(G, TEXT("beat")))
		{
			FVector Point;
			if (Place(B, Point))
			{
				Beat.Add(Point);
			}
		}
		if (Beat.Num() == 0)
		{
			Beat.Add(At);
		}
		Guard->SetBeat(Beat);
		Spawned.Add(Guard);
		GuardList.Add(Guard);
	}
}

void ANHMissions::OpenPlanStep()
{
	const TArray<TSharedPtr<FJsonObject>> Steps = List(Obj(), TEXT("steps"));
	if (!Steps.IsValidIndex(PlanStep))
	{
		return;
	}
	const TSharedPtr<FJsonObject> Step = Steps[PlanStep];
	const TArray<TSharedPtr<FJsonObject>> Options = List(Step, TEXT("options"));
	TArray<FString> Texts;
	for (const TSharedPtr<FJsonObject>& O : Options)
	{
		Texts.Add(Str(O, TEXT("text")));
	}
	Dir()->OpenPanel(FString::Printf(TEXT("THE PLAN  %d / %d"), PlanStep + 1, Steps.Num()), { Str(Step, TEXT("title")) }, Texts, [this, Step, Options](int32 Choice)
	{
		if (UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this); Hustle && Options.IsValidIndex(Choice))
		{
			Hustle->SetFlag(FName(*Str(Step, TEXT("flag"))), static_cast<int32>(Num(Options[Choice], TEXT("value"), Choice + 1)));
			UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: missions: the plan: %s = %s"), *Str(Step, TEXT("flag")), *Str(Options[Choice], TEXT("text")));
		}
		++PlanStep;
		OpenPlanStep();
	});
}

void ANHMissions::Arm()
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	ANHLeads* Leads = ANHLeads::Get(this);
	Radius = static_cast<float>(Num(Obj(), TEXT("radius"), 400.0));
	Seconds = static_cast<float>(Num(Obj(), TEXT("seconds")));
	bHasPlace = Place(Sub(Obj(), TEXT("at")), PlaceAt);

	if (Type == TEXT("enter"))
	{
		if (Flag(Obj(), TEXT("spawn")) && bHasPlace)
		{
			Car = Dir()->SpawnVehicle(FName(*Str(Obj(), TEXT("vehicle"), TEXT("danfo"))), FVector2D(PlaceAt), static_cast<float>(Num(Obj(), TEXT("yaw"), AnchorYaw)), FLinearColor(0.9f, 0.7f, 0.02f), FString());
			if (Car)
			{
				Spawned.Add(Car);
			}
		}
	}
	else if (Type == TEXT("choose"))
	{
		const TArray<TSharedPtr<FJsonObject>> Options = List(Obj(), TEXT("options"));
		TArray<FString> Texts;
		for (const TSharedPtr<FJsonObject>& O : Options)
		{
			Texts.Add(Str(O, TEXT("text")));
		}
		Dir()->OpenPanel(Str(Obj(), TEXT("title"), TEXT("YOUR CALL")), Lines(Obj(), TEXT("lines")), Texts, [this, Options](int32 Choice)
		{
			if (Options.IsValidIndex(Choice))
			{
				Apply(Options[Choice]);
			}
			bChose = true;
		});
	}
	else if (Type == TEXT("plan"))
	{
		OpenPlanStep();
	}
	else if (Type == TEXT("switch"))
	{
		if (Leads)
		{
			Leads->Switch(FName(*Str(Obj(), TEXT("to"))), true);
		}
	}
	else if (Type == TEXT("loseheat"))
	{
		if (const float Stars = static_cast<float>(Num(Obj(), TEXT("stars"))); Hustle && Stars > 0.f)
		{
			Hustle->AddHeat(Stars);
		}
	}
	else if (Type == TEXT("collect") || Type == TEXT("disguise"))
	{
		const int32 Count = Type == TEXT("disguise") ? 1 : FMath::Max(1, static_cast<int32>(Num(Obj(), TEXT("count"), 1.0)));
		const FVector Centre = bHasPlace ? PlaceAt : Anchor - FVector(0.f, 0.f, 92.f);
		for (int32 I = 0; I < Count; ++I)
		{
			// one where the place is; more in a ring round it
			const float Turn = 2.f * UE_PI * I / Count;
			const FVector Off = Count > 1 ? FVector(FMath::Cos(Turn), FMath::Sin(Turn), 0.f) * 280.f : FVector::ZeroVector;
			SpawnPickup(Centre + Off, Type == TEXT("disguise") ? FLinearColor(0.2f, 0.5f, 1.f) : FLinearColor(1.f, 0.77f, 0.f));
		}
		Left = Count;
	}
	else if (Type == TEXT("unlock"))
	{
		if (bHasPlace)
		{
			HackTarget = GetWorld()->SpawnActor<ANHHackPoint>(ANHHackPoint::StaticClass(), PlaceAt, FRotator(0.f, AnchorYaw + 180.f, 0.f));
			if (HackTarget)
			{
				HackTarget->Build(Str(Obj(), TEXT("kind")) == TEXT("light") ? ENHHackKind::Light : ENHHackKind::Camera);
				Spawned.Add(HackTarget);
			}
		}
		if (Leads && Flag(Obj(), TEXT("fillMeter"), true))
		{
			Leads->SetMeter(Leads->Current(), 1.f); // the story hands her the moment; free roam makes her earn it
		}
	}
	else if (Type == TEXT("defeat") || Type == TEXT("sneak") || Type == TEXT("takedown"))
	{
		SpawnGuards();
	}
	bObjectiveReady = true;
}

bool ANHMissions::Done(float DeltaSeconds)
{
	const ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const ANHLeads* Leads = ANHLeads::Get(this);
	if (!Pawn || !Hustle)
	{
		return false;
	}
	const FVector Here = Pawn->GetActorLocation();
	const auto GuardsUp = [this]()
	{
		int32 Up = 0;
		for (const ANHGuard* Guard : GuardList)
		{
			Up += IsValid(Guard) && !Guard->IsDown() ? 1 : 0;
		}
		return Up;
	};

	if (Type == TEXT("talk"))
	{
		return bSaid;
	}
	if (Type == TEXT("goto"))
	{
		return bHasPlace && FVector::Dist2D(Here, PlaceAt) < Radius;
	}
	if (Type == TEXT("enter"))
	{
		const ANHVehicle* In = Cast<ANHVehicle>(Pawn);
		return In && (Car ? In == Car : In->VehicleType == FName(*Str(Obj(), TEXT("vehicle"))));
	}
	if (Type == TEXT("wait"))
	{
		return ObjectiveT >= Seconds;
	}
	if (Type == TEXT("choose"))
	{
		return bChose;
	}
	if (Type == TEXT("plan"))
	{
		return PlanStep >= List(Obj(), TEXT("steps")).Num();
	}
	if (Type == TEXT("switch"))
	{
		return Leads && !Leads->IsSwitching() && Leads->Current() == FName(*Str(Obj(), TEXT("to")));
	}
	if (Type == TEXT("loseheat"))
	{
		return ObjectiveT > 0.5f && Hustle->Stars() == 0;
	}
	if (Type == TEXT("collect") || Type == TEXT("disguise"))
	{
		for (int32 I = Pickups.Num() - 1; I >= 0; --I)
		{
			if (IsValid(Pickups[I]) && FVector::Dist2D(Here, Pickups[I]->GetActorLocation()) < 170.f)
			{
				Pickups[I]->Destroy();
				Pickups.RemoveAt(I);
				--Left;
				if (Type == TEXT("disguise"))
				{
					Disguise = FName(*Str(Obj(), TEXT("outfit")));
					ANHHUD::Toast(this, FString::Printf(TEXT("Dressed as: %s"), *Str(Obj(), TEXT("outfitName"), Disguise.ToString())), 1);
				}
			}
		}
		return Left <= 0;
	}
	if (Type == TEXT("unlock"))
	{
		return HackTarget && HackTarget->IsHacked();
	}
	if (Type == TEXT("defeat") || Type == TEXT("takedown"))
	{
		return GuardList.Num() > 0 && GuardsUp() == 0;
	}
	if (Type == TEXT("sneak"))
	{
		if (Flag(Obj(), TEXT("failOnAlert")))
		{
			for (const ANHGuard* Guard : GuardList)
			{
				if (IsValid(Guard) && Guard->IsAlert())
				{
					Fail(TEXT("They saw you."));
					return false;
				}
			}
		}
		return bHasPlace && FVector::Dist2D(Here, PlaceAt) < Radius;
	}
	UE_LOG(LogNHGame, Warning, TEXT("NAIJA HUSTLE: missions: %s: objective type '%s' is not one this runner knows; passed over"), *Id.ToString(), *Type);
	return true;
}

void ANHMissions::Apply(const TSharedPtr<FJsonObject>& Effects)
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	if (!Effects.IsValid() || !Hustle)
	{
		return;
	}
	if (const TSharedPtr<FJsonObject> Flags = Sub(Effects, TEXT("flags")))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& F : Flags->Values)
		{
			Hustle->SetFlag(FName(*F.Key), static_cast<int32>(F.Value->AsNumber()));
			UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: missions: flag %s = %d"), *F.Key, static_cast<int32>(F.Value->AsNumber()));
		}
	}
	if (const int32 Integrity = static_cast<int32>(Num(Effects, TEXT("integrity"))); Integrity != 0)
	{
		Hustle->Integrity = FMath::Clamp(Hustle->Integrity + Integrity, -100, 100);
		Last.Integrity += Integrity;
	}
	if (const int32 Cash = static_cast<int32>(Num(Effects, TEXT("cash"))); Cash != 0)
	{
		Hustle->Earn(Cash, Title);
	}
	if (const FString Toast = Str(Effects, TEXT("toast")); !Toast.IsEmpty())
	{
		ANHHUD::Toast(this, Toast, 1);
	}
}

void ANHMissions::End()
{
	Times.Add(ObjectiveT);
	Apply(Sub(Obj(), TEXT("onDone")));
	Clear();
	if (Index + 1 < Objectives.Num())
	{
		Begin(Index + 1);
	}
	else
	{
		Finish();
	}
}

void ANHMissions::SkipObjective()
{
	if (bActive && Night == ENight::None && Objectives.IsValidIndex(Index) && !bFailed)
	{
		if (ANHGameDirector* D = Dir())
		{
			D->Panel = ANHGameDirector::FPanel();
			D->Dialogue = ANHGameDirector::FDialogue();
		}
		bSaid = true;
		End();
	}
}

void ANHMissions::Fail(const FString& Why)
{
	if (!bActive || bFailed || Night != ENight::None)
	{
		return;
	}
	bFailed = true;
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: missions: %s failed at objective %d after %s: %s"), *Id.ToString(), Index + 1, *Clock(MissionT), *Why);
	ANHGameDirector* D = Dir();
	D->Dialogue = ANHGameDirector::FDialogue();
	D->OpenPanel(TEXT("JOB FAILED"), { Why, FString::Printf(TEXT("Checkpoint: objective %d. The clock keeps running."), Checkpoint.Index + 1) }, { TEXT("Restart from the checkpoint"), TEXT("Give the job up") }, [this](int32 Choice)
	{
		if (Choice == 0)
		{
			RestartFromCheckpoint();
		}
		else
		{
			Abort(TEXT("given up at the failure card"));
		}
	});
}

void ANHMissions::RestartFromCheckpoint()
{
	if (!bActive)
	{
		return;
	}
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	ANHLeads* Leads = ANHLeads::Get(this);
	Clear();
	++RestartCount;
	bFailed = false;
	if (ANHGameDirector* D = Dir())
	{
		D->Panel = ANHGameDirector::FPanel();
	}
	if (PC && Cast<ANHVehicle>(PC->GetPawn()))
	{
		PC->LeaveVehicle(true);
	}
	if (ANHCharacter* Me = Player())
	{
		Me->Health = 100.f;
		Me->SetActorLocationAndRotation(Checkpoint.At, FRotator(0.f, Checkpoint.Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (Hustle)
	{
		Hustle->ClearHeat();
	}
	Disguise = Checkpoint.Disguise;
	// the checkpoint's lead, if the mission had moved on to the other one: the first objective waits for the switch
	if (Leads && !Checkpoint.Lead.IsNone() && Leads->Current() != Checkpoint.Lead)
	{
		Leads->Switch(Checkpoint.Lead, true);
	}
	Index = Checkpoint.Index - 1; // Tick begins the next objective once nobody is switching
	bObjectiveReady = false;
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: missions: %s back to the checkpoint at objective %d (restart %d), %s on the clock"), *Id.ToString(), Checkpoint.Index + 1, RestartCount, *Clock(MissionT));
}

bool ANHMissions::OnAction(APawn* Pawn)
{
	if (!bActive || !Pawn || !bObjectiveReady || bFailed)
	{
		return false;
	}
	for (ANHGuard* Guard : GuardList)
	{
		if (IsValid(Guard) && Guard->TakeDown(Pawn->GetActorLocation()))
		{
			ANHHUD::Floater(this, Guard->GetActorLocation() + FVector(0.f, 0.f, 150.f), TEXT("down"));
			UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: missions: a guard taken down from behind"));
			return true;
		}
	}
	return false;
}

FString ANHMissions::ActionPrompt(const APawn* Pawn) const
{
	if (bActive && Pawn && bObjectiveReady && !bFailed)
	{
		for (const ANHGuard* Guard : GuardList)
		{
			if (IsValid(Guard) && Guard->CanBeTakenDown(Pawn->GetActorLocation()))
			{
				return TEXT("E  Take down");
			}
		}
	}
	return FString();
}

// ---------------------------------------------------------------------------------------------------- the end of a mission
void ANHMissions::Finish()
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const TSharedPtr<FJsonObject> Rewards = Sub(Root, TEXT("rewards"));
	Last.bDone = true;
	Last.Seconds = MissionT;
	Last.ObjectiveSeconds = Times;
	Last.Restarts = RestartCount;
	Last.Cred = static_cast<int32>(Num(Rewards, TEXT("cred")));
	Last.GoldKobo = static_cast<int32>(Num(Rewards, TEXT("goldKobo")));
	Last.bTooLong = MissionT > LengthLimit;
	if (const int32 Integrity = static_cast<int32>(Num(Rewards, TEXT("integrity"))); Hustle && Integrity != 0)
	{
		Hustle->Integrity = FMath::Clamp(Hustle->Integrity + Integrity, -100, 100);
		Last.Integrity += Integrity;
	}
	if (Hustle)
	{
		Hustle->Cred += Last.Cred;
		Hustle->Done.AddUnique(Id);
		++Hustle->Jobs;
	}
	GoldKoboEarned += Last.GoldKobo;

	// the clock, for the length rule
	FString Each;
	for (int32 I = 0; I < Times.Num(); ++I)
	{
		Each += FString::Printf(TEXT("%s%d %s"), I ? TEXT(", ") : TEXT(""), I + 1, *Clock(Times[I]));
	}
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: missions: %s done in %s (limit %s%s), %d restarts; objectives: %s"), *Id.ToString(), *Clock(MissionT), *Clock(LengthLimit),
		Last.bTooLong ? TEXT(", OVER") : TEXT(""), RestartCount, *Each);

	TArray<FString> CardLines;
	if (const FString Standout = Str(Root, TEXT("standout")); !Standout.IsEmpty())
	{
		CardLines.Add(Standout);
	}
	CardLines.Add(FString::Printf(TEXT("Time %s"), *Clock(MissionT)));
	CardLines.Add(FString::Printf(TEXT("Cred +%d    Integrity %+d    Gold Kobo +%d"), Last.Cred, Last.Integrity, Last.GoldKobo));
	const bool bNight = Flag(Root, TEXT("nightShift"), Number >= 2);
	Night = ENight::Card;
	Dir()->OpenPanel(FString::Printf(TEXT("JOB DONE: %s"), *Title.ToUpper()), CardLines, { bNight ? TEXT("On to the night shift") : TEXT("Done") }, [this, bNight](int32)
	{
		if (bNight)
		{
			OpenNightShift();
		}
		else
		{
			PayNightShift(false);
		}
	});
}

void ANHMissions::OpenNightShift()
{
	Night = ENight::Card;
	Dir()->OpenPanel(TEXT("NIGHT SHIFT"), { TEXT("Tunde takes the danfo out for the night."), TEXT("Drive the short route yourself, or go straight to the takings.") },
		{ TEXT("Drive the night route"), TEXT("Skip to the takings") }, [this](int32 Choice) { NightShiftChoice(Choice == 0); });
}

void ANHMissions::NightShiftChoice(bool bDrive)
{
	if (!bActive || Night != ENight::Card)
	{
		return;
	}
	if (ANHGameDirector* D = Dir())
	{
		D->Panel = ANHGameDirector::FPanel();
	}
	if (!bDrive)
	{
		PayNightShift(false);
		return;
	}
	// it is Tunde's bus: the drive is his
	if (ANHLeads* Leads = ANHLeads::Get(this); Leads && Leads->Find(TEXT("tunde")) && Leads->Current() != TEXT("tunde"))
	{
		Leads->Switch(TEXT("tunde"), true);
	}
	Night = ENight::Drive;
	NightDriven = 0.f;
	NightRoute = static_cast<float>(Num(Root, TEXT("nightRoute"), 30000.0)); // 300 m, unless the mission says
	NightBus = nullptr;
}

void ANHMissions::PayNightShift(bool bDrove)
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const int32 As = static_cast<int32>(Num(Root, TEXT("nightShiftAs"), Number));
	Last.Pay = Flag(Root, TEXT("nightShift"), Number >= 2) ? PayFor(As) : 0;
	if (Hustle && Last.Pay > 0)
	{
		Hustle->Earn(Last.Pay, TEXT("Night shift"));
	}
	Night = ENight::Paid;
	const auto Close = [this]()
	{
		if (IsValid(NightBus) && !NightBus->GetController())
		{
			NightBus->Destroy();
		}
		NightBus = nullptr;
		Night = ENight::None;
		bActive = false;
		Disguise = NAME_None;
		if (ANHLeads* Leads = ANHLeads::Get(this))
		{
			Leads->SetLocked(false);
		}
		if (ANHGameDirector* D = Dir())
		{
			D->bMarker = false;
		}
		if (UNHHustleSubsystem* H = UNHHustleSubsystem::Get(this))
		{
			H->Save();
		}
	};
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: missions: %s night shift (%s): %d naira"), *Id.ToString(), bDrove ? TEXT("driven") : TEXT("skipped"), Last.Pay);
	if (Last.Pay <= 0)
	{
		Close();
		return;
	}
	Dir()->OpenPanel(TEXT("NIGHT SHIFT TAKINGS"), { FString::Printf(TEXT("The night's takings: %s"), *UNHHustleSubsystem::Naira(Last.Pay)), bDrove ? TEXT("You drove it yourself.") : TEXT("Tunde drove; you counted.") },
		{ TEXT("Done") }, [Close](int32) { Close(); });
}

// ---------------------------------------------------------------------------------------------------- every frame
void ANHMissions::Card()
{
	ANHGameDirector* D = Dir();
	if (!D)
	{
		return;
	}
	D->DeadlineMinutesLeft = -1.f;
	D->RouteStopPoints.Reset();
	D->ObjTitle = Title.ToUpper();
	if (Night == ENight::Drive)
	{
		D->ObjTitle = TEXT("NIGHT SHIFT");
		const bool bIn = NightBus && NightBus->GetController();
		D->ObjText = bIn ? TEXT("Drive the night route") : TEXT("Get in the danfo");
		D->ObjSub = bIn ? FString::Printf(TEXT("%d m to go"), FMath::Max(0, FMath::RoundToInt((NightRoute - NightDriven) / 100.f))) : TEXT("F to get in");
		D->bMarker = NightBus && !bIn;
		D->Marker = NightBus ? NightBus->GetActorLocation() : FVector::ZeroVector;
		return;
	}
	if (!Objectives.IsValidIndex(Index) || Night != ENight::None)
	{
		D->ObjText = Night != ENight::None ? TEXT("Job done") : TEXT("Get ready");
		D->ObjSub.Reset();
		D->bMarker = false;
		return;
	}
	D->ObjText = Str(Obj(), TEXT("text"));
	D->ObjSub = Str(Obj(), TEXT("sub"));
	if (Type == TEXT("wait"))
	{
		D->ObjSub = FString::Printf(TEXT("%s  %d s"), *D->ObjSub, FMath::Max(0, FMath::CeilToInt(Seconds - ObjectiveT)));
	}
	else if (Type == TEXT("collect") && Left > 0)
	{
		D->ObjSub = FString::Printf(TEXT("%s  %d to go"), *D->ObjSub, Left);
	}
	// where to: the place, or the thing that stands for it
	const AActor* Thing = Car ? static_cast<const AActor*>(Car.Get()) : HackTarget ? static_cast<const AActor*>(HackTarget.Get()) : Pickups.Num() > 0 && IsValid(Pickups[0]) ? Pickups[0].Get() : nullptr;
	D->bMarker = bHasPlace || Thing;
	D->Marker = Thing ? Thing->GetActorLocation() : PlaceAt;
}

void ANHMissions::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActive || bCardOpen)
	{
		return;
	}
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	const ANHLeads* Leads = ANHLeads::Get(this);
	ANHCharacter* Me = Player();
	ANHGameDirector* D = Dir();
	if (!PC || !D)
	{
		return;
	}
	const bool bSwitching = Leads && Leads->IsSwitching();

	// ---- the night shift's short drive
	if (Night == ENight::Drive)
	{
		if (!bSwitching && !NightBus && Me)
		{
			const FVector Ahead = Me->GetActorLocation() + Me->GetActorForwardVector() * 700.f;
			NightBus = D->SpawnVehicle(TEXT("danfo"), FVector2D(Ahead), Me->GetActorRotation().Yaw, FLinearColor(0.9f, 0.7f, 0.02f), TEXT("NIGHT"));
			NightFrom = Ahead;
		}
		if (NightBus && NightBus->GetController())
		{
			NightDriven += FMath::Abs(NightBus->Speed) * DeltaSeconds;
			if (NightDriven >= NightRoute)
			{
				PayNightShift(true);
			}
		}
		Card();
		return;
	}
	if (Night != ENight::None)
	{
		Card();
		return;
	}

	MissionT += DeltaSeconds;
	// the first objective, or the one a checkpoint goes back to, once the player is in the right shoes
	if (!Objectives.IsValidIndex(Index) || !bObjectiveReady)
	{
		if (!bSwitching && !bFailed)
		{
			Begin(FMath::Max(Index + 1, 0));
		}
		Card();
		return;
	}
	if (bFailed)
	{
		return;
	}
	ObjectiveT += DeltaSeconds;

	// knocked down in a mission: back to the checkpoint, not to the street
	if (Me && Me->Health <= 0.f)
	{
		Me->Health = 1.f;
		Fail(TEXT("You went down."));
		return;
	}
	// what the guards can hear: running feet close by, a shot from far off
	NoiseWait -= DeltaSeconds;
	if (NoiseWait <= 0.f && Me && GuardList.Num() > 0)
	{
		NoiseWait = 0.3f;
		const bool bGun = Me->Equipped() == TEXT("pistol") || Me->Equipped() == TEXT("ak47");
		const float Loud = Me->Attacks > ShotsHeard ? (bGun ? 4000.f : 600.f) : Me->IsSprinting() && !Me->bIsCrouched && Me->GetVelocity().SizeSquared2D() > 400.f * 400.f ? 900.f : 0.f;
		ShotsHeard = Me->Attacks;
		if (Loud > 0.f)
		{
			for (ANHGuard* Guard : GuardList)
			{
				if (IsValid(Guard))
				{
					Guard->Hear(Me->GetActorLocation(), Loud);
				}
			}
		}
	}

	if (bSaid && !D->Dialogue.bOpen && Done(DeltaSeconds) && !bFailed)
	{
		End();
	}
	if (bActive)
	{
		Card();
	}
}
