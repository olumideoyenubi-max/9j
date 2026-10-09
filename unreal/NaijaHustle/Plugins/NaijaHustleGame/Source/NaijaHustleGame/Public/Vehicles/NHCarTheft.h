#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NHCarTheft.generated.h"

class ANHVehicle;
class ANHPerson;
struct FNHPlace;

/**
 * Taking vehicles that are not yours, and what follows.
 *
 * Parked vehicles (ANHTraffic's): F tries the handle. Most are locked: break the window (the alarm goes off and whoever
 * is near sees it), then hotwire it; some open but have no keys, so hotwire it; a few were left running. Hotwiring is
 * a marker sweeping a bar: press E while it is in the green, three times, before the time runs out.
 * Vehicles with a driver: F pulls the driver out when the vehicle is stopped or crawling. What the driver does next
 * depends on the vehicle: runs off, comes after you on foot and drags you out if he catches you, or rings the Task
 * Force. A danfo's or keke's conductor comes after you too.
 * Witnesses (people and drivers near enough to see) raise the wanted level with the vehicle's value, and somebody posts
 * on Yarns. A stolen vehicle stays hot for a while; a luxury one has a tracker that keeps the wanted level topped up
 * until the mechanic removes it.
 * Three places in the real city (UNHGameData::Places), reached by driving the vehicle there and pressing E: the
 * mechanic takes a tracker out; the paint shop resprays it, which ends the heat and makes it yours; the chop shop
 * buys it by value and damage, unless it is still too hot or tracked.
 *
 * There are no Task Force units or checkpoints in the Unreal game yet, so "wanted" here is the star count only.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHCarTheft : public AActor
{
	GENERATED_BODY()

public:
	ANHCarTheft();
	static ANHCarTheft* Get(const UObject* WorldContext);
	virtual void Tick(float DeltaSeconds) override;

	/** True if getting into this vehicle is a theft to go through here rather than just getting in */
	bool Guards(const ANHVehicle* Vehicle) const;
	/** F at such a vehicle */
	void Approach(ANHVehicle* Vehicle);
	FString Prompt(const ANHVehicle* Vehicle) const;
	/** E: a press at the hotwire, or business at a place; false if there was nothing for it to do */
	bool Action();
	FString ActionPrompt() const;
	/** The car keys: locks or unlocks the player's own vehicle if it is near; false if that is not what they are for right now */
	bool UseKeys(ANHVehicle* Vehicle);

	/** For scripted screenshots: "steal" (breaks into the nearest locked car), "hotwin" (and starts it), "carjack", "sell" (a stolen car at the chop shop), "roll", "climb" (onto the nearest parked vehicle) */
	void Debug(const FString& What);

	// ---- the hotwire, for the HUD
	bool bHotwiring = false;
	float Marker = 0.f, ZoneLo = 0.4f, ZoneHi = 0.6f, TimeLeft = 0.f;
	int32 Hits = 0;

private:
	TWeakObjectPtr<ANHVehicle> Wiring;
	float MarkerDir = 1.f;
	void StartHotwire(ANHVehicle* Vehicle);
	void EndHotwire(bool bStarted);
	void BreakIn(ANHVehicle* Vehicle);
	void Carjack(ANHVehicle* Vehicle);
	/** How many near enough to have seen; some of them hold up a phone */
	int32 Witnesses(const FVector& At, const AActor* Except);
	/** The crime: heat by witnesses and value, and a Yarns post if anybody saw */
	void Report(ANHVehicle* Vehicle, const TCHAR* What, float BaseHeat);
	void Take(ANHVehicle* Vehicle);

	enum class EAfter : uint8 { Runs, Chases, Calls };
	struct FAngry
	{
		TWeakObjectPtr<ANHPerson> Person;
		TWeakObjectPtr<ANHVehicle> Car;
		EAfter Kind = EAfter::Runs;
		float T = 0.f, Think = 0.f;
		FString Name;
	};
	TArray<FAngry> Angry;
	void Throw(ANHVehicle* Car, EAfter Kind, const FString& Name, const FLinearColor& Shirt);

	const FNHPlace* PlaceHere() const;
	void OpenPlace(const FNHPlace& Place, ANHVehicle* Car);
	float TrackerBeat = 0.f;
	bool bToldTracker = false;
};
