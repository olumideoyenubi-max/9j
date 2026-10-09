#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NHStreetRow.generated.h"

/**
 * One street segment of a district, from OpenStreetMap: a row of DT_Streets_<District>.
 *
 * The row name is "W" + the OpenStreetMap way id, and the same key names the segment's centre line in
 * Data/osm_<district>.json: that is how a row and the road it describes are tied together. Made by
 * Scripts/build_district_osm.py (the CSV) and Scripts/import_streets_table.py (the table).
 * Street names are real. (c) OpenStreetMap contributors, ODbL.
 */
USTRUCT(BlueprintType)
struct FNHStreetRow : public FTableRowBase
{
	GENERATED_BODY()

	/** The street's real name; empty for the many unnamed service roads and some residential streets */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Street") FString StreetName;
	/** motorway, trunk, primary, secondary, tertiary, unclassified, residential, living_street, service or track */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Street") FName RoadClass;
	/** A slip road joining two roads of that class */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Street") bool bLink = false;
	/** Lanes in all, where OpenStreetMap says; 0 where it does not (most streets) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Street") int32 Lanes = 0;
	/** Paved width, m: from the lanes where known, otherwise the class's usual width */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Street") float WidthM = 7.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Street") bool bOneWay = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Street") bool bBridge = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Street") bool bRoundabout = false;
	/** 0 where OpenStreetMap has none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Street") int32 SpeedLimitKmh = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Street") float LengthM = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Street") FString District;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Street") FString OsmWayId;
};
