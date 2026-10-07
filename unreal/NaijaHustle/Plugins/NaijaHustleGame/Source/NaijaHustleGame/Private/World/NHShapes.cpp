#include "World/NHShapes.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace NHShapes
{
	UStaticMesh* Mesh(ENHShape Shape)
	{
		static const TCHAR* Paths[] = {
			TEXT("/Engine/BasicShapes/Cube.Cube"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
			TEXT("/Engine/BasicShapes/Sphere.Sphere"), TEXT("/Engine/BasicShapes/Cone.Cone") };
		static TWeakObjectPtr<UStaticMesh> Cache[4];
		const int32 I = FMath::Clamp(static_cast<int32>(Shape), 0, 3);
		if (!Cache[I].IsValid())
		{
			Cache[I] = LoadObject<UStaticMesh>(nullptr, Paths[I]);
		}
		return Cache[I].Get();
	}

	UMaterialInterface* PrimMaterial()
	{
		static TWeakObjectPtr<UMaterialInterface> Cache;
		if (!Cache.IsValid())
		{
			Cache = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NaijaHustle/Environment/Materials/M_NHBlockoutPrim.M_NHBlockoutPrim"));
		}
		return Cache.Get();
	}

	void SetSurface(UStaticMeshComponent* Piece, const FNHSurface& S)
	{
		if (!Piece)
		{
			return;
		}
		const float Data[8] = { S.Color.R, S.Color.G, S.Color.B, S.Roughness, S.Metallic, S.Glow, S.Wet, S.GlowWarm };
		for (int32 I = 0; I < 8; ++I)
		{
			Piece->SetCustomPrimitiveDataFloat(I, Data[I]);
		}
	}

	UStaticMeshComponent* AddPiece(AActor* Owner, USceneComponent* Parent, ENHShape Shape, const FVector& Location, const FVector& Size, const FNHSurface& Surface, const FRotator& Rotation)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetStaticMesh(Mesh(Shape));
		if (UMaterialInterface* M = PrimMaterial())
		{
			C->SetMaterial(0, M);
		}
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->SetupAttachment(Parent);
		C->SetRelativeLocationAndRotation(Location, Rotation);
		C->SetRelativeScale3D(Size / 100.f);
		C->RegisterComponent();
		SetSurface(C, Surface);
		return C;
	}

	USceneComponent* AddPivot(AActor* Owner, USceneComponent* Parent, const FVector& Location)
	{
		USceneComponent* C = NewObject<USceneComponent>(Owner);
		C->SetupAttachment(Parent);
		C->SetRelativeLocation(Location);
		C->RegisterComponent();
		return C;
	}
}
