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
	/**
	 * For a body that comes with its own animations instead of an animation Blueprint: the asset path they share,
	 * up to the clip name (Idle, Walk, Run, Sprint, Jump). UNHClipAnimInstance plays them and AnimClass is not used.
	 */
	UPROPERTY(EditAnywhere, Category = "Skin") FString Clips;
	/** The body is modelled with its shoes on, so it does not wear ShoeMesh */
	UPROPERTY(EditAnywhere, Category = "Skin") bool bShod = false;
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
	/** R: keeps running without the key held, until pressed again or you stand still */
	void ToggleRun();
	bool RunLocked() const { return bRunLocked; }
	/** A forward roll the way you are moving (or facing): quick, low, and it carries you about four metres */
	void Roll();
	bool IsRolling() const { return RollLeft > 0.f; }
	/** Space: up onto a ledge, wall top or vehicle in front, if there is one within reach (up to 2.4 m); otherwise a jump */
	virtual void Jump() override;
	bool IsClimbing() const { return ClimbTime > 0.f; }

	/** True when a body mesh was found and put on; false when the player is still the capsule */
	UFUNCTION(BlueprintPure, Category = "Body") bool HasBody() const { return bHasBody; }

	/** The bodies the player can wear, best first. The Lagos Runner outfits come from Scripts/import_lagos_runner.py, other characters from Scripts/import_player_gltf.py; the last is the Third Person template's mannequin. */
	UPROPERTY(EditAnywhere, Category = "Body") TArray<FNHPlayerSkin> Skins;

	/** Puts a skin on by id, and with bRemember saves it as the player's choice. False if there is no such skin or the project does not have its assets. */
	bool WearSkin(FName Id, bool bRemember = false);
	/** Puts on the next skin the project has (Step -1: the one before), going round the list; returns the name of what is now worn */
	FString WearNextSkin(int32 Step = 1);
	FName GetSkin() const { return CurrentSkin; }
	/** The worn skin's name as shown to the player ("" for the capsule) */
	FString SkinName() const;
	/** Which way a body faces in its own space, from where its feet point in the reference pose (degrees; 90 if unknown) */
	static float FacingYawOf(const USkeletalMesh* Mesh);
	/** A hand torch: a beam ahead of the body, for the night */
	void ToggleTorch();
	bool TorchOn() const;

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
	UPROPERTY() TObjectPtr<class USpotLightComponent> Torch;
	bool bSprinting = false;
	bool bRunLocked = false;
	float StillFor = 0.f;
	/** The roll: seconds left of it, the way it goes, and when the next is allowed */
	float RollLeft = 0.f, RollWait = 0.f;
	FVector RollDir = FVector::ForwardVector;
	/** The climb: from, up over the edge, to standing on top */
	float ClimbTime = 0.f, ClimbLength = 1.f;
	FVector ClimbFrom = FVector::ZeroVector, ClimbTo = FVector::ZeroVector;
	bool TryClimb();
	/** The body's place and turn on the capsule when standing (set when a skin is put on) */
	FVector MeshHome = FVector(0.f, 0.f, -92.f);
	FRotator MeshTurn = FRotator(0.f, -90.f, 0.f);
	/** 0 walking .. 1 sprinting, eased */
	float SprintAlpha = 0.f;
	float SwayTime = 0.f;
};
