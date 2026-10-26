#pragma once

#include "CoreMinimal.h"

class AActor;
class USceneComponent;

/** How a blockout person looks */
struct FNHBodyLook
{
	FLinearColor Top = FLinearColor(0.5f, 0.5f, 0.5f);
	FLinearColor Bottom = FLinearColor(0.1f, 0.1f, 0.15f);
	FLinearColor HeadTie = FLinearColor(0.96f, 0.77f, 0.f);
	/** 0 (darkest) to 5 */
	int32 Skin = 2;
	/** Height factor (1 = about 1.8 m) */
	float Height = 1.f;
	bool bHeadTie = false;

	/** A random passer-by from a seed: skin, trousers, height and head-tie colour */
	static FNHBodyLook FromSeed(int32 Seed, const FLinearColor& Top, bool bHeadTie, float Scale);
};

/**
 * The blockout Lagosian shared by passers-by (ANHPerson) and the playable characters (ANHCharacter):
 * legs and arms hang from hip and shoulder pivots so they can swing as the body walks.
 */
struct FNHBody
{
	USceneComponent* HipL = nullptr;
	USceneComponent* HipR = nullptr;
	USceneComponent* ShoulderL = nullptr;
	USceneComponent* ShoulderR = nullptr;

	/** Builds the pieces under Parent, with the soles of the feet at Parent's origin */
	static FNHBody Build(AActor* Owner, USceneComponent* Parent, const FNHBodyLook& Look);
	/** Swings legs and arms (degrees). A waving body holds its right arm up. */
	void Pose(float Swing, bool bWaving = false, float Time = 0.f) const;
};
