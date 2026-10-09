#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NHPerson.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;
class UAnimSequence;
class UNHOutfitComponent;

/**
 * A Lagosian (passengers, Baba Driver, agberos, car owners). Walks in a straight line to a target and keeps its
 * feet on the ground.
 *
 * Where the project has the wardrobe people (UNHOutfitComponent), the body is one of them, chosen by Seed, in
 * hair and clothes also chosen by Seed, with that person's idle and walk. Otherwise it is the blockout: legs and
 * arms that swing as they walk, a shirt, trousers, a head and sometimes a head-tie.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHPerson : public AActor
{
	GENERATED_BODY()

public:
	ANHPerson();

	/** Builds the body: Seed picks who it is, the clothes and the height; Top is the shirt colour; bHeadTie makes it a woman */
	void Init(int32 Seed, const FLinearColor& Top, bool bHeadTie = false, float Scale = 1.f);

	UFUNCTION(BlueprintCallable, Category = "Naija") void WalkTo(const FVector& Target, float Speed = 140.f);
	UFUNCTION(BlueprintCallable, Category = "Naija") void StopWalking() { bWalking = false; }
	UFUNCTION(BlueprintPure, Category = "Naija") bool IsWalking() const { return bWalking; }
	/** Face a point (e.g. the player while talking) */
	void FaceTowards(const FVector& Point);
	/** Arm raised: waving down a danfo */
	void SetWaving(bool bOn) { bWaving = bOn; }

	virtual void Tick(float DeltaSeconds) override;

	// ---- being shot at, cut, or near it
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float Health = 100.f;
	/** Somebody the story cannot go on without (Baba Driver): cannot be hurt and does not run */
	bool bEssential = false;
	bool IsDown() const { return bDown; }
	bool IsFleeing() const { return FleeLeft > 0.f; }
	/** Takes the damage; runs from where it came from if still standing. True if this is what put them down. */
	bool Hurt(float Damage, const FVector& From);
	/** Runs away from there for a few seconds, sometimes with a shout */
	void Scare(const FVector& From);
	/** The nearest standing person a ray passes through (within 35 cm of the body), nearer than MaxDistance; where along the ray in OutDistance */
	static ANHPerson* OnRay(const UWorld* World, const FVector& From, const FVector& Direction, float MaxDistance, float& OutDistance);
	/** Everybody standing within Radius of a point runs from it */
	static void ScareAround(const UWorld* World, const FVector& At, float Radius);
	/** Seconds until it removes itself (0 = never) */
	float LifeLeft = 0.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Naija") TObjectPtr<USceneComponent> Root;

private:
	UPROPERTY() TObjectPtr<USceneComponent> HipL;
	UPROPERTY() TObjectPtr<USceneComponent> HipR;
	UPROPERTY() TObjectPtr<USceneComponent> ShoulderL;
	UPROPERTY() TObjectPtr<USceneComponent> ShoulderR;
	FVector Target = FVector::ZeroVector;
	float WalkSpeed = 140.f;
	float Phase = 0.f;
	bool bWalking = false;
	bool bWaving = false;
	bool bBuilt = false;
	/** The real body, where there is one, and its two clips */
	UPROPERTY() TObjectPtr<USkeletalMeshComponent> Body;
	UPROPERTY() TObjectPtr<UNHOutfitComponent> Outfit;
	UPROPERTY() TObjectPtr<UAnimSequence> IdleClip;
	UPROPERTY() TObjectPtr<UAnimSequence> WalkClip;
	bool bWalkShown = false;
	bool bDown = false;
	float FleeLeft = 0.f, FallK = 0.f;
	bool BuildBody(int32 Seed, const FLinearColor& Top, bool bWoman, float Scale);
	void SnapToGround();
};
