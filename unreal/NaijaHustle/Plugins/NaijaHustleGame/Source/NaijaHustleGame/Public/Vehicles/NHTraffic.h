#pragma once

#include "CoreMinimal.h"
#include "Core/NHGameData.h"
#include "GameFramework/Actor.h"
#include "NHTraffic.generated.h"

class ANHVehicle;

/**
 * Traffic for the real-scale city: keeps a handful of the game's vehicles driving along the road graph around the
 * player, and a few more parked at the kerb of the streets nearby, wherever in the city the player is. Vehicles are
 * made out of sight ahead of and around the player and removed once left far behind, so the number alive stays
 * small. Any of them can be got into and driven; one the player has taken is no longer traffic.
 *
 * Moving vehicles keep to the right, follow one-way roads, slow for whatever is in front of them (each other, the
 * player, the player's vehicle) and pick a way on at each junction, mostly straight on. They are carried along the
 * road rather than driven: no overtaking, no traffic lights, no crashes of their own.
 *
 * The game mode spawns one in a level that uses the real city's data (UNHGameData::bRealCity).
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHTraffic : public AActor
{
	GENERATED_BODY()

public:
	ANHTraffic();
	virtual void Tick(float DeltaSeconds) override;

	/** How many vehicles drive around the player, and how many stand parked nearby */
	UPROPERTY(EditAnywhere, Category = "Traffic") int32 MaxMoving = 9;
	UPROPERTY(EditAnywhere, Category = "Traffic") int32 MaxParked = 6;
	/** Vehicles appear between these distances from the player and are removed beyond the last, cm */
	UPROPERTY(EditAnywhere, Category = "Traffic") float SpawnNear = 9000.f;
	UPROPERTY(EditAnywhere, Category = "Traffic") float SpawnFar = 26000.f;
	UPROPERTY(EditAnywhere, Category = "Traffic") float RemoveBeyond = 36000.f;

	/** 0 none, 1 light, 2 normal, 3 heavy: sets MaxMoving and MaxParked (the pause menu's traffic setting) */
	void SetDensity(int32 Level);
	/** Flags down the nearest moving vehicle within 80 m: it stops for a while so the player can get in. The vehicle's name, or "" if none is near. */
	FString Hail(const FVector& Player);
	static ANHTraffic* Get(const UObject* WorldContext);
	/** Somebody to sit at the wheel of a vehicle of that type (null if the project has no bodies for it) */
	class USkeletalMesh* DriverFor(FName Type);
	/** A vehicle of that type made on the road at a place, facing Yaw, for somebody else to drive along (a hailed ride); traffic leaves it alone */
	ANHVehicle* MakeForHire(FName Type, const FVector2D& At, float Yaw);

	int32 NumMoving() const;
	int32 NumParked() const { return Cars.Num() - NumMoving(); }

private:
	struct FCar
	{
		TWeakObjectPtr<ANHVehicle> Vehicle;
		bool bParked = false;
		/** On the way's segment Seg, going toward its higher node (Dir 1) or its lower (-1), Along cm from where it came on */
		FNHRoadSeg Seg;
		int32 Dir = 1;
		float Along = 0.f;
		/** Which side of the centre line it keeps to, -1 or 1 (always 1, the right, on a two-way road) */
		float Side = 1.f;
		float Speed = 0.f;
		/** Seconds it still waits, flagged down by the player */
		float Wait = 0.f;
		FVector2D At = FVector2D::ZeroVector;
		float Yaw = 0.f;
	};
	TArray<FCar> Cars;
	/** Vehicles the player took: left alone, and cleared away once abandoned far behind */
	TArray<TWeakObjectPtr<ANHVehicle>> Taken;
	float SpawnTimer = 0.f;
	/** For the log: seconds since the first fill, how far the traffic has driven in all (cm), and how many vehicles have been made */
	float ReportTime = 0.f;
	double Driven = 0.0;
	int32 Made = 0;
	int32 Seated = 0;
	bool bFilled = false;
	/** The people at the wheel: whichever of the Lagos Runner's outfits the project has (street, suit, dispatch rider) */
	UPROPERTY(Transient) TArray<TObjectPtr<class USkeletalMesh>> Drivers;
	bool bDriversLoaded = false;

	const UNHGameData* Data = nullptr;
	FVector2D NodeAt(const FNHRoadSeg& Seg, int32 Dir, bool bEnd) const;
	/** Where a car on the segment is and faces: the lane's line, Along from the segment's start in its direction */
	void Rail(const FCar& Car, FVector2D& OutAt, float& OutYaw) const;
	float Cruise(const FNHRoadWay& Way) const;
	/** Moves a car onto the next stretch of road at the end of its segment; false at a dead end */
	bool NextSegment(FCar& Car) const;
	void Step(FCar& Car, const FVector& Player, float DeltaSeconds);
	bool Blocked(const FCar& Car, const FVector& Player, float& OutGap) const;
	bool TrySpawn(const FVector2D& Player, bool bParked, float Near);
	ANHVehicle* Make(FName Type, const FVector2D& At, float Yaw, bool bBridge);
	FName RandomType(bool bParked) const;
	void Tidy(const FVector2D& Player);
};
