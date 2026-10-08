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

/** One body the player can wear */
USTRUCT()
struct FNHPlayerSkin
{
	GENERATED_BODY()

	/** Short name used by NHSkin and the saved choice */
	UPROPERTY(EditAnywhere, Category = "Skin") FName Id;
	UPROPERTY(EditAnywhere, Category = "Skin") FString Name;
	UPROPERTY(EditAnywhere, Category = "Skin") TSoftObjectPtr<USkeletalMesh> Mesh;
	UPROPERTY(EditAnywhere, Category = "Skin") TSoftClassPtr<UAnimInstance> AnimClass;
	/** How tall it should stand, cm: it is scaled to this from its own height. 0 leaves it as made. */
	UPROPERTY(EditAnywhere, Category = "Skin") float Height = 0.f;
};

/**
 * The player on foot, with a low, close over-the-shoulder camera. Sprinting pulls the camera in, widens the view
 * and adds a slight handheld sway; it eases back when you walk.
 *
 * The body is one of Skins: the saved choice if the project has it, otherwise the first one it does have. In a
 * project with none of them the player is the visible capsule. Change it in play with the console command NHSkin.
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

	/** The bodies the player can wear, best first. Characters come from Scripts/import_player_gltf.py; the last is the Third Person template's mannequin. */
	UPROPERTY(EditAnywhere, Category = "Body") TArray<FNHPlayerSkin> Skins;

	/** Puts a skin on by id. False if there is no such skin or the project does not have its assets. */
	bool WearSkin(FName Id);
	/** Puts on the next skin the project has, going round the list; returns the name of what is now worn */
	FString WearNextSkin();
	FName GetSkin() const { return CurrentSkin; }

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
	static bool SkinAvailable(const FNHPlayerSkin& Skin);
	/** Which way the body faces in its own space, from where its feet point in the reference pose (degrees; 90 if unknown) */
	float BodyFacingYaw() const;

	bool bHasBody = false;
	FName CurrentSkin;
	UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> ShoeParts;
	bool bSprinting = false;
	/** 0 walking .. 1 sprinting, eased */
	float SprintAlpha = 0.f;
	float SwayTime = 0.f;
};
