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

/**
 * A drivable vehicle with the browser demo's arcade handling (same top speed, acceleration and turn
 * rate per type): danfo, okada, keke, cars. Moves by sweeping its box through the world, so it stops on
 * buildings, rides up kerbs and loses health in crashes. The body is a blockout built from the type's
 * size; step 4 swaps in Chaos Vehicles and real meshes.
 */
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle") FString Board = TEXT("BALO");

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
	void Repair() { Health = MaxHealth; Speed = 0.f; EngineOffTime = BoostTime = 0.f; }
	/** Amaka's Unlock: the engine cuts out for a few seconds (no throttle; brakes and steering still work) */
	void KillEngine(float Seconds) { EngineOffTime = FMath::Max(EngineOffTime, Seconds); }
	UFUNCTION(BlueprintPure, Category = "Vehicle") bool IsEngineKilled() const { return EngineOffTime > 0.f; }
	/** Tunde's Hustle Rush at the wheel: more top speed and acceleration for a few seconds */
	void Boost(float Seconds, float Multiplier) { BoostTime = Seconds; BoostMult = Multiplier; }

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UBoxComponent> Box;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<USceneComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<USpringArmComponent> Arm;
	UPROPERTY(VisibleAnywhere, Category = "Vehicle") TObjectPtr<UCameraComponent> Camera;

private:
	FNHVehicleSpec Spec;
	float Throttle = 0.f, BrakeIn = 0.f, Steer = 0.f;
	bool bHandbrake = false;
	float EngineOffTime = 0.f, BoostTime = 0.f, BoostMult = 1.f;
	bool bBuilt = false;
	float Clearance = 30.f, HalfHeight = 60.f;
	float WheelSpin = 0.f, Lean = 0.f, LookIdle = 0.f;
	FVector2D LookOffset = FVector2D::ZeroVector;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Wheels;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> DriverPieces;
	UPROPERTY() TObjectPtr<UTextRenderComponent> BoardText;

	void BuildBody();
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
	void OnLook(const FInputActionValue& V);
};
