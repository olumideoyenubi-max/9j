#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NHTrees.generated.h"

/**
 * The real-scale city's trees as trees. The map came with a hundred and ten thousand green cones on sticks, merged
 * into pieces by square. Where the project has tree models (Scripts/import_interiors.py with NH_INTERIOR_DEST=/Game/Foliage:
 * a coconut palm and a shade tree), this plants one of them on every spot a cone stood (Data/lagos_trees.json, written
 * by Scripts/prep_tree_gltf.py), turned and sized by the spot, as instances that are only drawn within a few hundred
 * metres; and it hides the cones as their pieces load. With no models in the project it does nothing and the cones stay.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHTrees : public AActor
{
	GENERATED_BODY()

public:
	ANHTrees();
	virtual void Tick(float DeltaSeconds) override;
	int32 Planted = 0;

protected:
	virtual void BeginPlay() override;

private:
	float Look = 0.f;
	int32 Hidden = 0;
};
