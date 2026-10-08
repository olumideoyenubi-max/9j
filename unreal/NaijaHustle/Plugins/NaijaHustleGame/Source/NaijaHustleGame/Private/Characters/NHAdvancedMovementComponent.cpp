// NHAdvancedMovementComponent.cpp

#include "Characters/NHAdvancedMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"

namespace NHAdvancedMovement
{
	/** Critically damped spring (no overshoot): heavy, weighty settling for lean values. */
	FORCEINLINE void SpringDamp(float& Value, float& Rate, float Target, float SmoothTime, float DeltaTime)
	{
		SmoothTime = FMath::Max(SmoothTime, 1.e-4f);
		const float Omega = 2.f / SmoothTime;
		const float X = Omega * DeltaTime;
		const float Exp = 1.f / (1.f + X + 0.48f * X * X + 0.235f * X * X * X);
		const float Change = Value - Target;
		const float Temp = (Rate + Omega * Change) * DeltaTime;
		Rate = (Rate - Omega * Temp) * Exp;
		Value = Target + (Change + Temp) * Exp;
	}

	static constexpr float StopSpeedEpsilon = 5.f;
}

/* =============================================================================================
 *  Construction
 * ============================================================================================= */

UNHAdvancedMovementComponent::UNHAdvancedMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsToSprint = false;
	bHeavyStopActive = false;

	SetIsReplicatedByDefault(true);

	// Weighty defaults; tune per character in the Blueprint.
	MaxWalkSpeed = 350.f;                // jog
	MaxAcceleration = 1400.f;
	BrakingDecelerationWalking = 1100.f;
	GroundFriction = 6.f;
	bUseSeparateBrakingFriction = true;
	BrakingFriction = 2.5f;
	RotationRate = FRotator(0.f, 540.f, 0.f);
	bOrientRotationToMovement = true;
}

void UNHAdvancedMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Owner and server already know these; only simulated proxies need them, for animation.
	DOREPLIFETIME_CONDITION(UNHAdvancedMovementComponent, RepInputDirection, COND_SimulatedOnly);
	DOREPLIFETIME_CONDITION(UNHAdvancedMovementComponent, RepAnimFlags, COND_SimulatedOnly);
}

/* =============================================================================================
 *  Queries
 * ============================================================================================= */

bool UNHAdvancedMovementComponent::IsSprinting() const
{
	if (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return (RepAnimFlags & 0x1) != 0;
	}
	return bWantsToSprint;
}

bool UNHAdvancedMovementComponent::IsHeavyStopping() const
{
	if (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return (RepAnimFlags & 0x2) != 0;
	}
	return bHeavyStopActive;
}

float UNHAdvancedMovementComponent::GetHeavyStopAlpha() const
{
	if (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		if (!IsHeavyStopping() || ProxyHeavyStopStartSpeed <= 0.f)
		{
			return 0.f;
		}
		return FMath::Clamp(ProxyHeavyStopElapsed / GetHeavyStopDuration(ProxyHeavyStopStartSpeed), 0.f, 1.f);
	}

	return bHeavyStopActive
		? FMath::Clamp(HeavyStopElapsed / GetHeavyStopDuration(HeavyStopStartSpeed), 0.f, 1.f)
		: 0.f;
}

bool UNHAdvancedMovementComponent::GetPredictedStopLocation(FVector& OutLocation) const
{
	OutLocation = PredictedStopLocation;
	return bHasPredictedStop;
}

float UNHAdvancedMovementComponent::GetSpeed01(float PlanarSpeed) const
{
	// 0 at jog speed or below, 1 at sprint speed: momentum effects only kick in above a jog.
	return FMath::Clamp(FMath::GetRangePct(MaxWalkSpeed, FMath::Max(SprintSpeed, MaxWalkSpeed + 1.f), PlanarSpeed), 0.f, 1.f);
}

float UNHAdvancedMovementComponent::GetHeavyStopDuration(float StartSpeed) const
{
	return FMath::Max(0.15f, HeavyStopBaseDuration * StartSpeed / FMath::Max(SprintSpeed, 1.f));
}

float UNHAdvancedMovementComponent::EvaluateHeavyStopCurve(float Alpha) const
{
	Alpha = FMath::Clamp(Alpha, 0.f, 1.f);
	if (HeavyStopSpeedCurve)
	{
		return FMath::Clamp(HeavyStopSpeedCurve->GetFloatValue(Alpha), 0.f, 1.f);
	}
	// Short momentum carry, then a hard plant.
	return FMath::Pow(FMath::Cos(Alpha * HALF_PI), 1.5f);
}

/* =============================================================================================
 *  Movement model overrides
 * ============================================================================================= */

float UNHAdvancedMovementComponent::GetMaxSpeed() const
{
	const float Base = Super::GetMaxSpeed();
	const bool bGrounded = MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking;
	if (bGrounded && IsSprinting() && !IsCrouching())
	{
		return FMath::Max(Base, SprintSpeed);
	}
	return Base;
}

float UNHAdvancedMovementComponent::GetMaxAcceleration() const
{
	const float Base = Super::GetMaxAcceleration();
	if (!IsMovingOnGround())
	{
		return Base;
	}
	// Heavier body: acceleration tapers as speed approaches a full sprint.
	return Base * FMath::Lerp(1.f, SprintAccelerationScale, GetSpeed01(Velocity.Size2D()));
}

FRotator UNHAdvancedMovementComponent::GetDeltaRotation(float DeltaTime) const
{
	FRotator Delta = Super::GetDeltaRotation(DeltaTime);
	if (IsMovingOnGround())
	{
		// Wide, committed turns at speed instead of pivoting on the spot.
		Delta.Yaw *= FMath::Lerp(1.f, HighSpeedRotationScale, GetSpeed01(Velocity.Size2D()));
	}
	return Delta;
}

bool UNHAdvancedMovementComponent::ShouldEnterHeavyStop(const FVector& PlanarVelocity, const FVector& InputDirection) const
{
	const float Speed = PlanarVelocity.Size();
	if (Speed < HeavyStopEntrySpeed)
	{
		return false;
	}
	if (InputDirection.IsNearlyZero())
	{
		return true; // released input at speed
	}
	return (InputDirection | (PlanarVelocity / Speed)) < HeavyStopReversalDot; // hard reversal
}

void UNHAdvancedMovementComponent::StartHeavyStop(const FVector& Direction, float Speed)
{
	bHeavyStopActive = true;
	HeavyStopElapsed = 0.f;
	HeavyStopStartSpeed = Speed;
	HeavyStopDirection = Direction.GetSafeNormal2D();
}

void UNHAdvancedMovementComponent::EndHeavyStop()
{
	bHeavyStopActive = false;
	HeavyStopElapsed = 0.f;
	HeavyStopStartSpeed = 0.f;
}

void UNHAdvancedMovementComponent::TickHeavyStop(float DeltaTime, bool bHasInput)
{
	HeavyStopElapsed += DeltaTime;

	const float Alpha = FMath::Clamp(HeavyStopElapsed / GetHeavyStopDuration(HeavyStopStartSpeed), 0.f, 1.f);
	const float TargetSpeed = HeavyStopStartSpeed * EvaluateHeavyStopCurve(Alpha);

	// Collisions can only slow us further, never speed us back up.
	const float NewSpeed = FMath::Min(Velocity.Size2D(), TargetSpeed);
	Velocity.X = HeavyStopDirection.X * NewSpeed;
	Velocity.Y = HeavyStopDirection.Y * NewSpeed;

	// Past the commit window, pushing forward again cancels the stop (sprint resume).
	if (bHasInput && Alpha >= HeavyStopCommitFraction && (Acceleration.GetSafeNormal2D() | HeavyStopDirection) > 0.5f)
	{
		EndHeavyStop();
		return;
	}

	if (Alpha >= 1.f || NewSpeed < NHAdvancedMovement::StopSpeedEpsilon)
	{
		Velocity.X = 0.f;
		Velocity.Y = 0.f;
		EndHeavyStop();
	}
}

void UNHAdvancedMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	// Leave root motion, air, swimming, flying and tiny steps entirely to the base implementation.
	if (!HasValidData() || HasAnimRootMotion() || HasRootMotionSources() || !IsMovingOnGround() || DeltaTime < MIN_TICK_TIME)
	{
		if (!IsMovingOnGround())
		{
			EndHeavyStop();
		}
		Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
		return;
	}

	const FVector PlanarVelocity(Velocity.X, Velocity.Y, 0.f);
	const FVector InputDirection = Acceleration.GetSafeNormal2D();
	const bool bHasInput = !InputDirection.IsNearlyZero();

	if (!bHeavyStopActive && ShouldEnterHeavyStop(PlanarVelocity, InputDirection))
	{
		StartHeavyStop(PlanarVelocity, PlanarVelocity.Size());
	}

	if (bHeavyStopActive)
	{
		// Input is ignored while committed: the body has to shed its momentum first.
		TickHeavyStop(DeltaTime, bHasInput);
		return;
	}

	// Less grip for re-directing velocity at speed = momentum carries through turns.
	const float Grip = Friction * FMath::Lerp(1.f, HighSpeedTurnGripScale, GetSpeed01(PlanarVelocity.Size()));
	Super::CalcVelocity(DeltaTime, Grip, bFluid, BrakingDeceleration);
}

void UNHAdvancedMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	if (!IsMovingOnGround())
	{
		EndHeavyStop();
	}
}

void UNHAdvancedMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

FNetworkPredictionData_Client* UNHAdvancedMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner != nullptr);
	if (ClientPredictionData == nullptr)
	{
		UNHAdvancedMovementComponent* MutableThis = const_cast<UNHAdvancedMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNHNetworkPredictionData_Client_Advanced(*this);
	}
	return ClientPredictionData;
}

/* =============================================================================================
 *  Tick: replication of anim state + animation data
 * ============================================================================================= */

void UNHAdvancedMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Movement (autonomous / authority) or smoothing (simulated) happens in Super.
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CharacterOwner || !UpdatedComponent)
	{
		return;
	}

	if (CharacterOwner->HasAuthority())
	{
		UpdateReplicatedAnimState();
	}

	if (IsNetMode(NM_DedicatedServer) && !bComputeAnimDataOnDedicatedServer)
	{
		return;
	}

	UpdateAnimData(DeltaTime);
}

void UNHAdvancedMovementComponent::UpdateReplicatedAnimState()
{
	// Plain property writes: replication only sends them when the value actually changes.
	const FVector InputDir = GetCurrentAcceleration().GetSafeNormal2D();
	if (!RepInputDirection.Equals(InputDir, 0.02f))
	{
		RepInputDirection = InputDir;
	}

	const uint8 NewFlags = (bWantsToSprint ? 0x1 : 0x0) | (bHeavyStopActive ? 0x2 : 0x0);
	if (NewFlags != RepAnimFlags)
	{
		RepAnimFlags = NewFlags;
	}
}

FVector UNHAdvancedMovementComponent::GetInputDirectionForAnim() const
{
	if (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return FVector(RepInputDirection.X, RepInputDirection.Y, 0.f);
	}
	return GetCurrentAcceleration().GetSafeNormal2D();
}

float UNHAdvancedMovementComponent::GetFacingYawOffset() const
{
	if (bApplyMeshYawOffset && CharacterOwner && CharacterOwner->GetMesh())
	{
		return CharacterOwner->GetMesh()->GetRelativeRotation().Yaw;
	}
	return 0.f;
}

void UNHAdvancedMovementComponent::UpdateAnimData(float DeltaTime)
{
	if (DeltaTime <= 0.f)
	{
		return;
	}

	// Simulated proxies don't run the stop simulation; track its progress locally from the flag.
	if (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		const bool bStopping = IsHeavyStopping();
		if (bStopping && !bProxyWasHeavyStopping)
		{
			ProxyHeavyStopElapsed = 0.f;
			ProxyHeavyStopStartSpeed = FMath::Max(Velocity.Size2D(), HeavyStopEntrySpeed);
		}
		if (bStopping)
		{
			ProxyHeavyStopElapsed += DeltaTime;
		}
		bProxyWasHeavyStopping = bStopping;
	}

	UpdateHistory();
	UpdateLean(DeltaTime);
	PredictTrajectory();
	FillPoseSearchTrajectory();

	if (bDrawDebugTrajectory)
	{
		DrawDebug();
	}
}

void UNHAdvancedMovementComponent::ResetTrajectoryHistory()
{
	History.Reset();
	LastHistoryTime = -1.f;
	PrevVelocity = FVector::ZeroVector;
	FilteredAcceleration = FVector::ZeroVector;
}

void UNHAdvancedMovementComponent::UpdateHistory()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	// Mesh root (feet), so smoothed simulated proxies record what is actually rendered.
	const FVector Position = CharacterOwner->GetMesh() ? CharacterOwner->GetMesh()->GetComponentLocation() : CharacterOwner->GetActorLocation();

	if (History.Num() > 0 && FVector::DistSquared(History.Last().Position, Position) > FMath::Square(TeleportResetDistance))
	{
		ResetTrajectoryHistory();
	}

	if (History.Num() == 0 || Now - LastHistoryTime >= HistorySampleInterval)
	{
		History.Add({ Position, CharacterOwner->GetActorRotation().Yaw, Now, Velocity.Size2D() });
		LastHistoryTime = Now;
	}

	const float MaxAge = HistoryDuration + HistorySampleInterval;
	int32 NumToRemove = 0;
	while (NumToRemove < History.Num() - 1 && Now - History[NumToRemove].WorldTime > MaxAge)
	{
		++NumToRemove;
	}
	if (NumToRemove > 0)
	{
		History.RemoveAt(0, NumToRemove, EAllowShrinking::No);
	}
}

void UNHAdvancedMovementComponent::UpdateLean(float DeltaTime)
{
	const FVector PlanarVelocity(Velocity.X, Velocity.Y, 0.f);

	// Acceleration from the velocity actually achieved (includes collisions and the heavy stop),
	// low-passed so network updates and frame jitter don't twitch the body.
	const FVector RawAcceleration = (PlanarVelocity - PrevVelocity) / DeltaTime;
	PrevVelocity = PlanarVelocity;
	const float FilterAlpha = 1.f - FMath::Exp(-DeltaTime / FMath::Max(AccelerationFilterTime, 1.e-3f));
	FilteredAcceleration = FMath::Lerp(FilteredAcceleration, RawAcceleration, FilterAlpha);

	float TargetRoll = 0.f;
	float TargetPitch = 0.f;

	const float Speed = PlanarVelocity.Size();
	if (Speed > 1.f)
	{
		const FVector Forward = PlanarVelocity / Speed;
		const FVector Right(-Forward.Y, Forward.X, 0.f);

		// Lateral component = centripetal acceleration (v * yaw rate) in a turn.
		const float LateralAccel = FilteredAcceleration | Right;
		const float LongitudinalAccel = FilteredAcceleration | Forward;
		const float Gravity = FMath::Max(FMath::Abs(GetGravityZ()), 1.f);

		// Physical bank angle of a body balancing in a turn: tan(theta) = a / g.
		const float RollFade = FMath::Clamp((Speed - LeanMinSpeed) / FMath::Max(LeanMinSpeed, 1.f), 0.f, 1.f);
		TargetRoll = FMath::RadiansToDegrees(FMath::Atan2(LateralAccel, Gravity)) * LeanRollScale * RollFade;
		TargetRoll = FMath::Clamp(TargetRoll, -MaxLeanRoll, MaxLeanRoll);

		// Pitch keeps working at low speed so the end of a heavy stop still rocks back.
		const float PitchFade = FMath::Clamp(Speed / FMath::Max(LeanMinSpeed, 1.f), 0.f, 1.f);
		TargetPitch = FMath::RadiansToDegrees(FMath::Atan2(LongitudinalAccel, Gravity)) * LeanPitchScale * PitchFade;
		TargetPitch = FMath::Clamp(TargetPitch, -MaxLeanPitch, MaxLeanPitch);
	}

	NHAdvancedMovement::SpringDamp(LeanRoll, LeanRollRate, TargetRoll, LeanSmoothingTime, DeltaTime);
	NHAdvancedMovement::SpringDamp(LeanPitch, LeanPitchRate, TargetPitch, LeanSmoothingTime, DeltaTime);
}

/* =============================================================================================
 *  Trajectory
 * ============================================================================================= */

void UNHAdvancedMovementComponent::StepPrediction(FPredictState& S, const FVector& InputDirection, float MaxSpeedForPrediction,
	float TargetControlYaw, float Dt) const
{
	FVector Vel(S.Velocity.X, S.Velocity.Y, 0.f);
	const float Speed = Vel.Size();
	const bool bHasInput = !InputDirection.IsNearlyZero();
	const float Speed01 = GetSpeed01(Speed);

	if (!S.bHeavyStop && ShouldEnterHeavyStop(Vel, InputDirection))
	{
		S.bHeavyStop = true;
		S.HeavyStopElapsed = 0.f;
		S.HeavyStopStartSpeed = Speed;
		S.HeavyStopDirection = Vel / Speed;
	}

	if (S.bHeavyStop)
	{
		S.HeavyStopElapsed += Dt;
		const float Alpha = FMath::Clamp(S.HeavyStopElapsed / GetHeavyStopDuration(S.HeavyStopStartSpeed), 0.f, 1.f);
		const float NewSpeed = FMath::Min(Speed, S.HeavyStopStartSpeed * EvaluateHeavyStopCurve(Alpha));
		Vel = S.HeavyStopDirection * NewSpeed;

		if (bHasInput && Alpha >= HeavyStopCommitFraction && (InputDirection | S.HeavyStopDirection) > 0.5f)
		{
			S.bHeavyStop = false;
		}
		else if (Alpha >= 1.f || NewSpeed < NHAdvancedMovement::StopSpeedEpsilon)
		{
			Vel = FVector::ZeroVector;
			S.bHeavyStop = false;
		}
	}
	else if (bHasInput)
	{
		// Mirrors UCharacterMovementComponent::CalcVelocity with our grip and acceleration scaling.
		const float Grip = GroundFriction * FMath::Lerp(1.f, HighSpeedTurnGripScale, Speed01);
		Vel = Vel - (Vel - InputDirection * Speed) * FMath::Min(Dt * Grip, 1.f);
		Vel += InputDirection * MaxAcceleration * FMath::Lerp(1.f, SprintAccelerationScale, Speed01) * Dt;
		Vel = Vel.GetClampedToMaxSize(MaxSpeedForPrediction);
	}
	else if (Speed > 0.f)
	{
		// Mirrors ApplyVelocityBraking.
		const float Friction = (bUseSeparateBrakingFriction ? BrakingFriction : GroundFriction) * FMath::Max(0.f, BrakingFrictionFactor);
		const FVector RevAccel = -Friction * Vel - BrakingDecelerationWalking * (Vel / Speed);
		const FVector NewVel = Vel + RevAccel * Dt;
		Vel = ((NewVel | Vel) <= 0.f || NewVel.Size() < NHAdvancedMovement::StopSpeedEpsilon) ? FVector::ZeroVector : NewVel;
	}

	// Facing.
	float DesiredYaw = S.FacingYaw;
	if (bOrientRotationToMovement)
	{
		if (Vel.SizeSquared() > 1.f)
		{
			DesiredYaw = Vel.Rotation().Yaw;
		}
	}
	else if (bUseControllerDesiredRotation)
	{
		DesiredYaw = TargetControlYaw;
	}

	const float DeltaYaw = FMath::FindDeltaAngleDegrees(S.FacingYaw, DesiredYaw);
	if (RotationRate.Yaw >= 0.f)
	{
		const float MaxStep = RotationRate.Yaw * FMath::Lerp(1.f, HighSpeedRotationScale, Speed01) * Dt;
		S.FacingYaw = FRotator::NormalizeAxis(S.FacingYaw + FMath::Clamp(DeltaYaw, -MaxStep, MaxStep));
	}
	else
	{
		S.FacingYaw = DesiredYaw; // negative rate = instant
	}

	S.Velocity = Vel;
	S.Position += Vel * Dt;
}

void UNHAdvancedMovementComponent::PredictTrajectory()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	const float YawOffset = GetFacingYawOffset();
	const FVector CurrentPosition = CharacterOwner->GetMesh() ? CharacterOwner->GetMesh()->GetComponentLocation() : CharacterOwner->GetActorLocation();
	const float CurrentYaw = CharacterOwner->GetActorRotation().Yaw;
	const float CurrentSpeed = Velocity.Size2D();

	TrajectorySamples.Reset(HistoryOutputSamples + 1 + PredictionOutputSamples);

	// ---- History: evenly spaced, interpolated from the recorded buffer ----
	for (int32 k = HistoryOutputSamples; k >= 1; --k)
	{
		const float RelTime = -HistoryDuration * (static_cast<float>(k) / HistoryOutputSamples);
		const float QueryTime = Now + RelTime;

		FNHAdvancedTrajectorySample Sample;
		Sample.Time = RelTime;

		if (History.Num() == 0)
		{
			Sample.Position = CurrentPosition;
			Sample.FacingYaw = CurrentYaw + YawOffset;
			Sample.Speed = CurrentSpeed;
		}
		else if (QueryTime <= History[0].WorldTime)
		{
			Sample.Position = History[0].Position;
			Sample.FacingYaw = History[0].FacingYaw + YawOffset;
			Sample.Speed = History[0].Speed;
		}
		else
		{
			int32 i = History.Num() - 1;
			while (i > 0 && History[i - 1].WorldTime > QueryTime)
			{
				--i;
			}
			const FHistoryPoint& A = History[FMath::Max(i - 1, 0)];
			const FHistoryPoint& B = History[i];
			const float Span = FMath::Max(B.WorldTime - A.WorldTime, KINDA_SMALL_NUMBER);
			const float T = FMath::Clamp((QueryTime - A.WorldTime) / Span, 0.f, 1.f);
			Sample.Position = FMath::Lerp(A.Position, B.Position, T);
			Sample.FacingYaw = A.FacingYaw + FMath::FindDeltaAngleDegrees(A.FacingYaw, B.FacingYaw) * T + YawOffset;
			Sample.Speed = FMath::Lerp(A.Speed, B.Speed, T);
		}
		TrajectorySamples.Add(Sample);
	}

	// ---- Present ----
	TrajectorySamples.Add({ CurrentPosition, CurrentYaw + YawOffset, 0.f, CurrentSpeed });

	// ---- Future: integrate the same movement model, assuming input stays constant ----
	FPredictState S;
	S.Position = CurrentPosition;
	S.Velocity = FVector(Velocity.X, Velocity.Y, 0.f);
	S.FacingYaw = CurrentYaw;

	if (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		S.bHeavyStop = IsHeavyStopping();
		S.HeavyStopElapsed = ProxyHeavyStopElapsed;
		S.HeavyStopStartSpeed = ProxyHeavyStopStartSpeed;
		S.HeavyStopDirection = S.Velocity.GetSafeNormal2D();
	}
	else
	{
		S.bHeavyStop = bHeavyStopActive;
		S.HeavyStopElapsed = HeavyStopElapsed;
		S.HeavyStopStartSpeed = HeavyStopStartSpeed;
		S.HeavyStopDirection = HeavyStopDirection;
	}

	const FVector InputDirection = GetInputDirectionForAnim();
	const float MaxSpeedForPrediction = GetMaxSpeed();
	const float ControlYaw = CharacterOwner->GetBaseAimRotation().Yaw;

	const int32 NumSteps = FMath::Max(1, FMath::CeilToInt(PredictionDuration / FMath::Max(PredictionStep, 1.e-3f)));
	const float Dt = PredictionDuration / NumSteps;
	const float OutputInterval = PredictionDuration / PredictionOutputSamples;

	bHasPredictedStop = false;
	const bool bStoppingNow = S.bHeavyStop || InputDirection.IsNearlyZero();
	float Elapsed = 0.f;
	float NextOutput = OutputInterval;

	for (int32 Step = 0; Step < NumSteps; ++Step)
	{
		const bool bWasMoving = S.Velocity.SizeSquared() > 1.f;
		StepPrediction(S, InputDirection, MaxSpeedForPrediction, ControlYaw, Dt);
		Elapsed += Dt;

		if (bStoppingNow && !bHasPredictedStop && bWasMoving && S.Velocity.IsNearlyZero())
		{
			PredictedStopLocation = S.Position;
			bHasPredictedStop = true;
		}

		if (Elapsed + KINDA_SMALL_NUMBER >= NextOutput)
		{
			TrajectorySamples.Add({ S.Position, S.FacingYaw + YawOffset, Elapsed, S.Velocity.Size() });
			NextOutput += OutputInterval;
		}
	}
}

void UNHAdvancedMovementComponent::FillPoseSearchTrajectory()
{
	// The only place that touches the trajectory type (FTransformTrajectory since UE 5.6).
	PoseSearchTrajectory.Samples.SetNum(TrajectorySamples.Num(), EAllowShrinking::No);
	for (int32 i = 0; i < TrajectorySamples.Num(); ++i)
	{
		const FNHAdvancedTrajectorySample& Src = TrajectorySamples[i];
		FTransformTrajectorySample& Dst = PoseSearchTrajectory.Samples[i];
		Dst.Position = Src.Position;
		Dst.Facing = FRotator(0.f, Src.FacingYaw, 0.f).Quaternion();
		Dst.TimeInSeconds = Src.Time;
	}
}

void UNHAdvancedMovementComponent::DrawDebug() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (int32 i = 0; i < TrajectorySamples.Num(); ++i)
	{
		const FNHAdvancedTrajectorySample& S = TrajectorySamples[i];
		const FColor Color = S.Time < 0.f ? FColor::Cyan : (S.Time == 0.f ? FColor::White : FColor::Green);
		const FVector P = S.Position + FVector(0.f, 0.f, 3.f);

		DrawDebugSphere(World, P, 4.f, 6, Color, false, -1.f, 0, 1.f);
		DrawDebugDirectionalArrow(World, P, P + FRotator(0.f, S.FacingYaw, 0.f).Vector() * 25.f, 8.f, Color, false, -1.f, 0, 1.f);
		if (i > 0)
		{
			DrawDebugLine(World, TrajectorySamples[i - 1].Position + FVector(0.f, 0.f, 3.f), P, Color, false, -1.f, 0, 1.5f);
		}
	}

	if (bHasPredictedStop)
	{
		DrawDebugCircle(World, PredictedStopLocation + FVector(0.f, 0.f, 2.f), 30.f, 16, FColor::Red, false, -1.f, 0, 2.f,
			FVector::ForwardVector, FVector::RightVector, false);
	}
#endif
}

/* =============================================================================================
 *  Saved moves (client prediction / replay)
 * ============================================================================================= */

void FNHSavedMove_Advanced::Clear()
{
	Super::Clear();
	bSavedWantsToSprint = 0;
	bSavedHeavyStopActive = 0;
	SavedHeavyStopElapsed = 0.f;
	SavedHeavyStopStartSpeed = 0.f;
	SavedHeavyStopDirection = FVector::ForwardVector;
}

uint8 FNHSavedMove_Advanced::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (bSavedWantsToSprint)
	{
		Result |= FLAG_Custom_0;
	}
	return Result;
}

bool FNHSavedMove_Advanced::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FNHSavedMove_Advanced* Other = static_cast<const FNHSavedMove_Advanced*>(NewMove.Get());
	if (bSavedWantsToSprint != Other->bSavedWantsToSprint || bSavedHeavyStopActive != Other->bSavedHeavyStopActive)
	{
		return false; // never merge across a gait change or the start/end of a heavy stop
	}
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FNHSavedMove_Advanced::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	// Captured at the START of the move, before it is performed.
	if (const UNHAdvancedMovementComponent* Move = Cast<UNHAdvancedMovementComponent>(C->GetCharacterMovement()))
	{
		bSavedWantsToSprint = Move->bWantsToSprint;
		bSavedHeavyStopActive = Move->bHeavyStopActive;
		SavedHeavyStopElapsed = Move->HeavyStopElapsed;
		SavedHeavyStopStartSpeed = Move->HeavyStopStartSpeed;
		SavedHeavyStopDirection = Move->HeavyStopDirection;
	}
}

void FNHSavedMove_Advanced::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	// Restore heavy-stop state before each replayed move after a server correction,
	// so the replay follows the same velocity curve instead of popping.
	if (UNHAdvancedMovementComponent* Move = Cast<UNHAdvancedMovementComponent>(C->GetCharacterMovement()))
	{
		Move->bWantsToSprint = bSavedWantsToSprint;
		Move->bHeavyStopActive = bSavedHeavyStopActive;
		Move->HeavyStopElapsed = SavedHeavyStopElapsed;
		Move->HeavyStopStartSpeed = SavedHeavyStopStartSpeed;
		Move->HeavyStopDirection = SavedHeavyStopDirection;
	}
}

FSavedMovePtr FNHNetworkPredictionData_Client_Advanced::AllocateNewMove()
{
	return FSavedMovePtr(new FNHSavedMove_Advanced());
}
