#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NHResponse.generated.h"

class ANHPerson;
class ANHVehicle;

/** What kind of streets these are, for who comes when there is trouble (Data/response_areas.json) */
UENUM()
enum class ENHArea : uint8
{
	Ordinary,
	/** The Task Force does not come here; the area boys see to it themselves */
	AreaBoys,
	/** The Task Force comes only if somebody who saw it lives long enough to call them */
	HighClass
};

/**
 * Who comes for the player when they have wanted stars, and what they do. One is spawned by the game mode.
 *
 *   Ordinary streets   the Task Force (the game's police) come at 2 stars within 1.5 km of a station, at 3 stars
 *                      within 4 km, and not at all beyond that
 *   Area boys' streets no Task Force: at 2 stars the area boys come, with machetes
 *   High-class streets the Task Force come from anywhere, at any stars, but only once a witness has called them:
 *                      somebody who ran from it and stayed alive for six seconds
 *
 *   Five stars         the army, anywhere, whoever was coming before: soldiers in green with rifles
 *
 * The Task Force, in black, and the army are driven in: a black mini van or pickup (an army pickup or truck) comes
 * from 80 m off, pulls up short of the player and the men get down from it, more of them the more stars. Area boys
 * come on foot from 50 m off, one every few seconds. The Task Force stop within 14 m and shoot pistols, soldiers
 * from 22 m with rifles; area boys run in and cut. All of them can be shot, and a vehicle they came in can be driven
 * off. They leave when the stars are gone, and the stars only fade while none of them can see the player. If the
 * player's health runs out: arrested and bailed (Task Force, army) or beaten and robbed (area boys), stars cleared,
 * and back at the start with less money.
 *
 * Everybody and everything here is made up: the Task Force, its stations, and who runs which streets.
 * Console: NHResponse.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHResponse : public AActor
{
	GENERATED_BODY()

public:
	ANHResponse();
	static ANHResponse* Get(const UObject* WorldContext);
	virtual void Tick(float DeltaSeconds) override;

	ENHArea AreaAt(const FVector& World) const;
	/** The nearest station and how far it is, cm; false if the city has none */
	bool NearestStation(const FVector& World, FString& OutName, float& OutDistance) const;
	/** Somebody who came for the player can see them: the stars do not fade */
	bool Seen() const { return bSeen; }
	int32 Responders() const { return Units.Num(); }
	/** Where the first vehicle that brought them stands; false if none has come */
	bool RideAt(FVector& Out) const;
	/** Where the player is, in a line: the district, its kind, the nearest station */
	FString AreaReport() const;
	/** That, and who is coming */
	FString Describe() const;

protected:
	virtual void BeginPlay() override;

private:
	TSet<FString> HighClass, AreaBoys;
	TArray<TPair<FString, FVector2D>> Stations;
	float NearCm = 150000.f, FarCm = 400000.f, WitnessSeconds = 6.f;
	int32 StarsNear = 2, StarsFar = 3, StarsBoys = 2, StarsArmy = 5;

	enum class EComing : uint8 { Nobody, TaskForce, Boys, Army };
	EComing Coming = EComing::Nobody;
	struct FUnit
	{
		TWeakObjectPtr<ANHPerson> Body;
		float Wait = 1.f;
		/** Who this one is, whoever is coming now: a Task Force officer stays one after the army takes over */
		EComing Kind = EComing::Boys;
	};
	TArray<FUnit> Units;
	/** A van, pickup or truck bringing men: driven toward the player, then emptied beside where it stops */
	struct FRide
	{
		TWeakObjectPtr<ANHVehicle> Car;
		FVector2D At = FVector2D::ZeroVector;
		float Yaw = 0.f, NextDown = 0.f;
		int32 Aboard = 0;
		bool bStopped = false;
		EComing Kind = EComing::TaskForce;
	};
	TArray<FRide> Rides;
	int32 RidesMade = 0;
	float Think = 0.f, NextArrival = 0.f;
	bool bSeen = false, bAlarm = false;
	TWeakObjectPtr<ANHPerson> Caller;
	float CallerFor = 0.f;
	int32 Made = 0;

	void Decide(const FVector& Player, int32 Stars);
	void Arrive(const FVector& Player);
	/** Sends a vehicle with that many men in it */
	void SendRide(const FVector& Player, int32 Men);
	void DriveRide(FRide& Ride, APawn* Pawn, float DeltaSeconds);
	ANHPerson* PutDown(const FVector& At, EComing Kind);
	/** Somewhere about that far from the player, on a road where there is one that way */
	FVector2D ComeFrom(const FVector& Player, float Near, float Far) const;
	int32 Aboard() const;
	/** The height of whatever is on top at a place, looking down from well above NearZ; false if there is nothing */
	bool TopAt(const FVector2D& At, float NearZ, const AActor* Ignore, float& OutZ) const;
	void Act(FUnit& Unit, APawn* Pawn, float DeltaSeconds);
	void StandDown();
	void PlayerDown(APawn* Pawn);
	bool CanSee(const ANHPerson* From, const APawn* Pawn) const;
};
