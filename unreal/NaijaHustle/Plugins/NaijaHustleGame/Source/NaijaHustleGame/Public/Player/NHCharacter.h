#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "NHCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USkeletalMesh;
class UAnimInstance;

/**
 * The player on foot, with a low, close over-the-shoulder camera. Sprinting pulls the camera in, widens the view
 * and adds a slight handheld sway; it eases back when you walk.
 *
 * The body is whatever BodyMesh and BodyAnimClass point at. They default to the Unreal Third Person template's
 * mannequin as a stand-in until the real character is made; in a project without that content the player is the
 * visible capsule, as before.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ANHCharacter();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Hold to run (the Sprint input calls this) */
	void SetSprinting(bool bSprint);

	/** True when a body mesh was found and put on; false when the player is still the capsule */
	UFUNCTION(BlueprintPure, Category = "Body") bool HasBody() const { return bHasBody; }

	/** The body and its animation Blueprint. Point these at the real character when it exists. */
	UPROPERTY(EditAnywhere, Category = "Body") TSoftObjectPtr<USkeletalMesh> BodyMesh;
	UPROPERTY(EditAnywhere, Category = "Body") TSoftClassPtr<UAnimInstance> BodyAnimClass;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** cm/s */
	UPROPERTY(EditAnywhere, Category = "Movement") float WalkSpeed = 350.f;
	UPROPERTY(EditAnywhere, Category = "Movement") float SprintSpeed = 650.f;
	/** Gamepad look rate, degrees per second at full stick (before the engine's legacy input scale) */
	UPROPERTY(EditAnywhere, Category = "Camera") float StickLookRate = 70.f;

	/** Camera arm length (cm) and field of view (degrees), walking and at a full sprint */
	UPROPERTY(EditAnywhere, Category = "Camera") float WalkArmLength = 280.f;
	UPROPERTY(EditAnywhere, Category = "Camera") float SprintArmLength = 220.f;
	UPROPERTY(EditAnywhere, Category = "Camera") float WalkFOV = 80.f;
	UPROPERTY(EditAnywhere, Category = "Camera") float SprintFOV = 90.f;
	/** Handheld sway at a full sprint: 1 is slight, 0 turns it off */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "3")) float SprintSway = 1.f;

private:
	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnLookStick(const FInputActionValue& Value);
	void OnSprintStart();
	void OnSprintStop();

	bool bHasBody = false;
	bool bSprinting = false;
	/** 0 walking .. 1 sprinting, eased */
	float SprintAlpha = 0.f;
	float SwayTime = 0.f;
};
