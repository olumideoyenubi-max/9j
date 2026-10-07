#pragma once

#include "CoreMinimal.h"
#include "World/NHBlockoutActor.h"

class UStaticMeshComponent;
class USceneComponent;
class UStaticMesh;
class UMaterialInterface;

/**
 * Loose blockout pieces for things that move (vehicles, people): one static mesh component per piece,
 * coloured through custom primitive data with M_NHBlockoutPrim (the same look as the instanced city).
 */
namespace NHShapes
{
	NAIJAHUSTLEGAME_API UStaticMesh* Mesh(ENHShape Shape);
	NAIJAHUSTLEGAME_API UMaterialInterface* PrimMaterial();

	/** Adds a piece at runtime. Size is the full size in cm (for cylinders and cones, Z is the height). */
	NAIJAHUSTLEGAME_API UStaticMeshComponent* AddPiece(AActor* Owner, USceneComponent* Parent, ENHShape Shape, const FVector& Location, const FVector& Size,
		const FNHSurface& Surface, const FRotator& Rotation = FRotator::ZeroRotator);
	NAIJAHUSTLEGAME_API void SetSurface(UStaticMeshComponent* Piece, const FNHSurface& Surface);
	/** A pivot to hang pieces from (hips, shoulders, wheels) */
	NAIJAHUSTLEGAME_API USceneComponent* AddPivot(AActor* Owner, USceneComponent* Parent, const FVector& Location);
}
