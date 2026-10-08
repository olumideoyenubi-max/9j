// NHAdvancedMovementComponent.h
//
// Heavy, momentum-driven locomotion for Motion Matching.
//
//  * Trajectory: records a world-space history and predicts a future trajectory (positions + facing)
//    with the SAME acceleration / friction / heavy-stop model the movement uses, then writes it into a
//    FTransformTrajectory that the Motion Matching node's Trajectory pin consumes.
//  * Banking: procedural roll lean from centripetal (lateral) acceleration and pitch lean from
//    longitudinal acceleration, physically derived (atan(a / g)) and spring-smoothed.
//  * Heavy stop: releasing input (or reversing it) from a sprint commits the character to a curve-driven
//    stop with a short momentum carry and a hard plant. The state is network-predicted: it lives in
//    the saved move and is restored on client replay, so corrections don't pop.
//
// Network model
//  * Autonomous proxy + server: sprint request travels in compressed flags (FLAG_Custom_0); heavy-stop
//    state is derived identically on both from velocity + acceleration and restored in PrepMoveFor.
//  * Simulated proxies: they don't simulate movement, so the server replicates a quantized input
//    direction and a gait/heavy-stop bitfield (COND_SimulatedOnly) used only for trajectory prediction
//    and animation. No custom RPCs.
//
// Engine notes
//  * Ported to UE 5.8: the trajectory is an FTransformTrajectory (Engine module), which replaced the
//    deprecated FTransformTrajectory in 5.6, so this component needs no Pose Search dependency.
//    The Motion Matching node itself still needs the Pose Search plugin enabled in the project.
//  * Only FillPoseSearchTrajectory() touches the trajectory type.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/TrajectoryTypes.h"
#include "NHAdvancedMovementComponent.generated.h"

class UCurveFloat;

/** One trajectory point in world space. Negative time = past, positive = predicted future. */
USTRUCT(BlueprintType)
struct FNHAdvancedTrajectorySample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
	FVector Position = FVector::ZeroVector;

	/** Facing yaw in degrees (world). */
	UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
	float FacingYaw = 0.f;

	/** Seconds relative to now. */
	UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
	float Time = 0.f;

	/** Planar speed at this sample (cm/s). Handy for distance matching / debugging. */
	UPROPERTY(BlueprintReadOnly, Category = "Trajectory")
	float Speed = 0.f;
};

UENUM(BlueprintType)
enum class ENHAdvancedGait : uint8
{
	Jog,
	Sprint
};

UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class NAIJAHUSTLEGAME_API UNHAdvancedMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

	friend class FNHSavedMove_Advanced;

public:
	UNHAdvancedMovementComponent(const FObjectInitializer& ObjectInitializer);

	// ---- Gait ---------------------------------------------------------------------------------

	/** Jog speed is MaxWalkSpeed. Sprint speed (cm/s): */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Gait")
	float SprintSpeed = 650.f;

	/** Acceleration multiplier at full sprint: lower = heavier, slower to reach top speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Gait", meta = (ClampMin = 0.1, ClampMax = 1))
	float SprintAccelerationScale = 0.55f;

	/** Turn grip (ground friction used for re-directing velocity) at full sprint, as a fraction of normal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Gait", meta = (ClampMin = 0.05, ClampMax = 1))
	float HighSpeedTurnGripScale = 0.35f;

	/** Yaw rotation rate at full sprint, as a fraction of RotationRate.Yaw (wide, committed turns). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Gait", meta = (ClampMin = 0.05, ClampMax = 1))
	float HighSpeedRotationScale = 0.45f;

	/** Called on the locally controlled client (or server for AI). Predicted via compressed flags. */
	UFUNCTION(BlueprintCallable, Category = "Advanced Movement")
	void SetSprinting(bool bNewSprinting) { bWantsToSprint = bNewSprinting; }

	UFUNCTION(BlueprintPure, Category = "Advanced Movement")
	bool IsSprinting() const;

	UFUNCTION(BlueprintPure, Category = "Advanced Movement")
	ENHAdvancedGait GetGait() const { return IsSprinting() ? ENHAdvancedGait::Sprint : ENHAdvancedGait::Jog; }

	// ---- Heavy stop ---------------------------------------------------------------------------

	/** Planar speed at or above which releasing / reversing input triggers a heavy stop. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Heavy Stop")
	float HeavyStopEntrySpeed = 560.f;

	/** Input direction dot velocity direction below which the input counts as a reversal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Heavy Stop", meta = (ClampMin = -1, ClampMax = 1))
	float HeavyStopReversalDot = -0.35f;

	/** Stop duration when stopping from exactly SprintSpeed; scales linearly with entry speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Heavy Stop")
	float HeavyStopBaseDuration = 0.75f;

	/** Fraction of the stop the character is locked into before forward input can cancel it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Heavy Stop", meta = (ClampMin = 0, ClampMax = 1))
	float HeavyStopCommitFraction = 0.55f;

	/**
	 * Optional speed curve. X = normalised stop time 0..1, Y = speed fraction 1..0 of entry speed.
	 * Default (no curve): pow(cos(u * PI/2), 1.5) — short momentum carry, then a hard plant.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Heavy Stop")
	TObjectPtr<UCurveFloat> HeavyStopSpeedCurve = nullptr;

	UFUNCTION(BlueprintPure, Category = "Advanced Movement")
	bool IsHeavyStopping() const;

	/** 0..1 progress through the current heavy stop (0 when not stopping). */
	UFUNCTION(BlueprintPure, Category = "Advanced Movement")
	float GetHeavyStopAlpha() const;

	/** Predicted world location where the character comes to rest, for distance matching. */
	UFUNCTION(BlueprintPure, Category = "Advanced Movement")
	bool GetPredictedStopLocation(FVector& OutLocation) const;

	// ---- Banking / leaning --------------------------------------------------------------------

	/** Multiplier on the physical bank angle atan(a_lateral / g). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Lean")
	float LeanRollScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Lean")
	float MaxLeanRoll = 22.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Lean")
	float LeanPitchScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Lean")
	float MaxLeanPitch = 14.f;

	/** Speed below which lean fades out (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Lean")
	float LeanMinSpeed = 150.f;

	/** Low-pass time constant on raw acceleration (removes frame noise before it becomes lean). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Lean")
	float AccelerationFilterTime = 0.08f;

	/** Critically damped spring time for the final lean (higher = heavier, laggier body). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Lean")
	float LeanSmoothingTime = 0.18f;

	/** Positive = leaning right (into a right turn). Degrees. */
	UFUNCTION(BlueprintPure, Category = "Advanced Movement")
	float GetLeanRoll() const { return LeanRoll; }

	/** Positive = leaning forward (accelerating), negative = leaning back (braking). Degrees. */
	UFUNCTION(BlueprintPure, Category = "Advanced Movement")
	float GetLeanPitch() const { return LeanPitch; }

	// ---- Trajectory ---------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Trajectory")
	float HistoryDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Trajectory")
	float HistorySampleInterval = 1.f / 30.f;

	/** Number of history samples written to the query (evenly spaced over HistoryDuration). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Trajectory", meta = (ClampMin = 1, ClampMax = 30))
	int32 HistoryOutputSamples = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Trajectory")
	float PredictionDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Trajectory", meta = (ClampMin = 1, ClampMax = 30))
	int32 PredictionOutputSamples = 8;

	/** Internal integration step for the prediction (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Trajectory")
	float PredictionStep = 1.f / 30.f;

	/**
	 * Add the skeletal mesh's relative yaw to the facing (Mannequin-based meshes are rotated -90 in the
	 * capsule). Match whatever your Pose Search Schema / database expects.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Trajectory")
	bool bApplyMeshYawOffset = true;

	/** Clears history if the character moves further than this in one tick (teleport / respawn). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Trajectory")
	float TeleportResetDistance = 500.f;

	/** Trajectory and lean are animation-only data; skip them on dedicated servers unless needed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Advanced Movement|Trajectory")
	bool bComputeAnimDataOnDedicatedServer = false;

	UPROPERTY(EditAnywhere, Category = "Advanced Movement|Debug")
	bool bDrawDebugTrajectory = false;

	/** Ready-to-plug trajectory for the Motion Matching node (world space). */
	UFUNCTION(BlueprintPure, Category = "Advanced Movement")
	const FTransformTrajectory& GetPoseSearchTrajectory() const { return PoseSearchTrajectory; }

	UFUNCTION(BlueprintPure, Category = "Advanced Movement")
	const TArray<FNHAdvancedTrajectorySample>& GetTrajectorySamples() const { return TrajectorySamples; }

	/** Call after teleporting the character so history doesn't drag across the map. */
	UFUNCTION(BlueprintCallable, Category = "Advanced Movement")
	void ResetTrajectoryHistory();

	// ---- UCharacterMovementComponent ----------------------------------------------------------

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual FRotator GetDeltaRotation(float DeltaTime) const override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

private:
	/** State used by the prediction simulator; mirrors the predicted movement state. */
	struct FPredictState
	{
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float FacingYaw = 0.f;
		bool bHeavyStop = false;
		float HeavyStopElapsed = 0.f;
		float HeavyStopStartSpeed = 0.f;
		FVector HeavyStopDirection = FVector::ForwardVector;
	};

	// Movement model (shared by real movement and prediction)
	float GetSpeed01(float PlanarSpeed) const;
	float GetHeavyStopDuration(float StartSpeed) const;
	float EvaluateHeavyStopCurve(float Alpha) const;
	bool ShouldEnterHeavyStop(const FVector& PlanarVelocity, const FVector& InputDirection) const;
	void StartHeavyStop(const FVector& Direction, float Speed);
	void EndHeavyStop();
	void TickHeavyStop(float DeltaTime, bool bHasInput);

	// Animation data
	void UpdateReplicatedAnimState();
	void UpdateAnimData(float DeltaTime);
	void UpdateHistory();
	void UpdateLean(float DeltaTime);
	void PredictTrajectory();
	void StepPrediction(FPredictState& S, const FVector& InputDirection, float MaxSpeedForPrediction, float TargetControlYaw, float Dt) const;
	void FillPoseSearchTrajectory();
	void DrawDebug() const;

	FVector GetInputDirectionForAnim() const;
	float GetFacingYawOffset() const;

	// ---- Predicted movement state (saved / restored by FNHSavedMove_Advanced) ----
	uint8 bWantsToSprint : 1;
	uint8 bHeavyStopActive : 1;
	float HeavyStopElapsed = 0.f;
	float HeavyStopStartSpeed = 0.f;
	FVector HeavyStopDirection = FVector::ForwardVector;

	// ---- Replicated to simulated proxies (animation only) ----
	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal RepInputDirection;

	/** bit0 = sprinting, bit1 = heavy stopping */
	UPROPERTY(Replicated)
	uint8 RepAnimFlags = 0;

	// Sim-proxy local heavy-stop tracking (for alpha + prediction)
	float ProxyHeavyStopElapsed = 0.f;
	float ProxyHeavyStopStartSpeed = 0.f;
	bool bProxyWasHeavyStopping = false;

	// ---- Animation outputs ----
	struct FHistoryPoint
	{
		FVector Position;
		float FacingYaw;
		float WorldTime;
		float Speed;
	};
	TArray<FHistoryPoint> History;
	float LastHistoryTime = -1.f;

	TArray<FNHAdvancedTrajectorySample> TrajectorySamples;
	FTransformTrajectory PoseSearchTrajectory;

	FVector PredictedStopLocation = FVector::ZeroVector;
	bool bHasPredictedStop = false;

	FVector PrevVelocity = FVector::ZeroVector;
	FVector FilteredAcceleration = FVector::ZeroVector;
	float LeanRoll = 0.f;
	float LeanRollRate = 0.f;
	float LeanPitch = 0.f;
	float LeanPitchRate = 0.f;
};

/* =============================================================================================
 *  Client prediction
 * ============================================================================================= */

class NAIJAHUSTLEGAME_API FNHSavedMove_Advanced : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;

private:
	uint8 bSavedWantsToSprint : 1 = 0;
	uint8 bSavedHeavyStopActive : 1 = 0;
	float SavedHeavyStopElapsed = 0.f;
	float SavedHeavyStopStartSpeed = 0.f;
	FVector SavedHeavyStopDirection = FVector::ForwardVector;
};

class NAIJAHUSTLEGAME_API FNHNetworkPredictionData_Client_Advanced : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	explicit FNHNetworkPredictionData_Client_Advanced(const UCharacterMovementComponent& ClientMovement)
		: Super(ClientMovement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override;
};
