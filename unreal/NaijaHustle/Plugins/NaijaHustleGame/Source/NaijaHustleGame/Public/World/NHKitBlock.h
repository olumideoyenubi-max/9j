#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NHKitBlock.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

/** One building of a block: where its walls stand and what the generator may not choose for itself */
USTRUCT(BlueprintType)
struct FNHKitBuilding
{
	GENERATED_BODY()

	/** The outline of the walls, cm from the block actor, in order round the building (either way round) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit")
	TArray<FVector2D> Footprint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit", meta = (ClampMin = 1, ClampMax = 12))
	int32 Floors = 2;

	/** Which wall faces the street (Footprint[FrontEdge] to the next point): shops, the door and balconies go there. -1: the longest */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit")
	int32 FrontEdge = -1;

	/** Everything else about the building follows from this: colours, windows, shop or house, roof, tank */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit")
	int32 Seed = 0;
};

/**
 * A block of Lagos buildings put together from the modular kit (Scripts/build_kit.py, /Game/NaijaHustle/Kit).
 *
 * The level stores only each building's outline, storeys and seed. The walls, windows, shopfronts, balconies, roofs
 * and tanks are instances of the kit's 17 pieces, worked out again from the seed whenever the block loads, so a
 * building costs a few dozen bytes on disk and every piece is stored once. All buildings of a block share one
 * instanced component a piece.
 *
 * A wall of any length is filled with whole 3 m panels stretched or squeezed a little to fit. Each building has its
 * own paint colour, fade and metalwork colour, passed to the materials as per-instance custom data
 * (0-2 wall colour, 3 fade, 4-6 trim colour).
 *
 * Scripts/place_kit_block.py makes these from OpenStreetMap footprints. Change a Seed in the editor to get another variant.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHKitBlock : public AActor
{
	GENERATED_BODY()

public:
	ANHKitBlock();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit")
	TArray<FNHKitBuilding> Buildings;

	/** Height of the ground the buildings stand on, cm, relative to the actor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit")
	float GroundZ = 0.f;

	/** Throw the buildings away and put them together again from Buildings */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Kit")
	void Rebuild();

	/** How many instances the last Rebuild placed */
	UFUNCTION(BlueprintPure, Category = "Kit")
	int32 InstanceCount() const { return Instances; }

private:
	struct FBatch
	{
		TArray<FTransform> Transforms;
		TArray<float> Data; // seven a transform
	};

	void Build(const FNHKitBuilding& Building, TMap<FName, FBatch>& Out) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Parts;

	int32 Instances = 0;
};
