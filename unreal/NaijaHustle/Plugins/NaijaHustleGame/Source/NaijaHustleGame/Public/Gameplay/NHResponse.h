#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NHResponse.generated.h"

class ANHPerson;

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
 * They arrive on foot from 50 m off, one every few seconds, more of them the more stars. The Task Force stop within
 * 14 m and shoot; area boys run in and cut. Either can be shot. They leave when the stars are gone, and the stars
 * only fade while none of them can see the player. If the player's health runs out: arrested and bailed (Task
 * Force) or beaten and robbed (area boys), stars cleared, and back at the start with less money.
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
	int32 StarsNear = 2, StarsFar = 3, StarsBoys = 2;

	enum class EComing : uint8 { Nobody, TaskForce, Boys };
	EComing Coming = EComing::Nobody;
	struct FUnit
	{
		TWeakObjectPtr<ANHPerson> Body;
		float Wait = 1.f;
	};
	TArray<FUnit> Units;
	float Think = 0.f, NextArrival = 0.f;
	bool bSeen = false, bAlarm = false;
	TWeakObjectPtr<ANHPerson> Caller;
	float CallerFor = 0.f;
	int32 Made = 0;

	void Decide(const FVector& Player, int32 Stars);
	void Arrive(const FVector& Player);
	void Act(FUnit& Unit, APawn* Pawn, float DeltaSeconds);
	void StandDown();
	void PlayerDown(APawn* Pawn);
	bool CanSee(const ANHPerson* From, const APawn* Pawn) const;
};
