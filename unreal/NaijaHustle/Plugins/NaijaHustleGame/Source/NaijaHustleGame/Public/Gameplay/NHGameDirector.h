#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/NHGameData.h"
#include "NHGameDirector.generated.h"

class ANHVehicle;
class ANHPerson;

/**
 * Runs the game in a level: the "First Day on the Danfo" mission, free conductor shifts on any danfo,
 * Baba Driver, the vehicles parked at Oshodi Motor Park, the clock, lighting by time of day and the
 * wanted level's decay. A port of the browser demo's conductor and mission code, with the same rules
 * and numbers (read from naija_rules.json). The game mode spawns one; the HUD reads its state.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHGameDirector : public AActor
{
	GENERATED_BODY()

private:
	friend class UNHDebugPlay; // scripted playtests read the mission state and press the same buttons

	enum class EStage : uint8 { Meet, Talk, Board, Route, Return, Wrapping, Failed, Done };
	EStage Stage = EStage::Meet;
	TWeakObjectPtr<ANHPerson> Baba;
	TWeakObjectPtr<ANHVehicle> MissionBus;

public:
	ANHGameDirector();
	static ANHGameDirector* Get(const UObject* WorldContext);

	virtual void Tick(float DeltaSeconds) override;

	// ---- read by the HUD
	FString ObjTitle, ObjText, ObjSub, ShiftLine, NextStopLine;
	float DeadlineMinutesLeft = -1.f;
	bool bMarker = false;
	FVector Marker = FVector::ZeroVector;
	TArray<FVector> RouteStopPoints;

	struct FPanel
	{
		bool bOpen = false;
		FString Title;
		TArray<FString> Lines;
		TArray<FString> Options;
		TFunction<void(int32)> OnChoose;
	};
	struct FDialogue
	{
		bool bOpen = false;
		FString Speaker;
		TArray<FString> Lines;
		int32 Index = 0;
		float T = 0.f;
		TFunction<void()> OnDone;
	};
	FPanel Panel;
	FDialogue Dialogue;

	// ---- input from the player controller
	void OnAction(APawn* Pawn);
	void OnChoice(int32 Index);
	/** The player picked a lighting preset by hand: stop following the clock */
	void SetManualLighting() { bManualLighting = true; }
	/** The lighting debug menu: harsh morning, golden evening, or back to following the clock */
	void OpenLightingMenu();
	/** What E does right now, for the prompt ("" if nothing) */
	FString ActionPrompt(const APawn* Pawn) const;
	bool IsBusy() const { return Panel.bOpen || Dialogue.bOpen; }
	/** The vehicle the current job wants you in, if any (the first day's danfo) */
	const ANHVehicle* WantedVehicle() const { return Stage >= EStage::Board && Stage <= EStage::Return ? MissionBus.Get() : nullptr; }

	ANHVehicle* SpawnVehicle(FName Type, const FVector2D& Pos, float Yaw, const FLinearColor& Paint, const FString& Board);

protected:
	virtual void BeginPlay() override;

private:
	friend class ANHMissions;
	float Deadline = -1.f;
	bool bSlowClock = false;
	bool bManualLighting = false;
	float LightCheck = 0.f;
	int32 LastPreset = -1;

	struct FPax
	{
		int32 Id = 0;
		FName Stop;
		FName Dest;
		int32 Fare = 200;
		int32 Note = 200;
		FString Name;
		TWeakObjectPtr<ANHPerson> Body;
	};
	struct FChange
	{
		FPax Pax;
		int32 Right = 0;
		TArray<int32> Opts;
	};
	struct FShift
	{
		bool bOn = false;
		FNHRoute Route;
		int32 Idx = 0;
		FName AtStop;
		bool bNear = false;
		TArray<FPax> Onboard;
		TMap<FName, TArray<FPax>> Waiting;
		TArray<FChange> ChangeQ;
		int32 Fares = 0, Tips = 0, Bonus = 0, Keep = 0, Penalties = 0, AgberoPaid = 0, Missed = 0, Carried = 0;
		float Comfort = 100.f, HpStart = 0.f, LastSpeed = 0.f, LastYaw = 0.f, Refill = 10.f, SmoothT = 0.f, Cut = 0.35f;
		FString Owner;
		TMap<FName, int32> Demand;
		TWeakObjectPtr<ANHVehicle> Bus;
		bool bMission = false;
		bool bRouteDone = false;
	};
	FShift Shift;
	int32 PaxId = 0;

	// ---- conductor (port of startShift / arrive / callPassengers / answerChange / depart / missStop / openAgbero / endShift)
	void StartShift(const FNHRoute& Route, ANHVehicle* Bus, bool bMission, float Cut, const FString& Owner);
	FPax MakePax(FName StopId);
	void SpawnWaiting(FName StopId, FPax& Pax, int32 Slot);
	void Arrive(FName StopId);
	void CallPassengers();
	void AnswerChange(int32 Choice);
	void Depart();
	void MissStop(FName StopId, const FString& Why);
	void OpenAgbero(FName StopId, int32 Amount, bool bCanBeg);
	void OpenChangePanel();
	void OpenRoutePicker(ANHVehicle* Bus);
	void OpenShiftMenu();
	void EndShift(const FString& Title, TFunction<void()> OnClose);
	void ClearPassengers();
	void UpdateShift(float DeltaSeconds);
	struct FTotals { int32 Fares, Tips, Bonus, Keep, Penalties, Agbero, Damage, Cut, Net; };
	FTotals Totals() const;
	FString StopName(FName StopId) const;

	// ---- the first mission
	void UpdateFirstDay(float DeltaSeconds);
	void FailFirstDay(const FString& Reason);
	void RetryFirstDay();
	void PassFirstDay();
	void SpawnMissionBus();

	// ---- helpers
	void Say(const FString& Speaker, const TArray<FString>& Lines, TFunction<void()> OnDone);
	void OpenPanel(const FString& Title, const TArray<FString>& Lines, const TArray<FString>& Options, TFunction<void(int32)> OnChoose);
	FVector Ground(const FVector2D& P, float Up = 0.f) const;
	FVector StopKerb(FName StopId) const;
	FVector StopWait(FName StopId) const;
	FVector QueueSpot(FName StopId, int32 Slot) const;
	APawn* PlayerPawn() const;
	ANHVehicle* PlayerVehicle() const;
	void UpdateLighting(float DeltaSeconds);
	const UNHGameData* Data() const;
};
