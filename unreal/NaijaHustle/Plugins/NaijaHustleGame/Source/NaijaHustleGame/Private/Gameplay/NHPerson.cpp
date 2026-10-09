#include "Gameplay/NHPerson.h"

#include "EngineUtils.h"
#include "NaijaHustleGame.h"
#include "UI/NHHUD.h"

#include "Animation/AnimSequence.h"
#include "Characters/NHOutfitComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "Player/NHCharacter.h"
#include "Engine/World.h"
#include "World/NHShapes.h"

namespace NHPeople
{
	// a realistic range of Nigerian skin tones, sRGB
	const FColor Skin[] = { FColor(0x3b, 0x24, 0x19), FColor(0x4a, 0x2e, 0x1f), FColor(0x5c, 0x3a, 0x26), FColor(0x6b, 0x44, 0x2c), FColor(0x7a, 0x50, 0x35), FColor(0x8d, 0x5e, 0x3c) };
	const FColor Bottoms[] = { FColor(0x1f, 0x2a, 0x44), FColor(0x2b, 0x2b, 0x2b), FColor(0x5d, 0x5a, 0x3e), FColor(0x6b, 0x4f, 0x3a), FColor(0x3d, 0x5a, 0x80) };
	const FColor Ties[] = { FColor(0xf5, 0xc4, 0x00), FColor(0xe5, 0x39, 0x35), FColor(0x43, 0xa0, 0x47), FColor(0x1e, 0x88, 0xe5) };

	float Rand(int32 Seed, int32 K) { return FMath::Frac(FMath::Sin(Seed * 12.9898f + K * 78.233f) * 43758.5453f); }
}

ANHPerson::ANHPerson()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void ANHPerson::Init(int32 Seed, const FLinearColor& Top, ENHCast InCast, float Scale)
{
	Part = InCast;
	const bool bHeadTie = Part == ENHCast::Woman; // the blockout's only sign of a woman
	using namespace NHPeople;
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	if (BuildBody(Seed, Top, Scale))
	{
		SnapToGround();
		return;
	}
	const float H = (0.92f + 0.14f * Rand(Seed, 1)) * Scale; // height factor
	const FNHSurface SkinS(FLinearColor(Skin[static_cast<int32>(Rand(Seed, 2) * 6.f) % 6]), 0.55f);
	const FNHSurface TopS(Top, 0.85f, 0.4f);
	const FNHSurface Bottom(FLinearColor(Bottoms[static_cast<int32>(Rand(Seed, 3) * 5.f) % 5]), 0.85f, 0.4f);
	const FNHSurface Shoe(FLinearColor(0.03f, 0.025f, 0.02f), 0.6f);

	// legs hang from hip pivots, arms from shoulder pivots, so they swing from the top
	HipL = NHShapes::AddPivot(this, Root, FVector(0, -10.f, 88.f * H));
	HipR = NHShapes::AddPivot(this, Root, FVector(0, 10.f, 88.f * H));
	for (USceneComponent* Hip : { HipL.Get(), HipR.Get() })
	{
		NHShapes::AddPiece(this, Hip, ENHShape::Cylinder, FVector(0, 0, -42.f * H), FVector(14.f, 14.f, 84.f * H), Bottom);
		NHShapes::AddPiece(this, Hip, ENHShape::Box, FVector(5.f, 0, -86.f * H), FVector(26.f, 11.f, 7.f), Shoe);
	}
	NHShapes::AddPiece(this, Root, ENHShape::Box, FVector(0, 0, 92.f * H), FVector(24.f, 36.f, 18.f), Bottom);            // hips
	NHShapes::AddPiece(this, Root, ENHShape::Box, FVector(0, 0, 124.f * H), FVector(24.f, 38.f, 50.f * H), TopS);        // torso
	NHShapes::AddPiece(this, Root, ENHShape::Cylinder, FVector(0, 0, 152.f * H), FVector(10.f, 10.f, 10.f), SkinS);      // neck
	NHShapes::AddPiece(this, Root, ENHShape::Sphere, FVector(1.f, 0, 166.f * H), FVector(20.f, 18.f, 24.f), SkinS);      // head
	if (bHeadTie)
	{
		NHShapes::AddPiece(this, Root, ENHShape::Sphere, FVector(-1.f, 0, 178.f * H), FVector(26.f, 24.f, 16.f), FNHSurface(FLinearColor(Ties[static_cast<int32>(Rand(Seed, 4) * 4.f) % 4]), 0.8f, 0.3f));
	}
	ShoulderL = NHShapes::AddPivot(this, Root, FVector(0, -24.f, 144.f * H));
	ShoulderR = NHShapes::AddPivot(this, Root, FVector(0, 24.f, 144.f * H));
	for (USceneComponent* Sh : { ShoulderL.Get(), ShoulderR.Get() })
	{
		NHShapes::AddPiece(this, Sh, ENHShape::Cylinder, FVector(0, 0, -16.f * H), FVector(11.f, 11.f, 32.f * H), TopS);
		NHShapes::AddPiece(this, Sh, ENHShape::Cylinder, FVector(0, 0, -44.f * H), FVector(9.f, 9.f, 26.f * H), SkinS);
	}
	SnapToGround();
}

bool ANHPerson::BuildBody(int32 Seed, const FLinearColor& Top, float Scale)
{
	using namespace NHPeople;
	// who: only those the part allows, and only those the project has. The men come first in the list.
	const TArray<FString>& All = UNHOutfitComponent::People();
	const bool bMenOnly = Part == ENHCast::Man || Part == ENHCast::ElderMan;
	TArray<FString> Have;
	for (int32 I = Part == ENHCast::Woman ? UNHOutfitComponent::Men : 0; I < (bMenOnly ? UNHOutfitComponent::Men : All.Num()); ++I)
	{
		if (UNHOutfitComponent::Exists(All[I]))
		{
			Have.Add(All[I]);
		}
	}
	if (Have.Num() == 0)
	{
		return false;
	}
	FString Who = Have[FMath::Min(static_cast<int32>(Rand(Seed, 5) * Have.Num()), Have.Num() - 1)];
	if (Part == ENHCast::Man)
	{
		// area boys and the Task Force are Lagos men: the Nigerian men the project has, each in turn
		TArray<FString> Local;
		for (const TCHAR* Name : { TEXT("Tunde"), TEXT("Dayo"), TEXT("Emeka") })
		{
			if (Have.Contains(Name))
			{
				Local.Add(Name);
			}
		}
		if (Local.Num() > 0)
		{
			Who = Local[FMath::Abs(Seed) % Local.Num()];
		}
	}
	if (Part == ENHCast::ElderMan)
	{
		// "Baba" once the project has an old man of that name; until then Emeka, the oldest of the men (middle-aged)
		Who = UNHOutfitComponent::Exists(TEXT("Baba")) ? FString(TEXT("Baba")) : Have.Contains(TEXT("Emeka")) ? FString(TEXT("Emeka")) : Who;
	}
	if (Part != ENHCast::Anyone)
	{
		static const TCHAR* Parts[] = { TEXT("anyone"), TEXT("a man"), TEXT("a woman"), TEXT("an elderly man") };
		UE_LOG(LogNHGame, Verbose, TEXT("NAIJA HUSTLE: cast as %s: %s"), Parts[static_cast<int32>(Part)], *Who);
	}
	const auto Clip = [&Who](const TCHAR* Path) -> UAnimSequence*
	{
		const FString Full = FString::Printf(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/%s_%s"), Path, *Who);
		return FPackageName::DoesPackageExist(Full) ? LoadObject<UAnimSequence>(nullptr, *Full) : nullptr;
	};
	Body = NewObject<USkeletalMeshComponent>(this);
	Body->SetupAttachment(Root);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCanEverAffectNavigation(false);
	Body->RegisterComponent();
	Outfit = NewObject<UNHOutfitComponent>(this);
	Outfit->RegisterComponent();
	if (!Outfit->Dress(Body, Who))
	{
		Body->DestroyComponent();
		Body = nullptr;
		return false;
	}
	Outfit->Pick(Seed, &Top);
	// people off screen do not animate, and far ones animate less often
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Body->bEnableUpdateRateOptimizations = true;
	const bool bMan = All.IndexOfByKey(Who) < UNHOutfitComponent::Men;
	const float Tall = Body->GetSkeletalMeshAsset()->GetBounds().BoxExtent.Z * 2.f;
	const float Height = (bMan ? 176.f : 164.f) * (0.95f + 0.1f * Rand(Seed, 1)) * Scale;
	Body->SetRelativeScale3D(FVector(Tall > 1.f ? Height / Tall : 1.f));
	Body->SetRelativeRotation(FRotator(0.f, -ANHCharacter::FacingYawOf(Body->GetSkeletalMeshAsset()), 0.f));
	IdleClip = Clip(TEXT("MM_Idle"));
	WalkClip = Clip(TEXT("Walk/MF_Unarmed_Walk_Fwd"));
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	if (IdleClip)
	{
		Body->PlayAnimation(IdleClip, true);
		Body->SetPosition(Rand(Seed, 6) * IdleClip->GetPlayLength()); // not everyone breathing in step
	}
	return true;
}

bool ANHPerson::Hurt(float Damage, const FVector& From)
{
	if (bDown)
	{
		return false;
	}
	if (bEssential)
	{
		ANHHUD::Floater(this, GetActorLocation() + FVector(0.f, 0.f, 200.f), TEXT("You dey craze?!"));
		return false;
	}
	Health -= Damage;
	if (Health > 0.f)
	{
		Scare(From);
		return false;
	}
	// down: falls where they stand, lies there half a minute, and is gone
	bDown = true;
	bWalking = false;
	bWaving = false;
	FleeLeft = 0.f;
	LifeLeft = 30.f;
	if (Body)
	{
		Body->Stop();
	}
	return true;
}

void ANHPerson::Scare(const FVector& From)
{
	if (bDown || bEssential || bBrave)
	{
		return;
	}
	static const TCHAR* Cries[] = { TEXT("Ye!"), TEXT("Jesu!"), TEXT("Gunshot o!"), TEXT("Run o!"), TEXT("E don happen!"), TEXT("Chineke!"), TEXT("Wayo!") };
	if (FleeLeft <= 0.f && FMath::RandRange(0, 2) == 0)
	{
		ANHHUD::Floater(this, GetActorLocation() + FVector(0.f, 0.f, 200.f), Cries[FMath::RandRange(0, UE_ARRAY_COUNT(Cries) - 1)]);
	}
	// straight away from it, give or take, at a run
	FVector Away = GetActorLocation() - From;
	Away.Z = 0.f;
	Away = Away.GetSafeNormal(1.f, GetActorForwardVector()).RotateAngleAxis(FMath::FRandRange(-35.f, 35.f), FVector::UpVector);
	FleeLeft = FMath::FRandRange(6.f, 9.f);
	Target = GetActorLocation() + Away * 6000.f;
	WalkSpeed = FMath::FRandRange(380.f, 470.f);
	bWalking = true;
	bWaving = false;
}

ANHPerson* ANHPerson::OnRay(const UWorld* World, const FVector& From, const FVector& Direction, float MaxDistance, float& OutDistance)
{
	ANHPerson* Best = nullptr;
	OutDistance = MaxDistance;
	for (TActorIterator<ANHPerson> It(World); It; ++It)
	{
		if (It->bDown)
		{
			continue;
		}
		// the body as an upright line from the feet to the top of the head: how near the ray passes to it
		const FVector Feet = It->GetActorLocation();
		FVector OnRayPoint, OnBody;
		FMath::SegmentDistToSegmentSafe(From, From + Direction * OutDistance, Feet + FVector(0.f, 0.f, 15.f), Feet + FVector(0.f, 0.f, 175.f), OnRayPoint, OnBody);
		if (FVector::DistSquared(OnRayPoint, OnBody) < FMath::Square(35.f))
		{
			OutDistance = FVector::Dist(From, OnRayPoint);
			Best = *It;
		}
	}
	return Best;
}

void ANHPerson::ScareAround(const UWorld* World, const FVector& At, float Radius)
{
	for (TActorIterator<ANHPerson> It(World); It; ++It)
	{
		if (FVector::DistSquared(It->GetActorLocation(), At) < FMath::Square(Radius))
		{
			It->Scare(At);
		}
	}
}

void ANHPerson::WalkTo(const FVector& InTarget, float Speed)
{
	if (Part == ENHCast::ElderMan)
	{
		Speed = FMath::Min(Speed, 95.f); // an old man does not hurry
	}
	if (bDown || FleeLeft > 0.f)
	{
		return; // not going anywhere they are told to just now
	}
	Target = InTarget;
	WalkSpeed = Speed;
	bWalking = true;
}

void ANHPerson::FaceTowards(const FVector& Point)
{
	const FVector D = Point - GetActorLocation();
	if (D.SizeSquared2D() > 1.f)
	{
		SetActorRotation(FRotator(0.f, D.Rotation().Yaw, 0.f));
	}
}

void ANHPerson::SnapToGround()
{
	FHitResult Hit;
	const FVector P = GetActorLocation();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(NHPersonGround), false, this);
	if (GetWorld() && GetWorld()->LineTraceSingleByObjectType(Hit, P + FVector(0, 0, 150.f), P - FVector(0, 0, 400.f), FCollisionObjectQueryParams(ECC_WorldStatic), Q))
	{
		SetActorLocation(FVector(P.X, P.Y, Hit.ImpactPoint.Z));
	}
}

void ANHPerson::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (LifeLeft > 0.f)
	{
		LifeLeft -= DeltaSeconds;
		if (LifeLeft <= 0.f)
		{
			Destroy();
			return;
		}
	}
	if (bDown && FallK < 1.f)
	{
		// over backwards in a third of a second
		FallK = FMath::Min(1.f, FallK + DeltaSeconds * 3.f);
		FRotator Lie = GetActorRotation();
		Lie.Pitch = 88.f * FMath::Sin(FallK * UE_HALF_PI);
		SetActorRotation(Lie);
	}
	if (FleeLeft > 0.f)
	{
		FleeLeft -= DeltaSeconds;
		if (FleeLeft <= 0.f)
		{
			bWalking = false;
		}
	}
	float Swing = 0.f;
	if (bWalking)
	{
		const FVector P = GetActorLocation(), D = FVector(Target.X - P.X, Target.Y - P.Y, 0.f);
		const float Dist = D.Size();
		if (Dist < 8.f)
		{
			bWalking = false;
		}
		else
		{
			const float Step = FMath::Min(Dist, WalkSpeed * DeltaSeconds);
			SetActorLocation(P + D / Dist * Step);
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, D.Rotation().Yaw, 0.f), DeltaSeconds, 10.f));
			SnapToGround();
			Phase += DeltaSeconds * WalkSpeed / 30.f;
			Swing = FMath::Sin(Phase) * 28.f;
		}
	}
	if (Body && bWalking != bWalkShown && IdleClip && WalkClip)
	{
		bWalkShown = bWalking;
		Body->PlayAnimation(bWalking ? WalkClip : IdleClip, true);
		Body->SetPlayRate(bWalking ? FMath::Clamp(WalkSpeed / 150.f, 0.7f, 1.6f) : 1.f); // the walk clip covers about 1.5 m a second
	}
	if (HipL && HipR && ShoulderL && ShoulderR)
	{
		HipL->SetRelativeRotation(FRotator(Swing, 0.f, 0.f));
		HipR->SetRelativeRotation(FRotator(-Swing, 0.f, 0.f));
		ShoulderL->SetRelativeRotation(FRotator(-Swing * 0.8f, 0.f, 0.f));
		ShoulderR->SetRelativeRotation(bWaving ? FRotator(0.f, 0.f, -150.f + FMath::Sin(GetWorld()->GetTimeSeconds() * 6.f) * 15.f) : FRotator(Swing * 0.8f, 0.f, 0.f));
	}
}
