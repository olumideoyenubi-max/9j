#pragma once

#include "CoreMinimal.h"
#include "World/NHBlockoutActor.h"
#include "NHCityTile.generated.h"

class USpotLightComponent;

/** A painted line on the road (lane dash, zebra stripe, parking bay edge), world cm */
USTRUCT(BlueprintType)
struct FNHMarking
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marking") FVector2D A = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marking") FVector2D B = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marking") float Width = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marking") FLinearColor Color = FLinearColor::White;
};

/** One street prop (pole, water tank, generator, chair, umbrella...) as a fitted primitive, world space */
USTRUCT(BlueprintType)
struct FNHPropInstance
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") FName Kind;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") ENHShape Shape = ENHShape::Box;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") FTransform Transform;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") FNHSurface Surface;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") bool bSolid = false;
};

/** A painted sign board with its text. Location is the bottom centre on the wall; Yaw faces the reader. */
USTRUCT(BlueprintType)
struct FNHSign
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign") FVector Location = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign") float Yaw = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign") float Width = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign") float Height = 70.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign") FString Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign") FString Sub;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign") FLinearColor Background = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sign") FLinearColor Foreground = FLinearColor::Black;
};

UENUM(BlueprintType)
enum class ENHShopfrontState : uint8
{
	Shutter,  // roller shutter down
	Half,     // shutter half up, shop lit inside
	Open,     // open shop with a counter out front
	Painted   // shutter painted with the shop's colours
};

/** A ground-floor shop front. Location is the bottom centre on the wall; Yaw faces the street. */
USTRUCT(BlueprintType)
struct FNHShopfront
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shopfront") FVector Location = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shopfront") float Yaw = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shopfront") float Width = 280.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shopfront") float Height = 230.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shopfront") ENHShopfrontState State = ENHShopfrontState::Shutter;
	/** The doorway of an enterable shop (the building leaves the opening): only the frame is drawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shopfront") bool bEnterable = false;
};

/**
 * A 64 m x 64 m piece of the city from the shared map data (16 x 16 cells of 4 m): ground, roads, pavements
 * and kerbs, the lagoon and the bridge, road markings, props, shop fronts, signs and street lamps.
 * One actor per tile so World Partition can stream them. Everything is in world space.
 */
UCLASS()
class NAIJAHUSTLE_API ANHCityTile : public ANHBlockoutActor
{
	GENERATED_BODY()

public:
	/** First cell of this tile in the city grid */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile") int32 Col0 = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile") int32 Row0 = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile") int32 Cols = 16;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile") int32 Rows = 16;
	/** Cell codes for (Cols + 2) x (Rows + 2) cells: this tile plus a one-cell border, row by row ('#' = off the map). Legend in the city JSON. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile") FString Cells;
	/** Same layout: '1' where the cell is in the dusty quarter (laterite instead of asphalt) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile") FString DustyMask;
	/** Lagoon columns: road cells here are the bridge deck */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile") int32 LagoonCol0 = 48;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile") int32 LagoonCol1 = 63;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|Content") TArray<FNHMarking> Markings;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|Content") TArray<FNHPropInstance> Props;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|Content") TArray<FNHSign> Signs;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|Content") TArray<FNHShopfront> Shopfronts;
	/** Street lamp heads, world cm; each gets a downward spot light that the lighting rig switches on at night */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|Content") TArray<FVector> LampHeads;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|Lamps") float LampLumens = 9000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile|Lamps") FLinearColor LampColor = FLinearColor(1.f, 0.78f, 0.52f);

	/** 0 = day (lamps off), 1 = night. Called by ANHLightingRig. */
	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetNightLights(float Alpha);

	static constexpr float CellSize = 400.f;
	static constexpr float KerbZ = 16.f;
	static constexpr float WaterZ = -130.f;

protected:
	virtual void Build() override;

private:
	TCHAR CellAt(int32 C, int32 R) const;  // tile-local cell, -1..Cols / -1..Rows (the border)
	bool DustyAt(int32 C, int32 R) const;
	bool IsLagoon(int32 C) const { return Col0 + C >= LagoonCol0 && Col0 + C <= LagoonCol1; }
	float TopZ(TCHAR Cell) const;

	void BuildGround();
	void BuildMarkings();
	void BuildProps();
	void BuildShopfronts();
	void BuildSigns();
	void BuildLamps();
	void ClearGenerated();

	/** Text and lights made by Build(), destroyed and remade on every rebuild */
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Generated;
	UPROPERTY() TArray<TObjectPtr<USpotLightComponent>> LampLights;
	float NightAlpha = 0.f;
};
