#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "Core/NHGameData.h"
#include "NHVehicle.generated.h"

class UBoxComponent;
class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UNHVehicleMaterialComponent;
class UNHVehicleDynamicsComponent;

/**
 * A drivable vehicle with the browser demo's arcade handling (same top speed, acceleration and turn
 * rate per type): danfo, okada, keke, cars. Moves by sweeping its box through the world, so it stops on
 * buildings, rides up kerbs and loses health in crashes. The body is the type's real model when one is
 * assigned (Scripts/assign_vehicle_meshes.py), or else a blockout built from the type's size.
 */
/** What stands between the player and driving a vehicle off */
enum class ENHLock : uint8
{
	Open,      // get in and go (the mission's vehicles, your own)
	Unlocked,  // the door opens, but there are no keys: hotwire it
	Locked,    // break the window, then hotwire it
	KeysIn     // somebody left it running
};

UCLASS()
class NAIJAHUSTLEGAME_API ANHVehicle : public APawn
{
	GENERATED_BODY()

public:
	ANHVehicle();

	/** danfo, okada, keke, sedan, suv, truck (keys of "vehicles" in naija_rules.json) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FName VehicleType = TEXT("danfo");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FLinearColor Paint = FLinearColor(0.9f, 0.55f, 0.f);
	/** Destination board text on a danfo */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FString Board = TEXT("YABA");

	UPROPERTY(BlueprintReadOnly, Category = "Vehicle") float Health = 100.f;
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle") float MaxHealth = 100.f;
	/** Forward speed, cm/s (negative when reversing) */
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle") float Speed = 0.f;

	UFUNCTION(BlueprintPure, Category = "Vehicle") bool IsWrecked() const { return Health <= 0.f; }
	const FNHVehicleSpec& GetSpec() const { return Spec; }
	UFUNCTION(BlueprintPure, Category = "Vehicle") FString DisplayName() const { return Spec.Name; }
	/** Half the length plus a bit: how close you need to be to get in */
	float EnterRadius() const { return Spec.Length * 0.5f + 220.f; }
	/** A free spot beside the driver's door to step out to */
	FVector ExitPoint() const;
	void SetOccupied(bool bOn);
	/** Puts it back on its wheels with full health (mission retries) */
	void Repair();
	/** Wet paint, clear coat and crash marks, on bodies whose materials support them (see NHVehicleMaterialComponent.h) */
	UNHVehicleMaterialComponent* GetPaintFx() const { return PaintFx; }
	/** Grip and slides, body roll and pitch, suspension (see NHVehicleDynamicsComponent.h) */
	UNHVehicleDynamicsComponent* GetDynamics() const { return Dynamics; }
	/** Held: it stands still whatever the driver presses (Baba Driver counting the money) */
	void SetHeld(bool bOn) { bHeld = bOn; if (bOn) { Speed = 0.f; } }
	bool IsHeld() const { return bHeld; }
	/** Is there water at that place for a boat to float on: the small city's lagoon cells, the real city's water surface */
	bool Afloat(const FVector& At) const;
	/** Set by ANHLeads while Hustle Rush lasts: the engine's pull and the top speed, times this (1: as made) */
	float PullBoost = 1.f, TopBoost = 1.f;
	/**
	 * Traffic: the vehicle does not drive itself; ANHTraffic carries it along the road with TrafficMove. Getting in
	 * still works, and it then drives as usual for as long as someone is at the wheel.
	 */
	void SetTraffic(bool bOn) { bTraffic = bOn; if (!bOn) { Speed = 0.f; } }
	bool IsTraffic() const { return bTraffic; }
	/** Puts it at a place on the road, on the ground, facing Yaw, with its wheels turning for InSpeed (cm/s) */
	void TrafficMove(const FVector2D& At, float Yaw, float InSpeed, float DeltaSeconds);
	// ---- stealing (see ANHCarTheft)
	ENHLock Lock = ENHLock::Open;
	/** Taken from somebody: flagged for HotLeft seconds more, until resprayed */
	bool bStolen = false;
	float HotLeft = 0.f;
	/** A tracker the Task Force follows, on luxury cars, until a mechanic takes it out */
	bool bTracker = false;
	/** The player's own: resprayed with new plates. The car keys lock and unlock it. */
	bool bOwned = false;
	bool bWindowBroken = false;
	/** Sets the alarm off: lights flashing and the siren's text for that long (0 stops it) */
	void SetAlarm(float Seconds) { AlarmLeft = Seconds; }
	bool AlarmOn() const { return AlarmLeft > 0.f; }
	/** Somebody other than the player is at the wheel */
	bool HasNpcDriver() const { return bNpcDriver; }
	/** What it would fetch whole, naira */
	int32 Value() const { return ValueOf(VehicleType); }
	static int32 ValueOf(FName Type);
	/** Its number in the player's garage (UNHHustleSubsystem::Cars), 0 if it is not one of the kept cars */
	int32 GarageSerial = 0;

	/** Seats somebody else at the wheel: a body on a skeleton with the mannequin's bone names, as made (traffic's drivers) */
	void SetNpcDriver(class USkeletalMesh* Mesh);
	/** The last vehicle the player drove: traffic does not clear it away, and the car keys find it */
	bool bPlayerOwned = false;
	/** World Partition: on while the player drives it, loading the city within Radius cm of the vehicle */
	void SetStreamingRadius(bool bOn, float Radius);
	/** How full the tank is, 0..1. It goes down with the distance driven; empty, the engine gives nothing. */
	float Fuel = 1.f;
	float TankLitres() const { return Spec.bBike ? 12.f : Spec.Length > 560.f ? 90.f : 60.f; }
	/** How far it has been driven since it was made, cm */
	double Odometer = 0.0;
	/** For the dashboard: the gear (-1 reverse, 0 neutral, 1 up) and the engine's revolutions a minute at this speed */
	void Readings(int32& OutGear, float& OutRpm) const;
	bool HandbrakeOn() const { return bHandbrake; }
	float TopSpeed() const { return Spec.MaxSpeed; }
	/** For the log: wheels, how they are turning, the lean, and which driving clip the rider is in */
	FString DescribeMotion() const;
	/** Sets the pedals and wheel directly, as the input bindings do (scripted driving) */
	void SetDriveInput(float InThrottle, float InBrake, float InSteer) { Throttle = InThrottle; BrakeIn = InBrake; Steer = InSteer; }

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Headlights and tail light on or off (K) */
	UFUNCTION(BlueprintCallable, Category = "Vehicle") void SetHeadlights(bool bOn);
	UFUNCTION(BlueprintPure, Category = "Vehicle") bool HeadlightsOn() const { return bHeadlights; }
	/** The view from the driver's eyes, over the dashboard (V), or the chase camera behind the vehicle */
	UFUNCTION(BlueprintCallable, Category = "Vehicle") void SetCabinView(bool bOn);
	UFUNCTION(BlueprintPure, Category = "Vehicle") bool CabinViewOn() const { return bCabinView; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UBoxComponent> Box;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<USceneComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<USpringArmComponent> Arm;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UNHVehicleMaterialComponent> PaintFx;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UNHVehicleDynamicsComponent> Dynamics;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<class UWorldPartitionStreamingSourceComponent> Streaming;

private:
	FNHVehicleSpec Spec;
	float Throttle = 0.f, BrakeIn = 0.f, Steer = 0.f;
	bool bHandbrake = false;
	bool bHeld = false;
	bool bTraffic = false;
	bool bNpcDriver = false;
	float AlarmLeft = 0.f, AlarmBeat = 0.f;
	bool bBuilt = false;
	float Clearance = 30.f, HalfHeight = 60.f;
	float WheelSpin = 0.f, Lean = 0.f, LookIdle = 0.f;
	FVector2D LookOffset = FVector2D::ZeroVector;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Wheels;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> DriverPieces;
	UPROPERTY() TObjectPtr<UTextRenderComponent> BoardText;
	/** The player's own body in the driving seat, made the first time someone gets in */
	UPROPERTY() TObjectPtr<class UPoseableMeshComponent> DriverBody;
	/**
	 * The same body moved by its driving clips, where it has them (ANHCharacter::ActionClip): hands on the wheel,
	 * carried round with the steering, and a look over the shoulder in reverse. It is shown in place of DriverBody,
	 * which keeps saying whether a driver should be seen.
	 */
	UPROPERTY() TObjectPtr<class USkeletalMeshComponent> DriverAnim;
	UPROPERTY() TArray<TObjectPtr<class UAnimSequence>> DriveClips;
	int32 DriveClipShown = -1;
	float SteerShown = 0.f, ReverseShown = 0.f;
	/** The steering as the wheel has got to, -1..1: it follows the key, not jumps to it */
	float SteerEased = 0.f;
	float DrySaid = -10.f;
	/** How fast it is falling, cm/s, while there is no ground under it */
	float FallSpeed = 0.f;
	void SetupDriverAnim(class USkeletalMesh* Mesh);
	void DriverAnimTick(float DeltaSeconds);
	/** Two headlights and a tail light, made the first time they are switched on */
	UPROPERTY() TArray<TObjectPtr<class ULocalLightComponent>> Lamps;
	/** The glowing lamp faces that go with them */
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> LampGlow;
	bool bHeadlights = false;
	bool bCabinView = false;
	/** Where the driver's hips are, relative to Body, and how tall the body is */
	FVector SeatAt = FVector::ZeroVector;
	float BodyHeight = 150.f;

	void BuildBody();
	/** Uses the type's real model if one is assigned and loads; sets its height */
	bool AddModel(float& OutHeight);
	void Drive(float DeltaSeconds);
	float GroundZ(const FVector& At) const;

	void OnThrottle(const FInputActionValue& V) { Throttle = V.Get<float>(); }
	void OnThrottleEnd() { Throttle = 0.f; }
	void OnBrake(const FInputActionValue& V) { BrakeIn = V.Get<float>(); }
	void OnBrakeEnd() { BrakeIn = 0.f; }
	void OnSteer(const FInputActionValue& V) { Steer = V.Get<float>(); }
	void OnSteerEnd() { Steer = 0.f; }
	void OnHandbrake() { bHandbrake = true; }
	void OnHandbrakeEnd() { bHandbrake = false; }
	void OnHorn();
	void OnHeadlights() { SetHeadlights(!bHeadlights); }
	void OnRadio();
	void OnRadioTrack();
	void OnCabinView() { SetCabinView(!bCabinView); }
	/** Sits the player's current body in the seat, posed for driving; false if the player has no body to show */
	bool SeatDriver();
	/** Sits a body in the seat, posed for driving. Scale is its size against how it was made; Facing the way it faces in its own space, degrees. */
	bool SeatBody(class USkeletalMesh* Mesh, float Scale, float Facing);
	void OnLook(const FInputActionValue& V);
};
