#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NHBridgeRails.generated.h"

/**
 * Parapets along the open edges of the real city's bridges and flyovers, so a vehicle cannot slide off a deck.
 *
 * The decks come from the map as bare slabs. A few seconds into play this walks every bridge road in the road data,
 * finds on each side where the deck stops (the surface drops away by more than a metre), and stands a low concrete
 * wall just inside that edge. A side where the deck carries on (the other carriageway, a slip road joining) gets
 * none, and nor does a deck at ground level. The walls are instances of one cube on one actor and block everything.
 */
UCLASS()
class NAIJAHUSTLEGAME_API UNHBridgeRails : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	/** How many lengths of parapet stand, for tests */
	int32 Num() const { return Made; }

private:
	void Build();
	UPROPERTY() TObjectPtr<AActor> Holder;
	int32 Made = 0;
};
