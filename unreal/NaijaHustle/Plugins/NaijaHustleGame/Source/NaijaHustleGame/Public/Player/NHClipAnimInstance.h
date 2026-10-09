#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "NHClipAnimInstance.generated.h"

class UAnimSequence;

/** The worker-thread side of UNHClipAnimInstance: blends the clips into the pose */
USTRUCT()
struct FNHClipAnimProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FNHClipAnimProxy() = default;
	explicit FNHClipAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

protected:
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	enum { Idle, Walk, Run, Sprint, Jump, Count };
	const UAnimSequence* Clips[Count] = {};
	/** Ground speed each of Idle..Sprint is made for, cm/s */
	float Speeds[4] = {};
	/** The two clips being blended and how far toward the second, 0..1 */
	int32 From = Idle;
	int32 To = Idle;
	float Alpha = 0.f;
	/** Eased ground speed, cm/s */
	float Speed = 0.f;
	/** Seconds into the idle, and 0..1 through the stride the moving clips share */
	float IdleTime = 0.f;
	float Stride = 0.f;
	/** Seconds into the jump, and 0..1 how much of it shows */
	float JumpTime = 0.f;
	float Air = 0.f;
	bool bWasFalling = false;
};

/**
 * Moves a body with its own in-place animation clips and no animation Blueprint: idle, walk, run and sprint are
 * blended by ground speed (the moving ones kept in step through a shared stride), and the jump shows while in the
 * air. For characters that come with their animations, like the Lagos Runner made in Blender.
 */
UCLASS(Transient)
class NAIJAHUSTLEGAME_API UNHClipAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Idle and Run are needed; a missing Walk, Sprint or Jump falls back to its neighbour */
	void SetClips(UAnimSequence* InIdle, UAnimSequence* InWalk, UAnimSequence* InRun, UAnimSequence* InSprint, UAnimSequence* InJump);

	UPROPERTY(Transient) TObjectPtr<UAnimSequence> IdleClip;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> WalkClip;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> RunClip;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> SprintClip;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> JumpClip;

	/** Ground speed each moving clip is made for, cm/s: the player walks at 350 and sprints at 650 */
	float WalkAt = 150.f;
	float RunAt = 350.f;
	float SprintAt = 650.f;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FNHClipAnimProxy(this); }
};
