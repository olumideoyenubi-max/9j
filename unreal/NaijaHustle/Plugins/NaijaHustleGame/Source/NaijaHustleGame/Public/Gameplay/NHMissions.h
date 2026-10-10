#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NHMissions.generated.h"

class FJsonObject;
class ANHGuard;
class ANHVehicle;
class ANHHackPoint;

/**
 * Runs the story's missions from Data/missions/<id>.json (docs/STORY.md), one at a time. Built once, used by all twelve.
 *
 * A mission is a job card, up to five objectives, a reward card and the night shift's pay:
 *   {
 *     "id": "m02", "number": 2, "title": "Phone Pass", "playAs": "amaka", "brief": "...", "standout": "...",
 *     "start": { "place": "balo_market" } (where its marker is), "rewardCard": true,
 *     "lockSwitch": true, "rewards": { "cred": 25, "goldKobo": 5, "integrity": 0 },
 *     "objectives": [ { "type": "goto", "text": "...", "sub": "...", "at": { "stop": "marketsq", "along": 600, "back": 300 },
 *                       "radius": 500, "checkpoint": true, "say": { "speaker": "Amaka", "lines": ["..."] },
 *                       "onDone": { "flags": { "found_shina_bag": 1 }, "integrity": 5, "cash": 0, "toast": "..." } } ]
 *   }
 *
 * Objective types (what the player does; "at" is where, see Place()):
 *   talk      the "say" lines; done when they have been heard or skipped
 *   firstday  mission 1 only: the conductor's first shift, which ANHGameDirector has always run (meet Baba Driver, the
 *             danfo, the route, bring it back); done when that is
 *   goto      get within "radius" of "at", on foot or driving; "limit": seconds allowed, after which it fails
 *   enter     get into a vehicle of "vehicle" type; "spawn": true stands one at "at"
 *   wait      hold out for "seconds"
 *   choose    a choice card: "title", "lines", "options": [ { "text", "flags", "integrity", "cash", "toast" } ]
 *   plan      the heist board: "steps": [ { "title", "flag", "options": [ { "text", "value" } ] } ], one card a step
 *   switch    the game puts the player in "to"'s shoes (ANHLeads)
 *   loseheat  get the wanted stars down to none; "stars" gives that many first
 *   collect   pick up "count" things stood round "at" (walk into them)
 *   disguise  put on "outfit" (walk into the bundle at "at"); guards "fooledBy" it then take the player at face value
 *   unlock    Amaka's Unlock on the "kind" (camera or light) stood at "at"
 *   defeat    put down the "guards" (see below)
 *   sneak     get to "at" past the "guards"; "failOnAlert": true ends it if one of them gives chase
 *   takedown  put down every guard from behind, unseen (E)
 * "guards": [ { "at": {...}, "beat": [ {...} ], "look": {...} (the place it faces; or "yaw"), "health": 100, "armed": false, "coward": false, "fooledBy": ["waiter"], "sight": 1800 } ]
 *
 * Checkpoints: an objective with "checkpoint": true is where the mission picks up again after a failure (knocked down,
 * seen when it must not be), with the mission's clock still running. Every mission starts with one.
 *
 * The clock: the mission's length and each objective's are logged at the end, with a warning past eight minutes
 * (twelve for missions 8 and 12), for the length rule in docs/BUILD_PROMPT.md.
 *
 * Any objective: "stars" puts that many wanted stars on as it begins; "sayDone": { "speaker", "lines" } is a scene played
 * when it has been done, before the next one.
 * "onDone" (and a choice's option): "flags", "integrity", "cash", "toast", and "switchTo": a lead the game then puts the
 * player in the shoes of, brought to "at" first if that is given; the next objective begins when the switch is done.
 *
 * Night shift: after the last objective Tunde drives the danfo a short way (or the player skips to the takings) and
 * is paid: nothing extra for mission 1, whose own shift pays; from mission 2,
 * round(economy.missionPayBase x (1 + economy.missionPayGrowth)^(number - 2)) from naija_rules.json. "nightShiftCard":
 * { "lines": [...], "flags": {...} } gives the takings card its own words and sets flags (the bag under the seat).
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHMissions : public AActor
{
	GENERATED_BODY()

public:
	ANHMissions();
	static ANHMissions* Get(const UObject* WorldContext);
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * The story mission waiting to be started: the first of m01..m12 not done yet (none while one runs, or when all are
	 * done). Out of a mission its card and marker show where it starts ("start": a place); E there starts it.
	 * Mission 1 starts by itself on a new game.
	 */
	FName Next() const;
	bool NextStart(FVector& OutAt, FString& OutTitle) const;
	/** The missions the Data folder has, in order */
	const TArray<FName>& Known() const { return Ids; }
	/** Begins one: the job card, then its first objective. False if there is no such mission or one is already running. */
	bool Start(FName Id);
	/** Gives the running mission up: everything it stood in the world goes, nothing is paid */
	void Abort(const FString& Why);
	bool IsActive() const { return bActive; }
	FName ActiveId() const { return bActive ? Id : NAME_None; }
	int32 ObjectiveIndex() const { return Index; }
	FString ObjectiveType() const;
	int32 ObjectiveCount() const { return Objectives.Num(); }
	/** Seconds since the job card was closed, checkpoint restarts included */
	float Elapsed() const { return MissionT; }

	/** The objective cannot go on: the card to restart from the last checkpoint or give the job up */
	void Fail(const FString& Why);
	void RestartFromCheckpoint();
	int32 Restarts() const { return RestartCount; }
	/** E: a takedown, when a guard's back is within reach. True if it did something. */
	bool OnAction(APawn* Pawn);
	FString ActionPrompt(const APawn* Pawn) const;
	/** What the player is dressed as, for the guards (none: themselves) */
	FName Disguise;
	/** Finishes the objective in hand, as if the player had done it: for tests and the console */
	void SkipObjective();
	/** The night shift's card: "drive" the short route or "skip" to the takings, for tests */
	void NightShiftChoice(bool bDrive);

	/** What a mission pays on the night shift after it, by its number in the story (1: nothing extra) */
	int32 PayFor(int32 Number) const;
	float PayBase = 1000000.f, PayGrowth = 0.135f;

	/** How the last finished mission went, for the reward card, the log and tests */
	struct FResult
	{
		FName Id;
		bool bDone = false;
		float Seconds = 0.f;
		TArray<float> ObjectiveSeconds;
		int32 Pay = 0, Cred = 0, Integrity = 0, GoldKobo = 0, Restarts = 0;
		bool bTooLong = false;
	};
	FResult Last;

	/** The guards of the objective in hand, for the HUD's marks over their heads and for tests */
	const TArray<TObjectPtr<ANHGuard>>& Guards() const { return GuardList; }
	const TArray<TObjectPtr<AActor>>& Things() const { return Pickups; }
	ANHHackPoint* UnlockTarget() const;
	ANHVehicle* MissionVehicle() const;
	/** Where the objective wants the player, if it has a place */
	bool Where(FVector& Out) const;
	/** Tests of other things: the story neither starts mission 1 by itself nor shows the next job (also -NHNoStory on the command line) */
	bool bStoryOff = false;
	/** Gold Kobo earned by playing, kept here until the store (Phase 7) takes it over */
	int32 GoldKoboEarned = 0;

protected:
	virtual void BeginPlay() override;

private:
	TArray<FName> Ids;
	/** Data/story_places.json: the story's places by id */
	TMap<FString, TSharedPtr<FJsonObject>> StoryPlaces;
	float OfferWait = 3.f;
	bool bOfferedFirst = false;
	void Offer(float DeltaSeconds);
	bool LoadFile(FName MissionId, TSharedPtr<FJsonObject>& Out) const;
	void LoadRules();

	// ---- the mission in hand
	bool bActive = false;
	FName Id;
	TSharedPtr<FJsonObject> Root;
	TArray<TSharedPtr<FJsonObject>> Objectives;
	FString Title;
	int32 Number = 0, Index = -1, RestartCount = 0;
	float MissionT = 0.f, ObjectiveT = 0.f, LengthLimit = 480.f;
	bool bCardOpen = false, bObjectiveReady = false, bFailed = false, bLockedSwitch = false;
	TArray<float> Times;

	struct FCheckpoint
	{
		int32 Index = 0;
		FVector At = FVector::ZeroVector;
		float Yaw = 0.f;
		FName Lead, Disguise;
	};
	FCheckpoint Checkpoint;
	void SaveCheckpoint();

	// ---- one objective
	void Begin(int32 NewIndex);
	void Arm();
	bool Done(float DeltaSeconds);
	void End();
	void Finish();
	void Apply(const TSharedPtr<FJsonObject>& Effects);
	void Clear();
	const TSharedPtr<FJsonObject>& Obj() const { return Objectives[Index]; }
	FString Type;
	bool bHasPlace = false, bChose = false, bSaid = false, bDoneSaid = false;
	FVector PlaceAt = FVector::ZeroVector;
	float Radius = 400.f, Seconds = 0.f;
	int32 PlanStep = 0, Left = 0;
	void OpenPlanStep();
	UPROPERTY() TArray<TObjectPtr<AActor>> Spawned;
	UPROPERTY() TArray<TObjectPtr<AActor>> Pickups;
	UPROPERTY() TArray<TObjectPtr<ANHGuard>> GuardList;
	UPROPERTY() TObjectPtr<ANHHackPoint> HackTarget;
	UPROPERTY() TObjectPtr<ANHVehicle> Car;
	void SpawnGuards();
	AActor* SpawnPickup(const FVector& At, const FLinearColor& Colour);
	float NoiseWait = 0.f;
	int32 ShotsHeard = 0;

	// ---- the night shift
	enum class ENight : uint8 { None, Card, Drive, Paid };
	ENight Night = ENight::None;
	UPROPERTY() TObjectPtr<ANHVehicle> NightBus;
	FVector NightFrom = FVector::ZeroVector;
	float NightDriven = 0.f, NightRoute = 30000.f;
	void OpenNightShift();
	void PayNightShift(bool bDrove);

	// ---- places
	/** {"place": id} one of the story's places (Data/story_places.json), with "along" and "back" where it is a bus stop; {"stop": id, "along": cm, "back": cm} beside a bus stop; {"xy": [x, y]}; {"player": [ahead, right]} from where the player stood when the objective began; {"lead": id} where that lead is */
	bool Place(const TSharedPtr<FJsonObject>& At, FVector& Out) const;
	FVector Anchor = FVector::ZeroVector;
	float AnchorYaw = 0.f;
	class ANHCharacter* Player() const;
	class ANHGameDirector* Dir() const;
	void Card();
};
