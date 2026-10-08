#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UObject/Object.h"
#include "NHDebugPlay.generated.h"

class ANHPlayerController;
class ANHGameDirector;
class ANHVehicle;
class ANHCharacter;
class UNHHustleSubsystem;

/** A bare character driven by UNHAdvancedMovementComponent, for the self-test to sprint and stop (the player does not use that component yet) */
UCLASS(NotPlaceable)
class NAIJAHUSTLEGAME_API ANHMomentumDummy : public ACharacter
{
	GENERATED_BODY()

public:
	ANHMomentumDummy(const FObjectInitializer& ObjectInitializer);
};

/**
 * Scripted play for testing, behind the NH console commands on ANHPlayerController (not made in Shipping builds):
 * NHGoto, NHBoard, NHAgbero and NHFinish do one thing each; NHAutoplay plays "First Day on the Danfo" from start
 * to finish and NHSelfTest checks the rules a script can check (vehicles, missed stops, wrecks, the deadline).
 * It plays through the game's own entry points (the E and F handlers, panel choices, the vehicle's drive
 * inputs) and logs every step with the "NAIJA HUSTLE:" prefix.
 */
UCLASS()
class NAIJAHUSTLEGAME_API UNHDebugPlay : public UObject
{
	GENERATED_BODY()

public:
	void Init(ANHPlayerController* InPC) { PC = InPC; }
	/** Called every frame by the player controller */
	void Tick(float DeltaSeconds);
	bool IsRunning() const { return Steps.IsValidIndex(StepIndex); }

	void Goto(const FString& Where);
	void Board();
	void Agbero(const FString& What);
	void Finish();
	void Autoplay(bool bQuitWhenDone);
	void SelfTest(bool bQuitWhenDone);
	void PaintDemo(const FVector& At);

	/** A run that has to restart the level first (to start the story again) carries on from here */
	static FString PendingRun;
	static bool bPendingQuit;

private:
	struct FStep
	{
		FString Name;
		TFunction<bool(float)> Run; // true when the step is done
		float Timeout = 30.f;
		float T = 0.f;
	};
	TArray<FStep> Steps;
	int32 StepIndex = 0;
	FString RunName;
	bool bQuit = false;
	int32 Passed = 0, Failed = 0;
	float Pace = 0.f;

	UPROPERTY() TObjectPtr<ANHPlayerController> PC;

	// the momentum movement check
	UPROPERTY() TObjectPtr<ANHMomentumDummy> MoveDummy;
	float MoveT = 0.f, MoveStopT = 0.f;
	bool bMoveSawHeavyStop = false, bMoveHadPrediction = false;
	FVector MoveStopPredicted = FVector::ZeroVector, MoveStopFrom = FVector::ZeroVector;
	void AddMomentumChecks();

	// the vehicle paint check
	UPROPERTY() TObjectPtr<AActor> PaintBody;
	float PaintT = 0.f;
	void AddVehiclePaintChecks();
	void AddVehicleDynamicsChecks();

	// ---- the script
	void Begin(const FString& Name, bool bQuitWhenDone);
	bool NeedsFreshStory(const FString& Name, bool bQuitWhenDone);
	void End(const FString& Why);
	void Do(const FString& Name, TFunction<void()> Fn);
	void Until(const FString& Name, TFunction<bool(float)> Fn, float Timeout = 30.f);
	bool Check(const FString& What, bool bOk, const FString& Detail = FString());
	void Note(const FString& Text) const;

	void AddMeetBaba();
	void AddSkipDialogue(const FString& Name);
	void AddEnterBus();
	void AddStop(int32 Index, bool bBoard);
	void AddBoard(const FString& Name);
	void AddDriveTo(const FString& Name, TFunction<FVector2D()> Target, float Cruise, bool bStopThere, TFunction<bool()> Done, float Timeout = 40.f);

	// ---- the game, as the script sees it
	ANHGameDirector* Dir() const;
	UNHHustleSubsystem* Hustle() const;
	ANHVehicle* Bus() const;
	ANHCharacter* Character() const;
	FString StageName() const;
	/** Unit vector a bus travels along at this stop, with the waiting passengers on its right */
	FVector2D StopForward(FName StopId) const;
	void PlaceVehicle(ANHVehicle* V, const FVector2D& At, const FVector2D& Forward) const;
	/** A straight run of clear ground ending near the point, to drive in along: returns where to start */
	FVector2D ApproachStart(ANHVehicle* V, const FVector2D& End, const FVector2D& Preferred, float Distance) const;
	bool Drive(ANHVehicle* V, const FVector2D& Target, float Cruise, bool bStopThere) const;
	bool WalkTowards(const FVector& Target, float Reach) const;
	/** Answers the open change panel with the right change; false if none is open */
	bool AnswerChangeRight();
	int32 WaitingAt(FName StopId) const;
};
