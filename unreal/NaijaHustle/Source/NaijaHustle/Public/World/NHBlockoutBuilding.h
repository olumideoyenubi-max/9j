#pragma once

#include "CoreMinimal.h"
#include "World/NHBlockoutActor.h"
#include "NHBlockoutBuilding.generated.h"

UENUM(BlueprintType)
enum class ENHBuildingKind : uint8
{
	House,       // 2–6 storey plastered house: windows with burglar bars, balconies on the street side
	Estate,      // tidier island houses
	Tower,       // glass office tower: spandrel bands, mullions, lit floors at night
	Stall,       // market stall: posts, table, goods, zinc or tarp canopy
	Stilt,       // timber house on stilts over the lagoon
	FuelStation, // forecourt canopy on columns, pumps, kiosk
	BusShelter,  // bus stop shelter and bench; faces +X (the kerb)
	Footbridge   // pedestrian overpass across a road: deck along X, ramps at both ends
};

UENUM(BlueprintType)
enum class ENHRoofStyle : uint8
{
	Flat,  // parapet; in Oke-Erupe, rebar and column stubs waiting for the next floor
	Zinc   // low gable of corrugated zinc
};

/** Which sides of the footprint face a street (bit flags). */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ENHFace : uint8
{
	None = 0 UMETA(Hidden),
	East = 1 << 0,  // +X
	West = 1 << 1,  // -X
	South = 1 << 2, // +Y
	North = 1 << 3  // -Y
};
ENUM_CLASS_FLAGS(ENHFace);

/**
 * One building from the city data, as a blockout. The actor sits at the centre of its footprint at
 * pavement height; Size is the footprint in cm and Height the wall height. Storeys are ~3 m and window
 * bays ~2.5 m, matching the browser demo's facades. Ground-floor shop fronts come from the city data
 * (ANHCityTile), so street-facing ground floors get no windows here. With bEnterableShop the ground
 * floor is hollow: walls with a doorway on the ShopFace side, a floor, a ceiling, shelves and a counter.
 */
UCLASS()
class NAIJAHUSTLE_API ANHBlockoutBuilding : public ANHBlockoutActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building") ENHBuildingKind Kind = ENHBuildingKind::House;
	/** Footprint in cm (X by Y) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building") FVector2D Size = FVector2D(800.f, 800.f);
	/** Wall height in cm (to the roof) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building", meta = (ClampMin = "200")) float Height = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building") ENHRoofStyle Roof = ENHRoofStyle::Flat;
	/** Oke-Erupe: raw or faded concrete, more bars, rusty zinc, unfinished roofs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building") bool bDusty = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building") FLinearColor WallColor = FLinearColor(0.6f, 0.5f, 0.4f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building") int32 Seed = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building", meta = (Bitmask, BitmaskEnum = "/Script/NaijaHustle.ENHFace")) int32 StreetFaces = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Shop") bool bEnterableShop = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Shop", meta = (Bitmask, BitmaskEnum = "/Script/NaijaHustle.ENHFace")) int32 ShopFace = 0;
	/** Centre of the doorway along the shop face, cm from the face's middle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Shop") float ShopDoorOffset = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Shop") float ShopDoorWidth = 280.f;

protected:
	virtual void Build() override;

private:
	struct FFace { FVector Normal; FVector Tangent; float Length; float Depth; ENHFace Flag; };
	TArray<FFace> Faces() const;

	void BuildHouse();
	void BuildTower();
	void BuildStall();
	void BuildStilt();
	void BuildFuelStation();
	void BuildBusShelter();
	void BuildFootbridge();

	void Windows(float Z0, float StoreyH, int32 Storeys, bool bSkipStreetGround);
	void Balconies(float StoreyH, int32 Storeys);
	void FlatRoof(float Z);
	void ZincRoof(float Z, const FVector2D& Footprint, float Overhang);
	void ShopInterior(float StoreyH);
};
