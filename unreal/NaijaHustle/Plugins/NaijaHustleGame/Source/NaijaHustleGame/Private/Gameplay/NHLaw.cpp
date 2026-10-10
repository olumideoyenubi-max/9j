#include "Gameplay/NHLaw.h"

#include "Core/NHGameData.h"
#include "Core/NHHustleSubsystem.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Gameplay/NHPerson.h"
#include "Gameplay/NHResponse.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "NaijaHustleGame.h"
#include "Phone/NHPhone.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UI/NHHUD.h"

namespace NHLawNames
{
	const TCHAR* Crimes[] = { TEXT("GunFired"), TEXT("VehicleDamage"), TEXT("Assault"), TEXT("Shooting"), TEXT("Killing") };
	const TCHAR* Reactions[] = { TEXT("ignore"), TEXT("film"), TEXT("shout"), TEXT("report") };
}

ANHLaw::ANHLaw()
{
	PrimaryActorTick.bCanEverTick = true;
}

ANHLaw* ANHLaw::Get(const UObject* WorldContext)
{
	return WorldContext ? Cast<ANHLaw>(UGameplayStatics::GetActorOfClass(WorldContext, ANHLaw::StaticClass())) : nullptr;
}

void ANHLaw::BeginPlay()
{
	Super::BeginPlay();
	static_assert(UE_ARRAY_COUNT(NHLawNames::Crimes) == static_cast<int32>(ENHCrime::Count), "one name per crime");
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *(UNHGameData::DataDir() / TEXT("law.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
	{
		UE_LOG(LogNHGame, Warning, TEXT("NAIJA HUSTLE: law: Data/law.json could not be read; using what is built in"));
		return;
	}
	double N = 0.0;
	SeeCm = Root->TryGetNumberField(TEXT("seeMetres"), N) ? N * 100.f : SeeCm;
	HearCm = Root->TryGetNumberField(TEXT("hearMetres"), N) ? N * 100.f : HearCm;
	StopCm = Root->TryGetNumberField(TEXT("stopMetres"), N) ? N * 100.f : StopCm;
	Root->TryGetNumberField(TEXT("callSeconds"), CallSeconds);
	Root->TryGetNumberField(TEXT("highClassReport"), HighClassReport);
	Root->TryGetNumberField(TEXT("nightReport"), NightReport);
	Root->TryGetNumberField(TEXT("victimReport"), VictimReport);
	Root->TryGetNumberField(TEXT("heardOnly"), HeardOnly);
	const TSharedPtr<FJsonObject>* Section = nullptr;
	if (Root->TryGetObjectField(TEXT("reactions"), Section))
	{
		for (int32 S = 0; S < 2; ++S)
		{
			const TSharedPtr<FJsonObject>* Row = nullptr;
			if ((*Section)->TryGetObjectField(S == 0 ? TEXT("minor") : TEXT("serious"), Row))
			{
				for (int32 R = 0; R < 4; ++R)
				{
					(*Row)->TryGetNumberField(NHLawNames::Reactions[R], Weights[S][R]);
				}
			}
		}
	}
	if (Root->TryGetObjectField(TEXT("crimes"), Section))
	{
		for (int32 C = 0; C < static_cast<int32>(ENHCrime::Count); ++C)
		{
			const TSharedPtr<FJsonObject>* Row = nullptr;
			if ((*Section)->TryGetObjectField(NHLawNames::Crimes[C], Row))
			{
				(*Row)->TryGetBoolField(TEXT("serious"), Rules[C].bSerious);
				(*Row)->TryGetBoolField(TEXT("loud"), Rules[C].bLoud);
				(*Row)->TryGetNumberField(TEXT("stars"), Rules[C].Stars);
			}
		}
	}
	if (Root->TryGetObjectField(TEXT("lines"), Section))
	{
		for (const auto& KV : (*Section)->Values)
		{
			TArray<FString> Out;
			(*Section)->TryGetStringArrayField(*KV.Key, Out);
			Lines.Add(FString(*KV.Key), MoveTemp(Out));
		}
	}
}

FString ANHLaw::Line(const TCHAR* Kind) const
{
	const TArray<FString>* Found = Lines.Find(Kind);
	return Found && Found->Num() ? (*Found)[FMath::RandRange(0, Found->Num() - 1)] : FString();
}

bool ANHLaw::CanSee(const ANHPerson* Who, const FVector& At) const
{
	FCollisionQueryParams Query(SCENE_QUERY_STAT(NHWitness), false, Who);
	Query.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this, 0));
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Who->GetActorLocation() + FVector(0.f, 0.f, 150.f), At + FVector(0.f, 0.f, 60.f), ECC_Visibility, Query);
}

void ANHLaw::Crime(ENHCrime Kind, const FVector& At, ANHPerson* Victim)
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const ANHResponse* Response = ANHResponse::Get(this);
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Hustle || !Pawn)
	{
		return;
	}
	if (Kind == ENHCrime::GunFired)
	{
		if (GetWorld()->GetTimeSeconds() - LastGunAt < 1.f)
		{
			return;
		}
		LastGunAt = GetWorld()->GetTimeSeconds();
	}
	++Crimes;
	const FCrimeRule& Rule = Rules[static_cast<int32>(Kind)];
	const int32 Index = Known.Add({ Rule.Stars, 0.f });
	const FVector Player = Pawn->GetActorLocation();
	const ENHArea Area = Response ? Response->AreaAt(Player) : ENHArea::Ordinary;
	const float Hour = Hustle->HourOfDay();
	const bool bNight = Hour >= 22.f || Hour < 5.f;
	const float Now = GetWorld()->GetTimeSeconds();
	bool bAnyone = false;
	for (TActorIterator<ANHPerson> It(GetWorld()); It; ++It)
	{
		ANHPerson* Who = *It;
		if (Who->IsDown())
		{
			continue;
		}
		const float Distance = FVector::Dist(Who->GetActorLocation(), Player);
		const bool bSaw = Distance < SeeCm && CanSee(Who, Player);
		const bool bHeard = Rule.bLoud && Distance < HearCm;
		if (!bSaw && !bHeard)
		{
			continue;
		}
		bAnyone = true;
		if (Who->bBrave)
		{
			// one of those sent for the player: it counts at once and in full
			if (bSaw)
			{
				Hustle->AddHeat(Count(Index, true));
			}
			continue;
		}
		if (Who->bEssential)
		{
			continue; // he says his piece when he is hurt (ANHPerson::Hurt), and that is all
		}
		// somebody already on the phone about the player adds this to what they are saying
		if (FCall* Call = Ringing.FindByPredicate([Who](const FCall& C) { return C.Who.Get() == Who; }))
		{
			Call->About.Add(Index, bSaw);
			Call->bSawPlayer |= bSaw;
			continue;
		}
		if (const float* When = Decided.Find(Who); When && Now - *When < 8.f)
		{
			continue; // made up their mind a moment ago
		}
		Decided.Add(Who, Now);
		float W[4];
		FMemory::Memcpy(W, Weights[Rule.bSerious ? 1 : 0], sizeof(W));
		W[3] *= (Area == ENHArea::HighClass ? HighClassReport : 1.f) * (bNight ? NightReport : 1.f) * (Who == Victim ? VictimReport : 1.f);
		// on area boys' streets nobody calls the Task Force: the same people call the boys instead, and it comes to the same stars
		float Roll = FMath::FRand() * (W[0] + W[1] + W[2] + W[3]);
		int32 Does = 0;
		for (; Does < 3 && Roll >= W[Does]; ++Does)
		{
			Roll -= W[Does];
		}
		const FVector Head = Who->GetActorLocation() + FVector(0.f, 0.f, 215.f);
		switch (Does)
		{
		case 0:
			++Ignored;
			if (FMath::RandRange(0, 2) == 0)
			{
				ANHHUD::Floater(this, Head, Line(TEXT("ignore")));
			}
			break;
		case 1:
			++Filmed;
			ANHHUD::Floater(this, Head, Line(TEXT("film")));
			Films.Add({ 5.f });
			break;
		case 2:
			++Shouted;
			ANHHUD::Floater(this, Head, Line(TEXT("shout")));
			break;
		default:
		{
			++Calls;
			FCall Call;
			Call.Who = Who;
			Call.Left = CallSeconds;
			Call.About.Add(Index, bSaw);
			Call.bSawPlayer = bSaw;
			Call.bBoys = Area == ENHArea::AreaBoys;
			Ringing.Add(MoveTemp(Call));
			ANHHUD::Floater(this, Head, Line(Area == ENHArea::AreaBoys ? TEXT("boys") : TEXT("report")));
			break;
		}
		}
	}
	Unseen += bAnyone ? 0 : 1;
	UE_LOG(LogNHGame, Verbose, TEXT("NAIJA HUSTLE: law: %s: %s"), NHLawNames::Crimes[static_cast<int32>(Kind)], bAnyone ? TEXT("seen or heard") : TEXT("nobody saw or heard it"));
}

float ANHLaw::Count(int32 Index, bool bSaw)
{
	if (!Known.IsValidIndex(Index))
	{
		return 0.f;
	}
	const float More = FMath::Max(Known[Index].Stars * (bSaw ? 1.f : HeardOnly) - Known[Index].Counted, 0.f);
	Known[Index].Counted += More;
	StarsReported += More;
	return More;
}

void ANHLaw::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Hustle || !Pawn)
	{
		return;
	}
	for (int32 I = Ringing.Num() - 1; I >= 0; --I)
	{
		FCall& Call = Ringing[I];
		ANHPerson* Who = Call.Who.Get();
		if (!Who || Who->IsDown())
		{
			++CallsStopped; // the call was never finished
			Ringing.RemoveAtSwap(I);
			continue;
		}
		if (FVector::Dist(Who->GetActorLocation(), Pawn->GetActorLocation()) < StopCm)
		{
			// caught up with: they put the phone away
			++CallsStopped;
			ANHHUD::Floater(this, Who->GetActorLocation() + FVector(0.f, 0.f, 215.f), Line(TEXT("stopped")));
			Ringing.RemoveAtSwap(I);
			continue;
		}
		Call.Left -= DeltaSeconds;
		if (Call.Left <= 0.f)
		{
			++CallsMade;
			float Stars = 0.f;
			for (const TPair<int32, bool>& One : Call.About)
			{
				Stars += Count(One.Key, One.Value);
			}
			Hustle->AddHeat(Stars);
			ANHHUD::Toast(this, Call.bBoys ? TEXT("Somebody don call area boys for you") : Call.bSawPlayer ? TEXT("Somebody don report you to Task Force") : TEXT("Somebody don report am: suspect unknown"), 2);
			UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: law: a report was made (%.1f stars more%s)"), Stars, Call.bSawPlayer ? TEXT("") : TEXT(", heard only"));
			Ringing.RemoveAtSwap(I);
		}
	}
	// what was filmed turns up on Yarns a few seconds later
	for (int32 I = Films.Num() - 1; I >= 0; --I)
	{
		Films[I].Left -= DeltaSeconds;
		if (Films[I].Left <= 0.f)
		{
			Films.RemoveAtSwap(I);
			const UNHGameData* Data = UNHGameData::Get(this);
			ANHPhone* Phone = ANHPhone::Get(this);
			FString Post = Line(TEXT("yarns"));
			if (Phone && Data && !Post.IsEmpty())
			{
				const float Hour = Hustle->HourOfDay();
				Post.ReplaceInline(TEXT("{district}"), *Data->DistrictAt(Pawn->GetActorLocation()));
				Post.ReplaceInline(TEXT("{time}"), Hour < 12.f ? TEXT("morning") : Hour < 17.f ? TEXT("afternoon") : TEXT("night"));
				Phone->Post(TEXT("Lagos Eye"), TEXT("@lagos_eye"), Post, true);
			}
		}
	}
	// forget who decided what long ago, and anybody gone
	if (Decided.Num() > 64)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		for (auto It = Decided.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid() || Now - It.Value() > 30.f)
			{
				It.RemoveCurrent();
			}
		}
	}
}

FString ANHLaw::Describe() const
{
	return FString::Printf(TEXT("law: %d crimes, %d of them seen or heard by nobody; witnesses: %d ignored it, %d filmed, %d shouted, %d began a report (%d made, %d stopped, %d still on the phone); %.1f stars from reports"),
		Crimes, Unseen, Ignored, Filmed, Shouted, Calls, CallsMade, CallsStopped, Ringing.Num(), StarsReported);
}
