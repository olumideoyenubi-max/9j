#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Gameplay/NHBody.h"
#include "NHCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class ANHVehicle;

/**
 * A playable person on foot: one of the story's leads (Tunde, Amaka and the characters unlocked later,
 * from characters.json) or, with no lead set, another player's own character. Blockout body with a
 * third-person camera until the MetaHumans arrive.
 *
 * Each lead with an ability has a meter that fills as you play:
 * - Tunde, Hustle Rush: faster on foot for a few seconds, or a boost in a vehicle
 * - Amaka, Unlock: kills the engine of the nearest vehicle within range for a few seconds
 * The server owns the lead, the meter and the ability; the owning client asks through its controller.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ANHCharacter();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Which character this is (an id in characters.json), or None for a player's own character */
	UFUNCTION(BlueprintPure, Category = "Naija") FName GetLeadId() const { return LeadId; }
	/** Server: makes this body that character */
	void SetLead(FName NewLead);
	/** Display name, for prompts and the HUD */
	FString DisplayName() const;
	/** hustle_rush, unlock or None */
	FName AbilityId() const;
	FString AbilityName() const;

	/** 0..100; the ability can be used when it's full */
	UFUNCTION(BlueprintPure, Category = "Naija") float GetAbilityMeter() const { return AbilityMeter; }
	UFUNCTION(BlueprintPure, Category = "Naija") bool IsAbilityActive() const { return bRushing; }
	/** Server: uses the ability. Vehicle is the one this character is driving, if any. Returns what happened, for the player. */
	FString UseAbility(ANHVehicle* Vehicle);
	/** The vehicle Unlock would hit from here (any machine; the server decides for real) */
	ANHVehicle* UnlockTarget() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** The blockout body hangs from here, at the soles of the feet */
	UPROPERTY(VisibleAnywhere, Category = "Body")
	TObjectPtr<USceneComponent> BodyRoot;

	/** cm/s */
	UPROPERTY(EditAnywhere, Category = "Movement") float WalkSpeed = 350.f;
	UPROPERTY(EditAnywhere, Category = "Movement") float SprintSpeed = 650.f;
	/** Gamepad look rate, degrees per second at full stick (before the engine's legacy input scale) */
	UPROPERTY(EditAnywhere, Category = "Camera") float StickLookRate = 70.f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Lead) FName LeadId;
	UPROPERTY(Replicated) float AbilityMeter = 0.f;
	UPROPERTY(ReplicatedUsing = OnRep_Speed) bool bRushing = false;
	UPROPERTY(ReplicatedUsing = OnRep_Speed) bool bSprinting = false;
	float AbilityLeft = 0.f;
	bool bBodyBuilt = false;
	float WalkPhase = 0.f;
	UPROPERTY() TObjectPtr<USceneComponent> HipL;
	UPROPERTY() TObjectPtr<USceneComponent> HipR;
	UPROPERTY() TObjectPtr<USceneComponent> ShoulderL;
	UPROPERTY() TObjectPtr<USceneComponent> ShoulderR;

	UFUNCTION() void OnRep_Lead();
	UFUNCTION() void OnRep_Speed();
	UFUNCTION(Server, Reliable) void ServerSetSprint(bool bOn);
	void BuildBody();
	void ApplySpeed();

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnLookStick(const FInputActionValue& Value);
	void OnSprintStart();
	void OnSprintStop();
};
