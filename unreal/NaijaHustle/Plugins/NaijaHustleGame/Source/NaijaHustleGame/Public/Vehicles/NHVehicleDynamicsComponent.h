// NHVehicleDynamicsComponent.h
//
// Weight, suspension and grip for the game's vehicles.
//
// ANHVehicle is a kinematic pawn: it works out its own speed and heading and is swept through the world. It has
// no physics body, so Chaos Vehicles (which needs a simulated, rigged skeletal mesh with wheel bones) cannot drive
// it. This component adds what that arcade model lacks, in three parts the vehicle calls each frame:
//
//  * StepTraction: tyre grip. Turning asks the tyres for sideways force; when the demand is more than the grip
//    (speed, handbrake, dirt, a wet road), the car slides sideways and has to be caught. Below the limit it adds
//    nothing, so ordinary driving is unchanged.
//  * StepChassis: the sprung body. Roll, pitch and heave are spring-dampers pushed by the accelerations the
//    vehicle actually achieved, so braking dives, launching squats, cornering rolls, and a kerb or a landing
//    compresses the suspension and rebounds.
//  * Wheels: each wheel is traced to the ground and kept planted while the body moves over it.
//
// Frame-rate independence: both simulations run in fixed sub-steps (MaxSubstepSeconds), like physics
// sub-stepping, so a slide or a rebound is the same at 20 fps and at 120.
//
// Network model: the body and wheel motion is derived on every machine from how the vehicle is seen to move, so
// it is never replicated. Only the slide (sideways speed, handbrake, sliding flag) is, quantised and skipped for
// the owner, for vehicles whose movement the server replicates.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NHVehicleDynamicsComponent.generated.h"

class UMaterialInterface;
class UMaterialParameterCollection;
class UMaterialParameterCollectionInstance;

/** The slide, as sent to other machines: 3 bytes. */
USTRUCT()
struct FNHVehicleSlideState
{
	GENERATED_BODY()

	/** Sideways speed in units of 4 cm/s, + to the right */
	UPROPERTY() int16 SideQuantised = 0;
	/** bit 0 = handbrake on, bit 1 = sliding */
	UPROPERTY() uint8 Flags = 0;
};

UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent))
class NAIJAHUSTLEGAME_API UNHVehicleDynamicsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNHVehicleDynamicsComponent();

	// ---- Setup ------------------------------------------------------------------------------

	/**
	 * Body: the component the visible vehicle hangs from, with its origin on the ground under the middle of the car.
	 * WheelHubs: children of Body, one per wheel, at the wheel centres. GroundDrop: how far below the actor's
	 * origin the ground is when the vehicle stands level. Call again after rebuilding the body.
	 */
	void Setup(USceneComponent* Body, const TArray<USceneComponent*>& WheelHubs, float GroundDrop, bool bBike);
	/** Level, still and gripping: after a teleport, a repair or a respawn */
	UFUNCTION(BlueprintCallable, Category = "Vehicle Dynamics") void ResetDynamics();

	// ---- Grip and slides --------------------------------------------------------------------

	/** Sideways acceleration the tyres can hold on dry asphalt (cm/s²). Arcade-scaled: the game's cars turn far harder than real ones. */
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Grip") float GripAcceleration = 3400.f;
	/** Once sliding, grip is this fraction of normal until the slide is nearly caught (a sliding tyre grips less than a rolling one) */
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Grip", meta = (ClampMin = "0.3", ClampMax = "1")) float SlidingGripRatio = 0.8f;
	/** Grip left with the handbrake on (the rear wheels are locked) */
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Grip", meta = (ClampMin = "0.05", ClampMax = "1")) float HandbrakeGripRatio = 0.3f;
	/** Grip on a fully wet road, as a fraction of dry */
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Grip", meta = (ClampMin = "0.1", ClampMax = "1")) float WetGripRatio = 0.6f;
	/** Grip by surface: a word found in the name of the material under the wheels, and the fraction of asphalt grip it gives */
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Grip") TMap<FString, float> SurfaceGrip;
	/** Grip on a surface that matches nothing in SurfaceGrip */
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Grip") float DefaultSurfaceGrip = 0.9f;
	/** Sideways speed (cm/s) above which the car counts as sliding, and below which a slide counts as caught */
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Grip") float SlideStartSpeed = 60.f;
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Grip") float SlideEndSpeed = 15.f;
	/** How much a slide scrubs forward speed off (per second, at 90 degrees of slip) */
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Grip") float SlideScrub = 1.2f;
	/** Weather source: "Wetness" (0..1) is read from it. Unset = the game's MPC_NHWeather. */
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Grip") TObjectPtr<UMaterialParameterCollection> WeatherCollection = nullptr;

	/**
	 * One frame of grip. Speed is forward speed (cm/s) and may be reduced by a slide; YawRate is how fast the
	 * vehicle is being turned (rad/s, + to the right). Returns how far the vehicle slid sideways this frame
	 * (cm, + to the right). Call on the machine that moves the vehicle.
	 */
	float StepTraction(float DeltaSeconds, float& Speed, float YawRate, bool bHandbrake);
	/** A crash stops most of a slide */
	void DampSlide(float Keep) { SideSpeed *= FMath::Clamp(Keep, 0.f, 1.f); }

	UFUNCTION(BlueprintPure, Category = "Vehicle Dynamics") bool IsSliding() const { return bSliding; }
	/** Angle between where the car points and where it is going, degrees, + when sliding to the right */
	UFUNCTION(BlueprintPure, Category = "Vehicle Dynamics") float GetSlipAngle() const { return SlipAngle; }
	UFUNCTION(BlueprintPure, Category = "Vehicle Dynamics") float GetSideSpeed() const { return SideSpeed; }
	/** Grip available right now (cm/s²): surface, wetness, handbrake and sliding all counted */
	UFUNCTION(BlueprintPure, Category = "Vehicle Dynamics") float GetCurrentGrip() const { return CurrentGrip; }
	/** Grip of the surface under the wheels, as a fraction of dry asphalt, before wetness */
	UFUNCTION(BlueprintPure, Category = "Vehicle Dynamics") float GetSurfaceGrip() const { return GroundGrip; }

	// ---- Body: weight transfer and suspension -------------------------------------------------

	/** Degrees of roll per g of cornering, and of pitch per g of braking or acceleration */
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Body") float RollPerG = 3.2f;
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Body") float PitchPerG = 2.4f;
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Body") float MaxRoll = 9.f;
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Body") float MaxPitch = 7.f;
	/** How quickly the body answers (Hz) and how soon it settles (1 = no bounce, lower = floatier) */
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Body") float BodyFrequency = 1.8f;
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Body", meta = (ClampMin = "0.1", ClampMax = "2")) float BodyDamping = 0.55f;
	/** Suspension travel each way (cm) */
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Body") float SuspensionTravel = 12.f;
	/** How much of a sudden change in vertical speed (a kerb, a landing) is thrown into the springs */
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Body", meta = (ClampMin = "0", ClampMax = "1")) float BumpTransfer = 0.7f;
	/** How far the body follows the ground under the wheels (0 = stays flat, 1 = follows fully) */
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Body", meta = (ClampMin = "0", ClampMax = "1")) float GroundFollow = 0.6f;
	/** Trace each wheel to the ground (4 line traces a frame while moving). Off = the wheels just follow the body. */
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Body") bool bTraceWheels = true;

	/**
	 * One frame of body and wheel motion, from how the vehicle moved since the last call. ExtraRoll is added to
	 * the body's roll (a bike leaning into a corner). Call after the vehicle has been moved, on every machine.
	 */
	void StepChassis(float DeltaSeconds, float ExtraRoll = 0.f);

	UFUNCTION(BlueprintPure, Category = "Vehicle Dynamics") float GetBodyRoll() const { return Roll; }
	UFUNCTION(BlueprintPure, Category = "Vehicle Dynamics") float GetBodyPitch() const { return Pitch; }
	/** Body height off its rest position (cm, negative = compressed) */
	UFUNCTION(BlueprintPure, Category = "Vehicle Dynamics") float GetBodyHeave() const { return Heave; }
	/** Suspension compression of one wheel (cm, + = pushed up into the arch) */
	UFUNCTION(BlueprintPure, Category = "Vehicle Dynamics") float GetWheelCompression(int32 WheelIndex) const;
	UFUNCTION(BlueprintPure, Category = "Vehicle Dynamics") int32 GetWheelCount() const { return Wheels.Num(); }

	// ---- Simulation ---------------------------------------------------------------------------

	/** Longest step either simulation takes; a longer frame is split up */
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Simulation") float MaxSubstepSeconds = 1.f / 120.f;
	UPROPERTY(EditAnywhere, Category = "Vehicle Dynamics|Simulation") int32 MaxSubsteps = 8;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	struct FWheel
	{
		TWeakObjectPtr<USceneComponent> Hub;
		FVector Rest = FVector::ZeroVector; // hub position in body space when level
		float Ground = 0.f;                 // ground height under the wheel against the level plane, smoothed (cm)
		float Compression = 0.f;
	};

	int32 Substeps(float DeltaSeconds, float& OutStep) const;
	float ReadWetness();
	void TraceWheels(float DeltaSeconds);
	float GripOf(const UMaterialInterface* Material);
	void ApplyToBody(float ExtraRoll);

	UFUNCTION() void OnRep_Slide();

	UPROPERTY(Transient) TWeakObjectPtr<USceneComponent> BodyComponent;
	UPROPERTY(Transient) TObjectPtr<UMaterialParameterCollectionInstance> WeatherInstance = nullptr;
	UPROPERTY(ReplicatedUsing = OnRep_Slide) FNHVehicleSlideState RepSlide;

	TArray<FWheel> Wheels;
	TMap<TWeakObjectPtr<const UMaterialInterface>, float> GripCache;
	FVector BodyRest = FVector::ZeroVector;
	float GroundDropCm = 0.f;
	bool bIsBike = false;
	bool bWeatherLooked = false;

	// slide
	float SideSpeed = 0.f;
	float SlipAngle = 0.f;
	float CurrentGrip = 0.f;
	float GroundGrip = 1.f;
	bool bSliding = false;
	bool bHandbrakeOn = false;

	// body springs: value and rate
	float Roll = 0.f, RollRate = 0.f;
	float Pitch = 0.f, PitchRate = 0.f;
	float Heave = 0.f, HeaveRate = 0.f;

	// how the vehicle was last seen
	FVector LastLocation = FVector::ZeroVector;
	FVector LastVelocity = FVector::ZeroVector;
	bool bHasLast = false;
};
