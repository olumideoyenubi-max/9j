#include "Gameplay/NHBody.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "World/NHShapes.h"

namespace NHBodyParts
{
	// a realistic range of Nigerian skin tones, sRGB
	const FColor Skin[] = { FColor(0x3b, 0x24, 0x19), FColor(0x4a, 0x2e, 0x1f), FColor(0x5c, 0x3a, 0x26), FColor(0x6b, 0x44, 0x2c), FColor(0x7a, 0x50, 0x35), FColor(0x8d, 0x5e, 0x3c) };
	const FColor Bottoms[] = { FColor(0x1f, 0x2a, 0x44), FColor(0x2b, 0x2b, 0x2b), FColor(0x5d, 0x5a, 0x3e), FColor(0x6b, 0x4f, 0x3a), FColor(0x3d, 0x5a, 0x80) };
	const FColor Ties[] = { FColor(0xf5, 0xc4, 0x00), FColor(0xe5, 0x39, 0x35), FColor(0x43, 0xa0, 0x47), FColor(0x1e, 0x88, 0xe5) };

	float Rand(int32 Seed, int32 K) { return FMath::Frac(FMath::Sin(Seed * 12.9898f + K * 78.233f) * 43758.5453f); }
}

FNHBodyLook FNHBodyLook::FromSeed(int32 Seed, const FLinearColor& InTop, bool bInHeadTie, float Scale)
{
	using namespace NHBodyParts;
	FNHBodyLook L;
	L.Top = InTop;
	L.bHeadTie = bInHeadTie;
	L.Height = (0.92f + 0.14f * Rand(Seed, 1)) * Scale;
	L.Skin = static_cast<int32>(Rand(Seed, 2) * 6.f) % 6;
	L.Bottom = FLinearColor(Bottoms[static_cast<int32>(Rand(Seed, 3) * 5.f) % 5]);
	L.HeadTie = FLinearColor(Ties[static_cast<int32>(Rand(Seed, 4) * 4.f) % 4]);
	return L;
}

FNHBody FNHBody::Build(AActor* Owner, USceneComponent* Parent, const FNHBodyLook& Look)
{
	using namespace NHBodyParts;
	FNHBody B;
	const float H = Look.Height;
	const FNHSurface SkinS(FLinearColor(Skin[FMath::Clamp(Look.Skin, 0, 5)]), 0.55f);
	const FNHSurface TopS(Look.Top, 0.85f, 0.4f);
	const FNHSurface Bottom(Look.Bottom, 0.85f, 0.4f);
	const FNHSurface Shoe(FLinearColor(0.03f, 0.025f, 0.02f), 0.6f);

	B.HipL = NHShapes::AddPivot(Owner, Parent, FVector(0, -10.f, 88.f * H));
	B.HipR = NHShapes::AddPivot(Owner, Parent, FVector(0, 10.f, 88.f * H));
	for (USceneComponent* Hip : { B.HipL, B.HipR })
	{
		NHShapes::AddPiece(Owner, Hip, ENHShape::Cylinder, FVector(0, 0, -42.f * H), FVector(14.f, 14.f, 84.f * H), Bottom);
		NHShapes::AddPiece(Owner, Hip, ENHShape::Box, FVector(5.f, 0, -86.f * H), FVector(26.f, 11.f, 7.f), Shoe);
	}
	NHShapes::AddPiece(Owner, Parent, ENHShape::Box, FVector(0, 0, 92.f * H), FVector(24.f, 36.f, 18.f), Bottom);        // hips
	NHShapes::AddPiece(Owner, Parent, ENHShape::Box, FVector(0, 0, 124.f * H), FVector(24.f, 38.f, 50.f * H), TopS);    // torso
	NHShapes::AddPiece(Owner, Parent, ENHShape::Cylinder, FVector(0, 0, 152.f * H), FVector(10.f, 10.f, 10.f), SkinS);  // neck
	NHShapes::AddPiece(Owner, Parent, ENHShape::Sphere, FVector(1.f, 0, 166.f * H), FVector(20.f, 18.f, 24.f), SkinS);  // head
	if (Look.bHeadTie)
	{
		NHShapes::AddPiece(Owner, Parent, ENHShape::Sphere, FVector(-1.f, 0, 178.f * H), FVector(26.f, 24.f, 16.f), FNHSurface(Look.HeadTie, 0.8f, 0.3f));
	}
	B.ShoulderL = NHShapes::AddPivot(Owner, Parent, FVector(0, -24.f, 144.f * H));
	B.ShoulderR = NHShapes::AddPivot(Owner, Parent, FVector(0, 24.f, 144.f * H));
	for (USceneComponent* Sh : { B.ShoulderL, B.ShoulderR })
	{
		NHShapes::AddPiece(Owner, Sh, ENHShape::Cylinder, FVector(0, 0, -16.f * H), FVector(11.f, 11.f, 32.f * H), TopS);
		NHShapes::AddPiece(Owner, Sh, ENHShape::Cylinder, FVector(0, 0, -44.f * H), FVector(9.f, 9.f, 26.f * H), SkinS);
	}
	return B;
}

void FNHBody::Pose(float Swing, bool bWaving, float Time) const
{
	if (!HipL || !HipR || !ShoulderL || !ShoulderR)
	{
		return;
	}
	HipL->SetRelativeRotation(FRotator(Swing, 0.f, 0.f));
	HipR->SetRelativeRotation(FRotator(-Swing, 0.f, 0.f));
	ShoulderL->SetRelativeRotation(FRotator(-Swing * 0.8f, 0.f, 0.f));
	ShoulderR->SetRelativeRotation(bWaving ? FRotator(0.f, 0.f, -150.f + FMath::Sin(Time * 6.f) * 15.f) : FRotator(Swing * 0.8f, 0.f, 0.f));
}
