#include "Player/NHClipAnimInstance.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UNHClipAnimInstance::SetClips(UAnimSequence* InIdle, UAnimSequence* InWalk, UAnimSequence* InRun, UAnimSequence* InSprint, UAnimSequence* InJump)
{
	IdleClip = InIdle;
	RunClip = InRun;
	WalkClip = InWalk ? InWalk : InRun;
	SprintClip = InSprint ? InSprint : InRun;
	JumpClip = InJump;
}

void FNHClipAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	const UNHClipAnimInstance* Instance = CastChecked<UNHClipAnimInstance>(InAnimInstance);
	Clips[Idle] = Instance->IdleClip;
	Clips[Walk] = Instance->WalkClip;
	Clips[Run] = Instance->RunClip;
	Clips[Sprint] = Instance->SprintClip;
	Clips[Jump] = Instance->JumpClip;
	Speeds[Idle] = 0.f;
	Speeds[Walk] = Instance->WalkAt;
	Speeds[Run] = Instance->RunAt;
	Speeds[Sprint] = Instance->SprintAt;

	const ACharacter* Owner = Cast<ACharacter>(InAnimInstance->GetOwningActor());
	const bool bFalling = Owner && Owner->GetCharacterMovement()->IsFalling();
	const float Target = Owner ? Owner->GetVelocity().Size2D() : 0.f;
	Speed = FMath::FInterpTo(Speed, Target, DeltaSeconds, 8.f);

	// which two clips the speed sits between
	From = Idle;
	while (From < Sprint - 1 && Speed >= Speeds[From + 1])
	{
		++From;
	}
	To = From + 1;
	Alpha = FMath::Clamp((Speed - Speeds[From]) / FMath::Max(Speeds[To] - Speeds[From], 1.f), 0.f, 1.f);

	if (Clips[Idle])
	{
		IdleTime = FMath::Fmod(IdleTime + DeltaSeconds, FMath::Max(Clips[Idle]->GetPlayLength(), 0.01f));
	}
	// the moving clips share one stride, so a walk blends into a run foot for foot; its length follows the blend
	const auto Length = [this](int32 Clip) { return Clip != Idle && Clips[Clip] ? FMath::Max(Clips[Clip]->GetPlayLength(), 0.01f) : 0.f; };
	const float A = Length(From), B = Length(To);
	const float StrideLength = A > 0.f && B > 0.f ? FMath::Lerp(A, B, Alpha) : FMath::Max(A, B);
	if (StrideLength > 0.f)
	{
		// faster than the sprint clip's speed, the legs turn over faster
		const float Rate = FMath::Clamp(Speed / FMath::Max(Speeds[Sprint], 1.f), 1.f, 1.5f);
		Stride = FMath::Frac(Stride + DeltaSeconds * Rate / StrideLength);
	}

	// the jump: skip the crouch at its start, hold near the top while still in the air
	if (Clips[Jump])
	{
		const float JumpLength = Clips[Jump]->GetPlayLength();
		if (bFalling && !bWasFalling)
		{
			JumpTime = 0.2f * JumpLength;
		}
		JumpTime = FMath::Min(JumpTime + DeltaSeconds, (bFalling ? 0.6f : 1.f) * JumpLength);
		Air = FMath::FInterpConstantTo(Air, bFalling ? 1.f : 0.f, DeltaSeconds, bFalling ? 8.f : 5.f);
	}
	bWasFalling = bFalling;
}

bool FNHClipAnimProxy::Evaluate(FPoseContext& Output)
{
	const auto Sample = [this](int32 Clip, FPoseContext& Pose)
	{
		if (!Clips[Clip])
		{
			Pose.ResetToRefPose();
			return;
		}
		const float Length = Clips[Clip]->GetPlayLength();
		const float Time = Clip == Idle ? IdleTime : Clip == Jump ? JumpTime : Stride * Length;
		FAnimationPoseData Data(Pose);
		Clips[Clip]->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(FMath::Clamp(Time, 0.f, Length))));
	};

	if (Alpha <= KINDA_SMALL_NUMBER || Alpha >= 1.f - KINDA_SMALL_NUMBER)
	{
		Sample(Alpha < 0.5f ? From : To, Output);
	}
	else
	{
		FPoseContext A(Output), B(Output);
		Sample(From, A);
		Sample(To, B);
		FAnimationPoseData DataA(A), DataB(B), Out(Output);
		FAnimationRuntime::BlendTwoPosesTogether(DataA, DataB, 1.f - Alpha, Out);
	}

	if (Air > KINDA_SMALL_NUMBER && Clips[Jump])
	{
		FPoseContext Ground(Output), Jumping(Output);
		Ground = Output;
		Sample(Jump, Jumping);
		FAnimationPoseData DataA(Ground), DataB(Jumping), Out(Output);
		FAnimationRuntime::BlendTwoPosesTogether(DataA, DataB, 1.f - Air, Out);
	}
	return true;
}
