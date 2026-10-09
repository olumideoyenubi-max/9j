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
	bool BuildBody(int32 Seed, const FLinearColor& Top, bool bWoman, float Scale);
	void SnapToGround();
};
