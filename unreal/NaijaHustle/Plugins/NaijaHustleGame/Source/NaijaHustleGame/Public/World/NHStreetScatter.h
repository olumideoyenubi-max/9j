#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NHStreetScatter.generated.h"

class ANHStreets;
class UInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * Street clutter, laid along the streets round the player as they move and taken away behind them: weeds at the edge
 * of the tar, litter, rubbish bags, rubble, drums, plastic chairs, generators, kiosks, traders' umbrellas, and power
 * poles with their wires.
 *
 * Nothing is stored in the level. The streets come from ANHStreets (the districts' OpenStreetMap data), the props
 * from /Game/NaijaHustle/Props (Scripts/build_props.py), and where each prop goes is worked out from the street and
 * how far along it is, so the same kiosk is at the same spot every time you come back.
 *
 * The world is cut into 100 m tiles. A tile is filled when the player comes within Reach of it, one tile a step so
 * the frame rate does not jump, and emptied beyond Reach + Slack. Props keep off junctions and out of buildings
 * (anything that blocks a ray down from above). The game mode spawns one in the real-scale city.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHStreetScatter : public AActor
{
	GENERATED_BODY()

public:
	ANHStreetScatter();
	virtual void Tick(float DeltaSeconds) override;

	/** How far from the player tiles are filled, cm */
	UPROPERTY(EditAnywhere, Category = "Scatter")
	float Reach = 16000.f;

	UPROPERTY(EditAnywhere, Category = "Scatter")
	float Slack = 6000.f;

	/** 1 is the normal amount of clutter; 0 none */
	UPROPERTY(EditAnywhere, Category = "Scatter", meta = (ClampMin = 0, ClampMax = 3))
	float Density = 1.f;

	int32 NumTiles() const { return Tiles.Num(); }
	int32 NumProps() const { return Props; }

protected:
	virtual void BeginPlay() override;

private:
	struct FBatch
	{
		TArray<FTransform> Transforms;
		TArray<float> Colours; // three a transform
	};

	void Fill(const FIntPoint& Tile);
	void Empty(const FIntPoint& Tile);
	/** Stand a prop on the ground at a spot, unless a building is there. Room: how much clear space it needs, cm (0: none) */
	bool Place(TMap<FName, FBatch>& Out, FName Prop, const FVector2D& At, float Yaw, float Scale, const FLinearColor& Colour, float Room = 0.f) const;

	UPROPERTY() TMap<FName, TObjectPtr<UStaticMesh>> Meshes;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Parts;
	/** Which of Parts belong to each filled tile */
	TMap<FIntPoint, TArray<TWeakObjectPtr<UInstancedStaticMeshComponent>>> Tiles;
	TWeakObjectPtr<ANHStreets> Streets;
	float Think = 0.f;
	int32 Props = 0;
	bool bSaid = false;
};
