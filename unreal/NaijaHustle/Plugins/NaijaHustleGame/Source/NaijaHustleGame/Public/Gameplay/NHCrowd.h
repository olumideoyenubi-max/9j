#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NHCrowd.generated.h"

class ANHPerson;

/**
 * The people on the pavements round the player, in the real city: a handful of walkers kept within 90 m, on the
 * edges of ordinary streets (none on expressways or bridges). How many depends on the zone and the hour
 * (Data/population_zones.json, read by ANHTraffic) and on the pause menu's traffic setting, 25 at the very most.
 *
 * Nobody is made or destroyed as the player moves: a walker left more than 90 m behind is stood somewhere new out
 * of the way ahead and walks on from there. They are ordinary ANHPerson, so they run from gunfire and can be hurt.
 * Most are Lagosians; about one in seven is anyone at all. The game mode spawns one in the real city.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHCrowd : public AActor
{
	GENERATED_BODY()

public:
	ANHCrowd();
	static ANHCrowd* Get(const UObject* WorldContext);
	virtual void Tick(float DeltaSeconds) override;

	/** The most there may be at rush hour on a busy street; the hour and the zone bring it down */
	UPROPERTY(EditAnywhere, Category = "Crowd") int32 MaxPeople = 16;
	int32 NumPeople() const { return Walkers.Num(); }
	int32 NumMade() const { return Made; }
	int32 NumMoved() const { return Moved; }

private:
	struct FWalker
	{
		TWeakObjectPtr<ANHPerson> Body;
		float Idle = 0.f;
	};
	TArray<FWalker> Walkers;
	float Think = 0.f;
	int32 Made = 0, Moved = 0;
	bool bFilled = false;
	/** A spot on the edge of a street between Near and Far from the player, and which way the street runs there; false if none was found */
	bool FindSpot(const FVector& Player, float Near, float Far, FVector& OutAt, FVector2D& OutAlong) const;
	void SendOn(ANHPerson* Body, const FVector2D& Along) const;
};

/**
 * Counts what is alive round the player and how fast the game runs with it (living-Lagos brief, step 2):
 *
 *   Scripts/mac.sh city -NHPopulationTest
 *
 * stands at the start for 40 seconds and logs "[populationtest]" lines: vehicles moving and parked, models in use,
 * vehicles reused from the pool, pedestrians, frame time and memory. Then it quits.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHPopulationTest : public AActor
{
	GENERATED_BODY()

public:
	ANHPopulationTest();
	virtual void Tick(float DeltaSeconds) override;

private:
	float Clock = 0.f, NextLine = 10.f;
	int32 Frames = 0;
	double FrameSeconds = 0.0;
	bool bDone = false;
};
