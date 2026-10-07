#include "Gameplay/NHPerson.h"

#include "Components/StaticMeshComponent.h"
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

void ANHPerson::Init(int32 Seed, const FLinearColor& Top, bool bHeadTie, float Scale)
{
	using namespace NHPeople;
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
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

void ANHPerson::WalkTo(const FVector& InTarget, float Speed)
{
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
	if (HipL && HipR && ShoulderL && ShoulderR)
	{
		HipL->SetRelativeRotation(FRotator(Swing, 0.f, 0.f));
		HipR->SetRelativeRotation(FRotator(-Swing, 0.f, 0.f));
		ShoulderL->SetRelativeRotation(FRotator(-Swing * 0.8f, 0.f, 0.f));
		ShoulderR->SetRelativeRotation(bWaving ? FRotator(0.f, 0.f, -150.f + FMath::Sin(GetWorld()->GetTimeSeconds() * 6.f) * 15.f) : FRotator(Swing * 0.8f, 0.f, 0.f));
	}
}
