#include "Gameplay/NHGuard.h"

#include "Audio/NHEngineWave.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/NHMissions.h"
#include "NaijaHustleGame.h"
#include "Player/NHCharacter.h"
#include "Player/NHPlayerController.h"
#include "UI/NHHUD.h"

ANHGuard::ANHGuard()
{
	bBrave = true; // its own nerve is decided here (bCoward), not by ANHPerson's running from every blow
}

void ANHGuard::SetBeat(const TArray<FVector>& Points)
{
	Beat = Points;
	BeatIndex = 0;
	PostYaw = GetActorRotation().Yaw;
	if (Beat.Num() > 1)
	{
		WalkTo(Beat[0], 130.f);
	}
}

void ANHGuard::Go(ENHGuardState To)
{
	if (State == To)
	{
		return;
	}
	const TCHAR* Names[] = { TEXT("patrol"), TEXT("investigate"), TEXT("chase"), TEXT("attack"), TEXT("flee") };
	UE_LOG(LogNHGame, Verbose, TEXT("NAIJA HUSTLE: guard %s: %s -> %s"), *GetName(), Names[static_cast<int32>(State)], Names[static_cast<int32>(To)]);
	if (To == ENHGuardState::Chase && State != ENHGuardState::Attack)
	{
		const FString Shout = NHBarks::Pick(TEXT("guard_alert"));
		ANHHUD::Say(this, GetActorLocation() + FVector(0.f, 0.f, 210.f), Shout.IsEmpty() ? FString(TEXT("!")) : Shout, GetName());
	}
	else if (To == ENHGuardState::Investigate)
	{
		const FString Asks = NHBarks::Pick(TEXT("guard_look"));
		ANHHUD::Say(this, GetActorLocation() + FVector(0.f, 0.f, 210.f), Asks.IsEmpty() ? FString(TEXT("?")) : Asks, GetName());
		++Investigated;
	}
	if (To == ENHGuardState::Flee)
	{
		bBrave = false; // now it runs like anybody
	}
	State = To;
	StateT = 0.f;
}

bool ANHGuard::Sees(const ANHCharacter* Player, float& OutDistance) const
{
	const FVector Eyes = GetActorLocation() + FVector(0.f, 0.f, 160.f), Them = Player->GetActorLocation() + FVector(0.f, 0.f, Player->bIsCrouched ? 20.f : 60.f);
	const FVector To = Them - Eyes;
	OutDistance = static_cast<float>(To.Size());
	float Range = SightRange * (Player->bIsCrouched ? 0.5f : 1.f);
	// a disguise it believes: it sees only what is right in front of it, unless the player gives the game away
	const ANHMissions* Missions = ANHMissions::Get(this);
	if (Missions && FooledBy.Contains(Missions->Disguise) && !Player->IsSprinting() && Player->Equipped().IsNone())
	{
		Range = FMath::Min(Range, 220.f);
	}
	if (OutDistance > Range)
	{
		return false;
	}
	const float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(GetActorForwardVector(), To.GetSafeNormal2D())));
	if (Angle > ConeHalfAngle && OutDistance > 150.f) // at arm's length it notices whoever is there
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NHGuardSight), false, this);
	Params.AddIgnoredActor(Player);
	return !GetWorld()->LineTraceSingleByObjectType(Hit, Eyes, Them, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
}

void ANHGuard::Hear(const FVector& At, float Radius)
{
	if (IsDown() || IsAlert() || State == ENHGuardState::Flee || FVector::Dist(At, GetActorLocation()) > Radius)
	{
		return;
	}
	LookAt = At;
	Go(ENHGuardState::Investigate);
	WalkTo(At, 180.f);
}

bool ANHGuard::CanBeTakenDown(const FVector& From) const
{
	if (IsDown() || IsAlert())
	{
		return false;
	}
	const FVector To = (From - GetActorLocation()).GetSafeNormal2D();
	return FVector::Dist2D(From, GetActorLocation()) < 190.f && FVector::DotProduct(GetActorForwardVector(), To) < -0.2f;
}

bool ANHGuard::TakeDown(const FVector& From)
{
	if (!CanBeTakenDown(From))
	{
		return false;
	}
	Hurt(Health + 1.f, From);
	return IsDown();
}

void ANHGuard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (IsDown())
	{
		return;
	}
	const ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	ANHCharacter* Player = PC ? Cast<ANHCharacter>(PC->GetPawn()) : nullptr;
	StateT += DeltaSeconds;
	StrikeWait -= DeltaSeconds;
	Think -= DeltaSeconds;

	// ---- what it can see, ten times a second
	if (Think <= 0.f)
	{
		Think = 0.1f;
		float Distance = 0.f;
		bSeesNow = Player && Player->Health > 0.f && Sees(Player, Distance);
		if (bSeesNow)
		{
			// across the road it takes a couple of seconds to be sure; at arm's length, a moment
			Aware = FMath::Min(1.f, Aware + 0.1f * FMath::GetMappedRangeValueClamped(FVector2D(200.f, SightRange), FVector2D(4.f, 0.45f), Distance));
			LastSeen = Player->GetActorLocation();
			LostFor = 0.f;
		}
		else
		{
			Aware = FMath::Max(0.f, Aware - 0.1f * (IsAlert() ? 0.f : 0.25f));
			LostFor += 0.1f;
		}
		if (Aware >= 1.f && !IsAlert() && State != ENHGuardState::Flee)
		{
			Go(ENHGuardState::Chase);
		}
		else if (Aware > 0.45f && State == ENHGuardState::Patrol && bSeesNow)
		{
			// half seen: goes to look
			LookAt = LastSeen;
			Go(ENHGuardState::Investigate);
			WalkTo(LookAt, 150.f);
		}
		if (bCoward && Health < 35.f && State != ENHGuardState::Flee)
		{
			Go(ENHGuardState::Flee);
		}
	}

	switch (State)
	{
	case ENHGuardState::Patrol:
		if (Beat.Num() > 1)
		{
			if (!IsWalking())
			{
				BeatIndex = (BeatIndex + 1) % Beat.Num();
				WalkTo(Beat[BeatIndex], 130.f);
			}
		}
		else if (Beat.Num() == 1 && FVector::Dist2D(GetActorLocation(), Beat[0]) > 80.f)
		{
			if (!IsWalking())
			{
				WalkTo(Beat[0], 130.f); // back to its post
			}
		}
		else if (!IsWalking())
		{
			SetActorRotation(FRotator(0.f, PostYaw, 0.f)); // at its post, facing the way it was set
		}
		break;

	case ENHGuardState::Investigate:
		if (!IsWalking())
		{
			// there: looks about, then back to the beat
			SetActorRotation(FRotator(0.f, GetActorRotation().Yaw + 70.f * DeltaSeconds, 0.f));
			if (StateT > 5.f)
			{
				Aware = FMath::Min(Aware, 0.3f);
				Go(ENHGuardState::Patrol);
				if (Beat.Num() > 0)
				{
					WalkTo(Beat[BeatIndex % Beat.Num()], 130.f);
				}
			}
		}
		break;

	case ENHGuardState::Chase:
	case ENHGuardState::Attack:
		if (!Player || Player->Health <= 0.f)
		{
			Go(ENHGuardState::Patrol);
			break;
		}
		if (LostFor > 6.f)
		{
			// lost them: to where they were last seen, then the beat again
			Aware = 0.6f;
			LookAt = LastSeen;
			Go(ENHGuardState::Investigate);
			WalkTo(LookAt, 180.f);
			break;
		}
		{
			const float Gap = FVector::Dist2D(GetActorLocation(), Player->GetActorLocation());
			const float Reach = bArmed ? 1500.f : 170.f;
			if (Gap < Reach && bSeesNow)
			{
				Go(ENHGuardState::Attack);
				StopWalking();
				FaceTowards(Player->GetActorLocation());
				if (StrikeWait <= 0.f)
				{
					StrikeWait = bArmed ? 0.9f : 1.1f;
					// a shot mostly misses somebody crouched: that is what cover is worth
					const bool bLands = !bArmed || FMath::FRand() < (Player->bIsCrouched ? 0.2f : 0.45f);
					if (bLands)
					{
						Player->Hurt(bArmed ? ShotDamage : MeleeDamage);
						++Landed;
					}
				}
			}
			else
			{
				Go(ENHGuardState::Chase);
				WalkTo(bSeesNow ? Player->GetActorLocation() : LastSeen, 420.f);
			}
		}
		break;

	case ENHGuardState::Flee:
		if (!IsFleeing() && Player)
		{
			Scare(Player->GetActorLocation());
		}
		break;
	}
}
