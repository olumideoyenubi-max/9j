#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "NHBodyAnimInstance.generated.h"

class UAnimSequence;

/** The worker-thread side of UNHBodyAnimInstance: runs the blueprint's graph, then lays the action clip over it */
USTRUCT()
struct FNHBodyAnimProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FNHBodyAnimProxy() = default;
	explicit FNHBodyAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

protected:
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	struct FPlaying
	{
		const UAnimSequence* Clip = nullptr;
		float Time = 0.f;
		float Rate = 1.f;
		bool bLoop = true;
	};
	/** The clip showing, and the one it is taking over from (held where it was) */
	FPlaying Now, Before;
	/** 0..1 from Before to Now, and how fast, a second */
	float Cross = 1.f;
	float CrossRate = 5.f;
	int32 Serial = 0;
	/** Eased 0..1: how much of the clip shows at all, on the trunk and left arm, and on the hips and legs */
	float Weight = 0.f;
	float Trunk = 0.f;
	float Legs = 0.f;
};

/**
 * The parent of the bodies' animation blueprints (set by Scripts/reparent_body_anims.py). The blueprint goes on
 * walking, running and jumping the body as before; over it this lays one of the player's action clips: always on the
 * right arm, and as far as asked on the rest of the trunk and on the legs. So a weapon is carried, raised and fired
 * by the arms while the blueprint's legs walk, and the whole clip (its stance too) shows only while stood still.
 */
UCLASS()
class NAIJAHUSTLEGAME_API UNHBodyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Shows a clip from its start, taking Fade seconds over it; nullptr lets the blueprint's own pose back */
	void ShowClip(UAnimSequence* Clip, float Rate, bool bLoop, float Fade);
	/** How far the clip reaches past the right arm, 0..1: the trunk, head and left arm; and the hips and legs */
	void SetReach(float InTrunk, float InLegs) { WantTrunk = InTrunk; WantLegs = InLegs; }
	const UAnimSequence* Showing() const { return Clip; }

	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Clip;
	float Rate = 1.f, Fade = 0.2f, WantTrunk = 1.f, WantLegs = 0.f;
	bool bLoop = true;
	/** Goes up each time a clip is shown, so the same clip can be started again */
	int32 Serial = 0;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FNHBodyAnimProxy(this); }
};
