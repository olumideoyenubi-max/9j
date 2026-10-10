#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NHLaw.generated.h"

class ANHPerson;

UENUM()
enum class ENHCrime : uint8
{
	/** A gun going off, hitting nobody */
	GunFired,
	VehicleDamage,
	/** Hurting somebody with a blade */
	Assault,
	/** Hurting somebody with a bullet */
	Shooting,
	Killing,
	Count UMETA(Hidden)
};

/**
 * Witnesses (living-Lagos brief, 3.1 and 3.2). A crime raises the player's wanted stars only if somebody saw or
 * heard it and then reported it. One is spawned by the game mode; ANHCharacter tells it about crimes with Crime().
 *
 * Everybody on foot within 25 m with a clear view (or within 60 m of a gunshot, by ear) decides what to do, once
 * every eight seconds at most: most ignore a small thing ("Na your business"), some film it for Yarns, some shout,
 * a few report it. A serious crime turns that round: most report. The victim is likelier to; on high-class streets
 * everybody is; at night fewer do. On area boys' streets nobody calls the Task Force: they call the boys, which comes
 * to the same stars, and ANHResponse sends area boys for them.
 *
 * A report is a phone call that takes six seconds. Get within 2.5 m of the caller before it ends, or put them down,
 * and it is never made. A crime nobody saw or heard changes nothing; one that was only heard counts for half
 * ("unknown suspect"). Whoever was sent for the player (ANHResponse) seeing a crime counts at once, in full.
 *
 * Nobody here fights: ordinary people ignore, film, shout, report or run. A crime counts once however many people report it. The only civilians who fight are the area
 * boys ANHResponse sends on their own streets. The numbers and the lines are in Data/law.json.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHLaw : public AActor
{
	GENERATED_BODY()

public:
	ANHLaw();
	static ANHLaw* Get(const UObject* WorldContext);
	virtual void Tick(float DeltaSeconds) override;

	/** The player did this, there, to that person (or to nobody) */
	void Crime(ENHCrime Kind, const FVector& At, ANHPerson* Victim = nullptr);

	// what has happened since the level began, for tests and the log
	int32 Crimes = 0, Unseen = 0, Ignored = 0, Filmed = 0, Shouted = 0, Calls = 0, CallsMade = 0, CallsStopped = 0;
	float StarsReported = 0.f;
	FString Describe() const;

protected:
	virtual void BeginPlay() override;

private:
	struct FCrimeRule { bool bSerious = false, bLoud = false; float Stars = 0.5f; };
	FCrimeRule Rules[static_cast<int32>(ENHCrime::Count)];
	float SeeCm = 2500.f, HearCm = 6000.f, CallSeconds = 6.f, StopCm = 250.f, HighClassReport = 1.6f, NightReport = 0.7f, VictimReport = 1.5f, HeardOnly = 0.5f;
	/** ignore, film, shout, report: for a minor crime and a serious one */
	float Weights[2][4] = { { 70.f, 10.f, 10.f, 10.f }, { 10.f, 15.f, 15.f, 60.f } };
	TMap<FString, TArray<FString>> Lines;
	FString Line(const TCHAR* Kind) const;

	/** Somebody on the phone to the Task Force */
	struct FCall
	{
		TWeakObjectPtr<ANHPerson> Who;
		float Left = 6.f;
		/** The crimes they are reporting (an index into Known), and whether they saw each or only heard it */
		TMap<int32, bool> About;
		bool bSawPlayer = true;
		/** On area boys' streets the call is to them, not to the Task Force */
		bool bBoys = false;
	};
	TArray<FCall> Ringing;
	/** Every crime since the level began: what a report of it is worth, and how much of that has been counted. A crime
	 *  counts once however many people report it; a second report only adds what the first did not know. */
	struct FKnown { float Stars = 0.f, Counted = 0.f; };
	TArray<FKnown> Known;
	/** Counts what is not yet counted of that crime, seen or only heard, and gives back the stars it added */
	float Count(int32 Index, bool bSaw);
	/** Who has already decided what to do about the player lately, and when (world seconds) */
	TMap<TWeakObjectPtr<ANHPerson>, float> Decided;
	struct FFilm { float Left = 5.f; };
	TArray<FFilm> Films;
	bool CanSee(const ANHPerson* Who, const FVector& At) const;
	/** When a gun was last counted: an AK-47's burst is one noise to the street, not ten */
	float LastGunAt = -10.f;
};
