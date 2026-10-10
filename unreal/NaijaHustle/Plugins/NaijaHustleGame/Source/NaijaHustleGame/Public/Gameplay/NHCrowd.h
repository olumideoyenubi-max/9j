#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NHCrowd.generated.h"

class ANHPerson;

/**
 * The people on the pavements round the player, in the real city: a handful of walkers kept within 90 m, on the
 * edges of ordinary streets (none on, under or within 30 m of an expressway or a bridge). Each keeps to the side of
 * the road, up and down one stretch of its edge. How many depends on the zone and the hour
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
	/** The ground under a point: the lowest thing a ray from the sky meets. False if something else is above it there (a deck, a roof). */
	bool OpenGround(const FVector2D& At, float& OutZ) const;

private:
	struct FWalker
	{
		TWeakObjectPtr<ANHPerson> Body;
		float Idle = 0.f;
		/** The two ends of the stretch of road edge they walk, already out at the side of the road, and which end they are heading for */
		FVector2D EndA = FVector2D::ZeroVector, EndB = FVector2D::ZeroVector;
		bool bToB = true;
	};
	TArray<FWalker> Walkers;
	float Think = 0.f;
	int32 Made = 0, Moved = 0;
	/** Seconds until somebody may greet the player again, and how many have, for tests */
	float GreetWait = 6.f;
public:
	int32 Greeted = 0;
private:
	bool bFilled = false;
	/**
	 * A spot on the edge of a street between Near and Far from the player, and the two ends of that edge (the street's
	 * own line moved out to its side). Never on, under or beside a bridge or an expressway, and its height is the
	 * ground's, whatever the player is standing on. False if none was found.
	 */
	bool FindSpot(const FVector& Player, float Near, float Far, FVector& OutAt, FVector2D& OutA, FVector2D& OutB) const;
	void SendOn(FWalker& Walker) const;
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
