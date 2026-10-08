// NHVehicleDynamicsComponent.cpp

#include "Vehicles/NHVehicleDynamicsComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Net/UnrealNetwork.h"

namespace NHVehicleDynamics
{
	static constexpr float Gravity = 981.f;       // cm/s²
	static constexpr float SideQuantum = 4.f;     // cm/s per replicated unit
	static constexpr float TeleportDistance = 600.f;

	/** One semi-implicit step of a spring-damper towards Target */
	FORCEINLINE void Spring(float& Value, float& Rate, float Target, float Frequency, float Damping, float Step)
	{
		const float Omega = 2.f * PI * Frequency;
		Rate += (-Omega * Omega * (Value - Target) - 2.f * Damping * Omega * Rate) * Step;
		Value += Rate * Step;
	}
}

UNHVehicleDynamicsComponent::UNHVehicleDynamicsComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // the vehicle calls the steps itself, in order, after it has moved
	SetIsReplicatedByDefault(true);

	// matched against the surface materials the city is built from (MI_NHSurface_<Type>)
	SurfaceGrip.Add(TEXT("Asphalt"), 1.f);
	SurfaceGrip.Add(TEXT("Concrete"), 0.95f);
	SurfaceGrip.Add(TEXT("Dirt"), 0.7f);
	SurfaceGrip.Add(TEXT("Wood"), 0.8f);
	SurfaceGrip.Add(TEXT("Metal"), 0.65f);
	SurfaceGrip.Add(TEXT("Zinc"), 0.65f);

	CurrentGrip = GripAcceleration;
}

void UNHVehicleDynamicsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UNHVehicleDynamicsComponent, RepSlide, COND_SkipOwner);
}

void UNHVehicleDynamicsComponent::OnRep_Slide()
{
	// another machine is driving: show its slide
	SideSpeed = RepSlide.SideQuantised * NHVehicleDynamics::SideQuantum;
	bHandbrakeOn = (RepSlide.Flags & 0x1) != 0;
	bSliding = (RepSlide.Flags & 0x2) != 0;
}

/* ---- Setup ------------------------------------------------------------------------------- */

void UNHVehicleDynamicsComponent::Setup(USceneComponent* Body, const TArray<USceneComponent*>& WheelHubs, float GroundDrop, bool bBike)
{
	BodyComponent = Body;
	BodyRest = Body ? Body->GetRelativeLocation() : FVector::ZeroVector;
	GroundDropCm = GroundDrop;
	bIsBike = bBike;
	Wheels.Reset(WheelHubs.Num());
	for (USceneComponent* Hub : WheelHubs)
	{
		if (Hub)
		{
			FWheel& W = Wheels.AddDefaulted_GetRef();
			W.Hub = Hub;
			W.Rest = Hub->GetRelativeLocation();
		}
	}
	ResetDynamics();
}

void UNHVehicleDynamicsComponent::ResetDynamics()
{
	SideSpeed = SlipAngle = 0.f;
	bSliding = bHandbrakeOn = false;
	Roll = RollRate = Pitch = PitchRate = Heave = HeaveRate = 0.f;
	bHasLast = false;
	for (FWheel& W : Wheels)
	{
		W.Ground = W.Compression = 0.f;
	}
	ApplyToBody(0.f);
}

int32 UNHVehicleDynamicsComponent::Substeps(float DeltaSeconds, float& OutStep) const
{
	const int32 N = FMath::Clamp(FMath::CeilToInt(DeltaSeconds / FMath::Max(MaxSubstepSeconds, 1.e-4f)), 1, FMath::Max(MaxSubsteps, 1));
	OutStep = DeltaSeconds / N;
	return N;
}

/* ---- Grip -------------------------------------------------------------------------------- */

float UNHVehicleDynamicsComponent::ReadWetness()
{
	if (!bWeatherLooked)
	{
		bWeatherLooked = true;
		if (!WeatherCollection)
		{
			WeatherCollection = TSoftObjectPtr<UMaterialParameterCollection>(FSoftObjectPath(TEXT("/Game/NaijaHustle/Lighting/Presets/MPC_NHWeather.MPC_NHWeather"))).LoadSynchronous();
		}
		UWorld* World = GetWorld();
		WeatherInstance = (World && WeatherCollection) ? World->GetParameterCollectionInstance(WeatherCollection) : nullptr;
	}
	float Wet = 0.f;
	if (UMaterialParameterCollectionInstance* Weather = WeatherInstance.Get())
	{
		static const FName WetnessName(TEXT("Wetness"));
		Weather->GetScalarParameterValue(WetnessName, Wet);
	}
	return FMath::Clamp(Wet, 0.f, 1.f);
}

float UNHVehicleDynamicsComponent::GripOf(const UMaterialInterface* Material)
{
	if (!Material)
	{
		return DefaultSurfaceGrip;
	}
	if (const float* Known = GripCache.Find(Material))
	{
		return *Known; // the name is only searched the first time a material is driven over
	}
	float Grip = DefaultSurfaceGrip;
	const FString Name = Material->GetName();
	for (const TPair<FString, float>& Pair : SurfaceGrip)
	{
		if (Name.Contains(Pair.Key))
		{
			Grip = Pair.Value;
			break;
		}
	}
	GripCache.Add(Material, Grip);
	return Grip;
}

float UNHVehicleDynamicsComponent::StepTraction(float DeltaSeconds, float& Speed, float YawRate, bool bHandbrake)
{
	if (DeltaSeconds <= 0.f)
	{
		return 0.f;
	}
	bHandbrakeOn = bHandbrake;

	const float Wet = ReadWetness();
	const float BaseGrip = GripAcceleration * GroundGrip * FMath::Lerp(1.f, WetGripRatio, Wet) * (bHandbrake ? HandbrakeGripRatio : 1.f);

	float Step;
	const int32 N = Substeps(DeltaSeconds, Step);
	float Slid = 0.f;
	for (int32 i = 0; i < N; ++i)
	{
		// Turning the car leaves its momentum pointing the old way: seen from the new heading, that is sideways speed
		SideSpeed -= Speed * YawRate * Step;

		// the tyres pull that sideways speed back, as hard as they can grip
		CurrentGrip = BaseGrip * (bSliding ? SlidingGripRatio : 1.f);
		const float Pull = CurrentGrip * Step;
		if (FMath::Abs(SideSpeed) <= Pull)
		{
			SideSpeed = 0.f; // within the limit: no slide at all
		}
		else
		{
			SideSpeed -= FMath::Sign(SideSpeed) * Pull;
		}

		const float Side = FMath::Abs(SideSpeed);
		bSliding = bSliding ? Side > SlideEndSpeed : Side > SlideStartSpeed;

		// sliding tyres scrub speed off
		if (bSliding)
		{
			const float SinSlip = Side / FMath::Max(FMath::Sqrt(Speed * Speed + Side * Side), 1.f);
			Speed *= FMath::Max(0.f, 1.f - SlideScrub * SinSlip * Step);
		}
		Slid += SideSpeed * Step;
	}

	SlipAngle = (FMath::Abs(SideSpeed) > 1.f || FMath::Abs(Speed) > 1.f) ? FMath::RadiansToDegrees(FMath::Atan2(SideSpeed, FMath::Abs(Speed))) : 0.f;

	// plain property writes: replication only sends them when the packed value changes
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		const int16 Quantised = static_cast<int16>(FMath::Clamp(FMath::RoundToInt(SideSpeed / NHVehicleDynamics::SideQuantum), -32767, 32767));
		const uint8 Flags = (bHandbrake ? 0x1 : 0x0) | (bSliding ? 0x2 : 0x0);
		if (Quantised != RepSlide.SideQuantised || Flags != RepSlide.Flags)
		{
			RepSlide.SideQuantised = Quantised;
			RepSlide.Flags = Flags;
		}
	}
	return Slid;
}

/* ---- Body and wheels --------------------------------------------------------------------- */

void UNHVehicleDynamicsComponent::TraceWheels(float DeltaSeconds)
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}
	static const FName TraceTag(TEXT("NHVehicleWheel"));
	const FCollisionQueryParams Params(TraceTag, /*bTraceComplex*/ false, Owner);
	const FTransform& ToWorld = Owner->GetActorTransform();
	const float LevelZ = Owner->GetActorLocation().Z - GroundDropCm; // where the ground is if the car stands level
	const float Follow = 1.f - FMath::Exp(-DeltaSeconds * 25.f);     // wheels drop into a rut quickly, but not in one frame

	float GripSum = 0.f;
	int32 GripCount = 0;
	for (FWheel& W : Wheels)
	{
		// the wheel's place on a level car (not where the leaning body has carried it)
		const FVector At = ToWorld.TransformPosition(BodyRest + FVector(W.Rest.X, W.Rest.Y, 0.f));
		FHitResult Hit;
		float Target = 0.f;
		if (World->LineTraceSingleByObjectType(Hit, FVector(At.X, At.Y, LevelZ + 80.f), FVector(At.X, At.Y, LevelZ - 80.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			Target = FMath::Clamp(Hit.ImpactPoint.Z - LevelZ, -SuspensionTravel, SuspensionTravel);
			const UPrimitiveComponent* Under = Hit.GetComponent();
			GripSum += GripOf(Under ? Under->GetMaterial(0) : nullptr);
			++GripCount;
		}
		W.Ground = FMath::Lerp(W.Ground, Target, Follow);
	}
	if (GripCount > 0)
	{
		GroundGrip = GripSum / GripCount;
	}
}

void UNHVehicleDynamicsComponent::StepChassis(float DeltaSeconds, float ExtraRoll)
{
	const AActor* Owner = GetOwner();
	if (!Owner || DeltaSeconds <= 0.f || !BodyComponent.IsValid())
	{
		return;
	}

	// ---- what the vehicle did since the last call
	const FVector Location = Owner->GetActorLocation();
	if (!bHasLast || FVector::DistSquared(Location, LastLocation) > FMath::Square(NHVehicleDynamics::TeleportDistance))
	{
		LastLocation = Location;
		LastVelocity = FVector::ZeroVector;
		bHasLast = true;
	}
	const FVector Velocity = (Location - LastLocation) / DeltaSeconds;
	const FVector Accel = (Velocity - LastVelocity) / DeltaSeconds;
	const float BumpSpeed = Velocity.Z - LastVelocity.Z; // a kerb or a landing: vertical speed changed at once
	LastLocation = Location;
	LastVelocity = Velocity;

	const bool bMoving = Velocity.SizeSquared() > 4.f;
	const bool bSettled = FMath::Abs(Roll) < 0.02f && FMath::Abs(Pitch) < 0.02f && FMath::Abs(Heave) < 0.02f
		&& FMath::Abs(RollRate) < 0.05f && FMath::Abs(PitchRate) < 0.05f && FMath::Abs(HeaveRate) < 0.05f;
	if (!bMoving && bSettled && FMath::Abs(BumpSpeed) < 1.f)
	{
		return; // parked and at rest: no traces, no springs, no transform updates
	}

	if (bTraceWheels && bMoving && Wheels.Num() > 0)
	{
		TraceWheels(DeltaSeconds);
	}

	// ---- weight transfer: where the springs want to be
	const float AlongAccel = Accel | Owner->GetActorForwardVector();
	const float AcrossAccel = Accel | Owner->GetActorRightVector();
	float RollTarget = bIsBike ? 0.f : FMath::Clamp(-AcrossAccel / NHVehicleDynamics::Gravity * RollPerG, -MaxRoll, MaxRoll); // thrown outwards
	float PitchTarget = FMath::Clamp(AlongAccel / NHVehicleDynamics::Gravity * PitchPerG, -MaxPitch, MaxPitch);                 // squat and dive
	float HeaveTarget = 0.f;

	// ---- the ground under the wheels tilts and lifts the body
	if (Wheels.Num() >= 2)
	{
		float Sum = 0.f, FrontSum = 0.f, RearSum = 0.f, LeftSum = 0.f, RightSum = 0.f;
		int32 Front = 0, Rear = 0, Left = 0, Right = 0;
		float FrontX = 0.f, RearX = 0.f, LeftY = 0.f, RightY = 0.f;
		for (const FWheel& W : Wheels)
		{
			Sum += W.Ground;
			if (W.Rest.X >= 0.f) { FrontSum += W.Ground; FrontX += W.Rest.X; ++Front; } else { RearSum += W.Ground; RearX += W.Rest.X; ++Rear; }
			if (W.Rest.Y >= 0.f) { RightSum += W.Ground; RightY += W.Rest.Y; ++Right; } else { LeftSum += W.Ground; LeftY += W.Rest.Y; ++Left; }
		}
		HeaveTarget = Sum / Wheels.Num() * GroundFollow;
		if (Front > 0 && Rear > 0)
		{
			const float Wheelbase = FMath::Max(FrontX / Front - RearX / Rear, 1.f);
			PitchTarget += FMath::RadiansToDegrees(FMath::Atan2(FrontSum / Front - RearSum / Rear, Wheelbase)) * GroundFollow;
		}
		if (Left > 0 && Right > 0 && !bIsBike)
		{
			const float Track = FMath::Max(RightY / Right - LeftY / Left, 1.f);
			RollTarget += FMath::RadiansToDegrees(FMath::Atan2(RightSum / Right - LeftSum / Left, Track)) * GroundFollow * -1.f; // right side up = roll left
		}
	}

	// ---- springs, in fixed steps
	HeaveRate -= BumpSpeed * BumpTransfer; // the wheels went up; the body, being heavy, has not yet
	float Step;
	const int32 N = Substeps(DeltaSeconds, Step);
	for (int32 i = 0; i < N; ++i)
	{
		NHVehicleDynamics::Spring(Roll, RollRate, RollTarget, BodyFrequency, BodyDamping, Step);
		NHVehicleDynamics::Spring(Pitch, PitchRate, PitchTarget, BodyFrequency, BodyDamping, Step);
		NHVehicleDynamics::Spring(Heave, HeaveRate, HeaveTarget, BodyFrequency * 1.3f, BodyDamping, Step);
	}
	// bump stops
	if (FMath::Abs(Heave) > SuspensionTravel)
	{
		Heave = FMath::Clamp(Heave, -SuspensionTravel, SuspensionTravel);
		HeaveRate *= -0.3f;
	}
	Roll = FMath::Clamp(Roll, -MaxRoll * 1.5f, MaxRoll * 1.5f);
	Pitch = FMath::Clamp(Pitch, -MaxPitch * 1.5f, MaxPitch * 1.5f);

	ApplyToBody(ExtraRoll);
}

void UNHVehicleDynamicsComponent::ApplyToBody(float ExtraRoll)
{
	USceneComponent* Body = BodyComponent.Get();
	if (!Body)
	{
		return;
	}
	const float TotalRoll = Roll + ExtraRoll;
	Body->SetRelativeLocationAndRotation(BodyRest + FVector(0.f, 0.f, Heave), FRotator(Pitch, 0.f, TotalRoll));

	// keep each wheel on the ground while the body moves over it: undo the body's motion at that corner
	// (a bike's lean is the whole machine tipping, wheels included, so only the sprung motion is undone)
	const float SinPitch = FMath::Sin(FMath::DegreesToRadians(Pitch)), SinRoll = FMath::Sin(FMath::DegreesToRadians(Roll));
	for (FWheel& W : Wheels)
	{
		USceneComponent* Hub = W.Hub.Get();
		if (!Hub)
		{
			continue;
		}
		const float BodyLift = Heave + W.Rest.X * SinPitch - W.Rest.Y * SinRoll; // nose up lifts the front; rolling right drops the right side
		W.Compression = FMath::Clamp(W.Ground - BodyLift, -SuspensionTravel, SuspensionTravel);
		Hub->SetRelativeLocation(W.Rest + FVector(0.f, 0.f, W.Compression));
	}
}

float UNHVehicleDynamicsComponent::GetWheelCompression(int32 WheelIndex) const
{
	return Wheels.IsValidIndex(WheelIndex) ? Wheels[WheelIndex].Compression : 0.f;
}
