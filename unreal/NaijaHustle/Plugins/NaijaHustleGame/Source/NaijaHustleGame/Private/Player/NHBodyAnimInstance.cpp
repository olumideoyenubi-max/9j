#include "Player/NHBodyAnimInstance.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"

void UNHBodyAnimInstance::ShowClip(UAnimSequence* InClip, float InRate, bool bInLoop, float InFade)
{
	Clip = InClip;
	Rate = InRate;
	bLoop = bInLoop;
	Fade = InFade;
	++Serial;
}

void FNHBodyAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	const UNHBodyAnimInstance* Instance = CastChecked<UNHBodyAnimInstance>(InAnimInstance);
	if (Instance->Serial != Serial)
	{
		Serial = Instance->Serial;
		if (Instance->Clip)
		{
			// what was showing is held where it was while the new clip comes in over it
			Before = Cross >= 0.5f ? Now : Before;
			Now.Clip = Instance->Clip;
			Now.Time = 0.f;
			Now.Rate = Instance->Rate;
			Now.bLoop = Instance->bLoop;
			Cross = Before.Clip && Weight > 0.01f ? 0.f : 1.f;
		}
		CrossRate = 1.f / FMath::Max(Instance->Fade, 0.01f);
	}
	const bool bShow = Instance->Clip != nullptr;
	if (Now.Clip)
	{
		const float Length = FMath::Max(Now.Clip->GetPlayLength(), 0.01f);
		Now.Time += DeltaSeconds * Now.Rate;
		Now.Time = Now.bLoop ? FMath::Fmod(Now.Time, Length) : FMath::Min(Now.Time, Length);
	}
	Cross = FMath::Min(1.f, Cross + DeltaSeconds * CrossRate);
	Weight = FMath::FInterpConstantTo(Weight, bShow ? 1.f : 0.f, DeltaSeconds, CrossRate);
	Trunk = FMath::FInterpConstantTo(Trunk, Instance->WantTrunk, DeltaSeconds, 5.f);
	Legs = FMath::FInterpConstantTo(Legs, Instance->WantLegs, DeltaSeconds, 5.f);
	if (Weight <= 0.f && !bShow)
	{
		Now = Before = FPlaying();
	}
}

bool FNHBodyAnimProxy::Evaluate(FPoseContext& Output)
{
	EvaluateAnimationNode(Output); // the blueprint's own graph: the walk, the run, the jump
	if (Weight <= KINDA_SMALL_NUMBER || !Now.Clip)
	{
		return true;
	}
	const auto Sample = [](const FPlaying& Playing, FPoseContext& Pose)
	{
		FAnimationPoseData Data(Pose);
		Playing.Clip->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(FMath::Clamp(Playing.Time, 0.f, Playing.Clip->GetPlayLength()))));
	};
	FPoseContext Layer(Output);
	if (Cross < 1.f - KINDA_SMALL_NUMBER && Before.Clip)
	{
		FPoseContext A(Output), B(Output);
		Sample(Before, A);
		Sample(Now, B);
		FAnimationPoseData DataA(A), DataB(B), Out(Layer);
		const float Eased = FMath::SmoothStep(0.f, 1.f, Cross);
		FAnimationRuntime::BlendTwoPosesTogether(DataA, DataB, 1.f - Eased, Out);
	}
	else
	{
		Sample(Now, Layer);
	}

	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const auto Find = [&Bones](const TCHAR* Name)
	{
		const int32 Mesh = Bones.GetPoseBoneIndexForBoneName(Name);
		return Mesh == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh));
	};
	const FCompactPoseBoneIndex Spine = Find(TEXT("spine_01")), Arm = Find(TEXT("clavicle_r"));
	if (Spine.GetInt() == INDEX_NONE || Arm.GetInt() == INDEX_NONE)
	{
		return true; // not one of the bodies the clips were made for
	}
	// which way a bone faces in the body's own space, in a pose
	const auto Facing = [&Bones](const FCompactPose& Pose, FCompactPoseBoneIndex Bone)
	{
		FQuat Q = FQuat::Identity;
		for (; Bone.GetInt() != INDEX_NONE; Bone = Bones.GetParentBoneIndex(Bone))
		{
			Q = Pose[Bone].GetRotation() * Q;
		}
		return Q;
	};
	// The trunk faces the way the clip faces it whatever the blueprint's hips are doing under it: a rifle stance turns
	// the hips a third of the way round, and laid over hips that are running straight it would aim off to one side.
	const FCompactPoseBoneIndex Hips = Bones.GetParentBoneIndex(Spine);
	const FQuat SpineTurn = Hips.GetInt() == INDEX_NONE ? Layer.Pose[Spine].GetRotation() : Facing(Output.Pose, Hips).Inverse() * Facing(Layer.Pose, Spine);

	// 0 the hips and legs, 1 the trunk, 2 the right arm; a parent always comes before its children
	TArray<uint8, TInlineAllocator<128>> Part;
	Part.SetNumZeroed(Output.Pose.GetNumBones());
	for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
	{
		const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone);
		const uint8 Above = Parent.GetInt() == INDEX_NONE ? 0 : Part[Parent.GetInt()];
		Part[Bone.GetInt()] = Bone == Arm ? 2 : Bone == Spine ? FMath::Max<uint8>(Above, 1) : Above;
		const float Reach[] = { Legs, Trunk, 1.f };
		const float K = Weight * Reach[Part[Bone.GetInt()]];
		if (K <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		FTransform Want = Layer.Pose[Bone];
		if (Bone == Spine)
		{
			Want.SetRotation(FQuat::Slerp(SpineTurn, Want.GetRotation(), Legs)); // with the clip's own hips under it, its own turn
		}
		FTransform& Have = Output.Pose[Bone];
		Have.SetTranslation(FMath::Lerp(Have.GetTranslation(), Want.GetTranslation(), K));
		Have.SetRotation(FQuat::Slerp(Have.GetRotation(), Want.GetRotation(), K));
		Have.SetScale3D(FMath::Lerp(Have.GetScale3D(), Want.GetScale3D(), K));
	}
	return true;
}
