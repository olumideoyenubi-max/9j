#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "NHCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USkeletalMesh;
class UAnimInstance;
class UStaticMesh;

/**
 * The player on foot, with a low, close over-the-shoulder camera. Sprinting pulls the camera in, widens the view
 * and adds a slight handheld sway; it eases back when you walk.
 *
 * The body is CharacterMesh with CharacterAnimClass if the project has them (a character brought in by
 * Scripts/import_player_gltf.py). Otherwise it is BodyMesh and BodyAnimClass, which default to the Unreal Third
 * Person template's mannequin. In a project with neither, the player is the visible capsule.
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

	/** The player's own character and its animation Blueprint, used when the project has them */
	UPROPERTY(EditAnywhere, Category = "Body") TSoftObjectPtr<USkeletalMesh> CharacterMesh;
	UPROPERTY(EditAnywhere, Category = "Body") TSoftClassPtr<UAnimInstance> CharacterAnimClass;
	/** How tall CharacterMesh should stand, cm: it is scaled to this from its own height */
	UPROPERTY(EditAnywhere, Category = "Body") float CharacterHeight = 180.f;
	/** The stand-in body and its animation Blueprint */
	UPROPERTY(EditAnywhere, Category = "Body") TSoftObjectPtr<USkeletalMesh> BodyMesh;
	UPROPERTY(EditAnywhere, Category = "Body") TSoftClassPtr<UAnimInstance> BodyAnimClass;

	/**
	 * One shoe, as an unrigged static mesh lying flat with its toe toward +X. It is worn on both feet (mirrored for
	 * the left) over the body's own feet, each fixed to its foot bone. Nothing is worn if the mesh is not in the project.
	 */
	UPROPERTY(EditAnywhere, Category = "Body|Wardrobe") TSoftObjectPtr<UStaticMesh> ShoeMesh;
	/** Turns the shoe about its heel (degrees), for a model that was not saved pointing straight along +X */
	UPROPERTY(EditAnywhere, Category = "Body|Wardrobe") float ShoeYaw = -9.f;
	UPROPERTY(EditAnywhere, Category = "Body|Wardrobe") float ShoeScale = 1.f;

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
	void PutOnShoes();
	/** Which way the body faces in its own space, from where its feet point in the reference pose (degrees; 90 if unknown) */
	float BodyFacingYaw() const;

	bool bHasBody = false;
	bool bSprinting = false;
	/** 0 walking .. 1 sprinting, eased */
	float SprintAlpha = 0.f;
	float SwayTime = 0.f;
};
