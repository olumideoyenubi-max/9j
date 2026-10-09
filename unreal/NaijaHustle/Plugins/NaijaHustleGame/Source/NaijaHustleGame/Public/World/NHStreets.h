#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NHStreets.generated.h"

class AHUD;
class UCanvas;

/**
 * Real street names in the game, from the districts' OpenStreetMap data (Data/osm_<district>.json, made by
 * Scripts/build_district_osm.py; the same streets are the rows of DT_Streets_<District>).
 *
 * It answers "what street is this?" (StreetAt), and from that:
 *   - a banner when you come onto another street: "Agege Motor Road · Oshodi";
 *   - street signs at the junctions near the player: a pole with a green plate for each street, made as you come
 *     near and removed behind you, so none are stored in the level;
 *   - names on the minimap and the map, more of them the closer the view (DrawNames, called by the HUD), with the
 *     district's side streets drawn in, which the city-wide road graph does not carry.
 * Outside the districts that have data it falls back to the main roads' names from the road graph.
 *
 * Street names are real. (c) OpenStreetMap contributors, ODbL. The game mode spawns one in the real-scale city.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHStreets : public AActor
{
	GENERATED_BODY()

public:
	ANHStreets();
	static ANHStreets* Get(const UObject* WorldContext);
	virtual void Tick(float DeltaSeconds) override;

	/** The name of the street at a place: the nearest named one within MaxDistance cm ("" if none) */
	FString StreetAt(const FVector2D& At, float MaxDistance = 3000.f) const;
	/** "Street, District", or whichever of the two there is */
	FString PlaceName(const FVector2D& At) const;

	/**
	 * Side streets and street names inside a square of the screen showing Span cm of the world from Corner. MaxNames
	 * limits how many are written; which classes are named depends on how much the view covers.
	 */
	void DrawNames(AHUD* Hud, float X, float Y, float Size, const FVector2D& Corner, float Span, int32 MaxNames, float Scale) const;

	int32 NumStreets() const { return Streets.Num(); }
	// ---- for systems that lay things along streets (the scatter)
	const TArray<FVector2D>& StreetPoints(int32 Index) const { return Streets[Index].Points; }
	float StreetWidth(int32 Index) const { return Streets[Index].Width; }
	/** 0 motorway .. 4 tertiary, 5 unclassified, 6 residential, 7 living street, 8 service, 9 track */
	int32 StreetRank(int32 Index) const { return Streets[Index].Rank; }
	/** The streets with a segment in the 200 m cells touching the square from Min to Max */
	void StreetsIn(const FVector2D& Min, const FVector2D& Max, TArray<int32>& Out) const;
	/** For scripted screenshots: says the current street's name again */
	void DebugRepeatBanner() { Current.Reset(); }
	int32 NumSigns() const { return Signs.Num(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	struct FStreet
	{
		FString Name;
		/** 0 motorway .. 4 tertiary, 5 unclassified, 6 residential, 7 living street, 8 service, 9 track */
		uint8 Rank = 6;
		float Width = 750.f;
		TArray<FVector2D> Points;
	};
	struct FJunction
	{
		FVector2D At = FVector2D::ZeroVector;
		TArray<FString> Names;
	};
	TArray<FStreet> Streets;
	TArray<FJunction> Junctions;
	/** Street segments by 200 m cell: street index and the index of the segment's first point */
	TMap<FIntPoint, TArray<FIntPoint>> Cells;
	TMap<FIntPoint, TArray<int32>> JunctionCells;
	bool LoadDistrict(const FString& Path);
	const FStreet* Nearest(const FVector2D& At, float MaxDistance, bool bNamedOnly, FVector2D* OutDir = nullptr) const;

	// ---- the banner
	FString Current, Pending, Banner;
	float PendingFor = 0.f, BannerLeft = 0.f, Think = 0.f;
	FDelegateHandle DrawHandle;
	void DrawBanner(AHUD* Hud, UCanvas* Canvas);

	// ---- signs near the player
	UPROPERTY() TMap<int32, TObjectPtr<AActor>> Signs;
	void UpdateSigns(const FVector& Player);
	AActor* MakeSign(const FJunction& Junction);
};
