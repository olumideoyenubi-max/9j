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
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TArray<FLinearColor> Colors;
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

/**
 * Everything the game reads from the plugin's Data folder, loaded once per game instance:
 * lagos_city.json (the map: tiles, stops, bays) and naija_rules.json (routes, fares, vehicles, dialogue),
 * both exported from the browser demo by web/tools/export-unreal.js.
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

	// ---- rules
	UPROPERTY(BlueprintReadOnly, Category = "Naija") int32 StartCash = 5000;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float StartMinutes = 480.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float ClockMinutesPerSecond = 2.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") float SecondsPerStar = 18.f;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FNHConductorRules Conductor;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TArray<FNHRoute> Routes;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") FNHRoute FirstRoute;
	UPROPERTY(BlueprintReadOnly, Category = "Naija") TMap<FName, FNHVehicleSpec> Vehicles;
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
	const FNHVehicleSpec& Spec(FName Type) const;
	const FNHRoute* FindRoute(FName Id) const;

	/** Folder holding the JSON files */
	static FString DataDir();

private:
	TMap<FString, TArray<FString>> Baba;
	FNHVehicleSpec DefaultSpec;
	bool LoadCity(const FString& Path);
	bool LoadRules(const FString& Path);
};
