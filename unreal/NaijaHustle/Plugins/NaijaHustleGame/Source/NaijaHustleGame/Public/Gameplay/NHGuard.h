#pragma once

#include "CoreMinimal.h"
#include "Gameplay/NHPerson.h"
#include "NHGuard.generated.h"

/** What a guard is doing */
UENUM()
enum class ENHGuardState : uint8
{
	/** Walking its beat, or standing its post */
	Patrol,
	/** Gone to look at something it heard or half saw */
	Investigate,
	/** After the player */
	Chase,
	/** In reach (fists, a blade) or in range (a gun) and at it */
	Attack,
	/** Hurt and not paid enough for this */
	Flee
};

/**
 * Somebody a mission puts in the player's way (ANHMissions): a guard, a gang's boy, a collector.
 *
 *   Sight   a cone in front of it. Being seen fills its awareness, faster the nearer the player is; at full it gives
 *           chase. Crouching halves how far it sees. A disguise it is fooled by (FooledBy) leaves it seeing only what
 *           is right under its nose, unless the player runs or has a weapon out.
 *   Noise   Hear() sends it to look: a sprint nearby, a shot from far off. It looks about and goes back to its beat.
 *   Chase   to within reach, then blows; with a gun, shots from range that mostly miss somebody crouched in cover.
 *           Out of sight for a while, it goes to where it last saw the player, then back to its beat.
 *   Flee    a coward badly hurt runs.
 *   Taken down from behind (TakeDown), unaware, it drops without a sound.
 *
 * It walks in straight lines like every ANHPerson: beats and posts are placed with that in mind.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHGuard : public ANHPerson
{
	GENERATED_BODY()

public:
	ANHGuard();
	virtual void Tick(float DeltaSeconds) override;

	/** Its beat: one point is a post it stands at; more are walked in turn */
	void SetBeat(const TArray<FVector>& Points);
	ENHGuardState GetState() const { return State; }
	/** 0 unaware .. 1 has seen the player */
	float Awareness() const { return Aware; }
	bool IsAlert() const { return State == ENHGuardState::Chase || State == ENHGuardState::Attack; }
	/** A noise at a place, carrying that far */
	void Hear(const FVector& At, float Radius);
	/** From behind and unaware: down, quietly. False if it has seen the player or the player is in front of it. */
	bool TakeDown(const FVector& From);
	bool CanBeTakenDown(const FVector& From) const;

	float SightRange = 1800.f, ConeHalfAngle = 55.f;
	/** Carries a gun: shoots from range instead of closing in */
	bool bArmed = false;
	/** Runs when badly hurt */
	bool bCoward = false;
	float MeleeDamage = 12.f, ShotDamage = 14.f;
	/** Disguises (ANHMissions::Disguise) this one takes at face value */
	TArray<FName> FooledBy;
	/** Times it has gone to look at a noise, and blows and shots that landed, for tests */
	int32 Investigated = 0, Landed = 0;

private:
	ENHGuardState State = ENHGuardState::Patrol;
	TArray<FVector> Beat;
	int32 BeatIndex = 0;
	float Aware = 0.f, Think = 0.f, StateT = 0.f, StrikeWait = 0.f, LostFor = 0.f, PostYaw = 0.f;
	FVector LastSeen = FVector::ZeroVector, LookAt = FVector::ZeroVector;
	bool bSeesNow = false;
	void Go(ENHGuardState To);
	bool Sees(const class ANHCharacter* Player, float& OutDistance) const;
};
