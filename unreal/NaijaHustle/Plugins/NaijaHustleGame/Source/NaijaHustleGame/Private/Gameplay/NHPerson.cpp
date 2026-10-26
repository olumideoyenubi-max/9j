#include "Gameplay/NHPerson.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Gameplay/NHBody.h"
#include "World/NHShapes.h"

ANHPerson::ANHPerson()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void ANHPerson::Init(int32 Seed, const FLinearColor& Top, bool bHeadTie, float Scale)
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	const FNHBody Body = FNHBody::Build(this, Root, FNHBodyLook::FromSeed(Seed, Top, bHeadTie, Scale));
	HipL = Body.HipL;
	HipR = Body.HipR;
	ShoulderL = Body.ShoulderL;
	ShoulderR = Body.ShoulderR;
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
	FNHBody Body;
	Body.HipL = HipL;
	Body.HipR = HipR;
	Body.ShoulderL = ShoulderL;
	Body.ShoulderR = ShoulderR;
	Body.Pose(Swing, bWaving, GetWorld()->GetTimeSeconds());
}
