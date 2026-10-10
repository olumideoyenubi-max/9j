#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NHGameData.generated.h"

/** A named danfo stop: where the bus pulls up (Kerb) and where passengers wait (Wait), world cm */
USTRUCT(BlueprintType)
struct FNHBusStop
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FName Id;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FVector2D Kerb = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FVector2D Wait = FVector2D::ZeroVector;
	/** The agbero "ticket" at this stop (0 = none), naira */
	UPROPERTY(BlueprintReadOnly, Category = "Naija") int32 Agbero = 0;
};

USTRUCT(BlueprintType)
struct FNHRoute
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FName Id;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FString Blurb;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TArray<FName> Stops;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") int32 FareLo = 200;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") int32 FareHi = 400;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") int32 Busy = 5;
	/** One way (the first-day route): no looping back */
	UPROPERTY(BlueprintReadOnly, Category = "Naija") bool bLinear = false;
	/** Stop names as this route calls them, if different */
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TMap<FName, FString> Labels;
};

/** Handling and look of a vehicle type. Lengths in cm, speed cm/s, acceleration cm/s², turn rad/s. */
USTRUCT(BlueprintType)
struct FNHVehicleSpec
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float Length = 450.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float Width = 220.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float MaxSpeed = 2800.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float Accel = 1900.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float Turn = 2.4f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float Hp = 100.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") bool bBike = false;
	/** A boat: it goes only where there is water under it (unreal_vehicles.json "boat") */
	UPROPERTY(BlueprintReadOnly, Category = "Naija") bool bBoat = false;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TArray<FLinearColor> Colors;
	/** Which blockout shape a car gets: sedan, suv or sports (empty: by type name) */
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FName Body;
};

/** A real model for a vehicle type, fitted by Scripts/assign_vehicle_meshes.py (Data/vehicle_meshes.json) */
USTRUCT(BlueprintType)
struct FNHVehicleMesh
{
	GENERATED_BODY()
	/** Static meshes drawn together (a body, or a body and its parts), object paths */
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TArray<FString> Meshes;
	/** Turns the model to face +X, then scales it to the type's length, then moves it (cm, after scaling) so its wheels sit on the ground under the vehicle's centre */
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float Yaw = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float Scale = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FVector Offset = FVector::ZeroVector;
	/** The model's height in cm after scaling */
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float Height = 150.f;
};

/** A vehicle left parked in the city */
USTRUCT(BlueprintType)
struct FNHParkedVehicle
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FName Type;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FVector2D Pos = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float Yaw = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FLinearColor Color = FLinearColor::Black;
};

/** The conductor job's numbers (distances cm, speeds cm/s, accelerations cm/s², times s) */
USTRUCT(BlueprintType)
struct FNHConductorRules
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Naija") int32 Capacity = 14;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float OwnerCut = 0.35f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float DamageCost = 40.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float FirstCut = 0.4f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") int32 FirstTicket = 500;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") int32 FullBusBonus = 500;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") int32 MissedStopFine = 100;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float ArriveRadius = 575.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float SlowSpeed = 188.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float NearRadius = 1125.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float PassRadius = 2125.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float DepartSpeed = 313.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float DepartRadius = 875.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float RoughAccel = 3500.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float RoughLateral = 2500.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float SmoothTipAfter = 20.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float Refill = 10.f;
};

USTRUCT(BlueprintType)
struct FNHParkBay
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Naija") int32 Number = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FVector2D Pos = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float Yaw = 0.f;
};

/** One road of the real-scale city: a line through RoadNodes. Class indexes UNHGameData::RoadHalfWidth (0 motorway .. 4 tertiary, 5 link). */
struct FNHRoadWay
{
	uint8 Class = 0;
	bool bOneWay = false;
	bool bBridge = false;
	/** How far its lanes may reach to the driver's left, cm, where the other carriageway is close on that side; 0: its class's half width */
	float Left = 0.f;
	FString Name;
	TArray<int32> Nodes;
};

/** Somewhere in the real city with business to do: the mechanic, the paint shop, the chop shop */
struct FNHPlace
{
	FName Id;
	FString Name;
	FVector2D Pos = FVector2D::ZeroVector;
};

/** The stretch of a way from its node Index to Index + 1 */
struct FNHRoadSeg
{
	int32 Way = 0;
	int32 Index = 0;
	bool operator==(const FNHRoadSeg& O) const { return Way == O.Way && Index == O.Index; }
};

/**
 * Everything the game reads from the plugin's Data folder, loaded once per game instance:
 * lagos_city.json (the map: tiles, stops, bays) and naija_rules.json (routes, fares, vehicles, dialogue),
 * both exported from the browser demo by web/tools/export-unreal.js.
 *
 * The real-scale Lagos level (L_Lagos_City) has its own map data, lagos_real.json from Scripts/build_lagos_real.py:
 * the same stops and motor park at their real places, and the road graph. UseRealCity switches between the two.
 */
UCLASS()
class NAIJAHUSTLEGAME_API UNHGameData : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	static UNHGameData* Get(const UObject* WorldContext);

	UPROPERTY(BlueprintReadOnly, Category = "Naija") bool bLoaded = false;

	// ---- map
	UPROPERTY(BlueprintReadOnly, Category = "Naija") int32 Cols = 96;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") int32 Rows = 64;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float CellSize = 400.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TArray<FString> Tiles;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TArray<FString> DistrictNames;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TArray<FString> DistrictGrid;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TMap<FName, FNHBusStop> Stops;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TArray<FNHParkBay> ParkBays;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FVector2D Park = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FVector2D Home = FVector2D::ZeroVector;

	// ---- the real-scale city
	/** True in L_Lagos_City: Stops, ParkBays, Park and Home are then real places and the road graph is loaded */
	UPROPERTY(BlueprintReadOnly, Category = "Naija") bool bRealCity = false;
	/** Swaps the map data between the small grid city and the real-scale one (the game mode calls it for each level). False if lagos_real.json is missing. */
	bool UseRealCity(bool bReal);
	TArray<FNHPlace> Places;
	const FNHPlace* FindPlace(FName Id) const { return Places.FindByPredicate([Id](const FNHPlace& P) { return P.Id == Id; }); }
	TArray<FVector2D> RoadNodes;
	TArray<FNHRoadWay> RoadWays;
	/** Half the paved width of one carriageway by road class, cm */
	TArray<float> RoadHalfWidth;
	/** The segments that end at each node */
	TMap<int32, TArray<FNHRoadSeg>> RoadJoins;
	/** Every road segment with any part within Radius of a point */
	void RoadsNear(const FVector2D& At, float Radius, TArray<FNHRoadSeg>& Out) const;
	/** The closest point on any road within about 3 km, and the segment it is on; false if there is none */
	bool NearestRoad(const FVector2D& At, FNHRoadSeg& OutSeg, FVector2D& OutPoint) const;
	/** The way to drive from one place to another along the roads, one-way streets respected: road points from near From to near To. False if no way is found. */
	bool RoadRoute(const FVector2D& From, const FVector2D& To, TArray<FVector2D>& OutLine) const;
	float HalfWidth(const FNHRoadWay& Way) const { return RoadHalfWidth.IsValidIndex(Way.Class) ? RoadHalfWidth[Way.Class] : 500.f; }
	/** The same to the driver's left: less where the other carriageway of a dual road is close on that side */
	float LeftHalf(const FNHRoadWay& Way) const { return Way.Left > 0.f ? FMath::Min(Way.Left, HalfWidth(Way)) : HalfWidth(Way); }
	/** Where a lane's middle is, cm to the right of the way's line: Side -1 the left lane, 1 the right, 0 the line between them */
	float LaneOffset(const FNHRoadWay& Way, float Side) const { return (HalfWidth(Way) - LeftHalf(Way)) * 0.5f + Side * (HalfWidth(Way) + LeftHalf(Way)) * 0.25f; }

	// ---- rules
	UPROPERTY(BlueprintReadOnly, Category = "Naija") int32 StartCash = 5000;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float StartMinutes = 480.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float ClockMinutesPerSecond = 2.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float SecondsPerStar = 18.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FNHConductorRules Conductor;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TArray<FNHRoute> Routes;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FNHRoute FirstRoute;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TMap<FName, FNHVehicleSpec> Vehicles;
	/** Real models by vehicle type; a type without one keeps its blockout body */
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TMap<FName, FNHVehicleMesh> VehicleMeshes;
	/** Extra parked vehicles (the luxury cars) from unreal_vehicles.json */
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TArray<FNHParkedVehicle> Parked;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TArray<FString> PaxNames;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TArray<FString> Calls;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TArray<FString> WantedUnits;
	/** Outfit id -> respect (helps when begging the agbero) */
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TMap<FName, int32> OutfitRespect;
	/** First mission: title and objective texts */
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FString FirstDayTitle;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TArray<FString> FirstDayObjectives;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TArray<FString> FirstDaySubs;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") int32 FirstDayCred = 20;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float FirstDayClock = 120.f;

	/** Baba Driver's lines: intro, retry, done, wrecked, late */
	TArray<FString> BabaLines(const FString& Key) const;

	/** Map cell code at a world position ('#' off the map); see the legend in lagos_city.json */
	TCHAR TileAt(const FVector& World) const;
	FString DistrictAt(const FVector& World) const;
	/** The middle of a district of the real city, by name; false if there is none */
	bool DistrictCentre(const FString& Name, FVector2D& Out) const;
	const FNHVehicleSpec& Spec(FName Type) const;
	const FNHRoute* FindRoute(FName Id) const;

	/** Folder holding the JSON files */
	static FString DataDir();

private:
	TMap<FString, TArray<FString>> Baba;
	FNHVehicleSpec DefaultSpec;
	bool LoadCity(const FString& Path);
	bool LoadRealCity(const FString& Path);
	/** The grid city's and the real city's places, kept so UseRealCity can swap them */
	TMap<FName, FNHBusStop> GridStops, RealStops;
	TArray<FNHParkBay> GridBays, RealBays;
	FVector2D GridPark = FVector2D::ZeroVector, GridHome = FVector2D::ZeroVector, RealPark = FVector2D::ZeroVector, RealHome = FVector2D::ZeroVector;
	TArray<TPair<FString, FVector2D>> RealDistricts;
	/** Road segments by 200 m cell */
	TMap<FIntPoint, TArray<FNHRoadSeg>> RoadCells;
	bool bRealLoaded = false;
	bool LoadRules(const FString& Path);
	void LoadVehicleExtras(const FString& TypesPath, const FString& MeshesPath);
};
