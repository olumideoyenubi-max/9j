#include "Player/NHCharacter.h"

#include "Animation/AnimInstance.h"
#include "AnimationRuntime.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/NHInputSet.h"
#include "Misc/PackageName.h"
#include "Player/NHPlayerController.h"
#include "NaijaHustleGame.h"

ANHCharacter::ANHCharacter()
{
	PrimaryActorTick.bCanEverTick = true; // the camera eases between walking and sprinting

	// The capsule shows until BeginPlay finds a body to put on
	GetCapsuleComponent()->InitCapsuleSize(42.f, 92.f);
	GetCapsuleComponent()->SetHiddenInGame(false);

	// Stand-in body: the Third Person template's mannequin, if the project has it
	BodyMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
	BodyAnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C")));
	ShoeMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Wardrobe/Trainers_LowTop/Untsssho00215ed/StaticMeshes/hash_CF7B2BF4_model_001.hash_CF7B2BF4_model_001")));
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -92.f), FRotator(0.f, -90.f, 0.f)); // feet on the ground, facing forward

	// The body turns toward where it's moving; the camera is free
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 540.f, 0.f);
	Move->MaxWalkSpeed = WalkSpeed;
	Move->JumpZVelocity = 420.f;
	Move->AirControl = 0.25f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = WalkArmLength;
	CameraBoom->SocketOffset = FVector(0.f, 50.f, 45.f); // low and close over the right shoulder
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetFieldOfView(WalkFOV);
}

void ANHCharacter::BeginPlay()
{
	Super::BeginPlay();

	// asking for a package that is not there logs a warning, so look first
	const auto Exists = [](const FSoftObjectPath& Path) { return Path.IsValid() && FPackageName::DoesPackageExist(Path.GetLongPackageName()); };
	USkeletalMesh* Mesh = Exists(BodyMesh.ToSoftObjectPath()) ? BodyMesh.LoadSynchronous() : nullptr;
	if (!Mesh)
	{
		UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: no body mesh at %s; the player stays a capsule"), *BodyMesh.ToString());
		return;
	}
	GetMesh()->SetSkeletalMesh(Mesh);
	if (UClass* Anim = Exists(BodyAnimClass.ToSoftObjectPath()) ? BodyAnimClass.LoadSynchronous() : nullptr)
	{
		GetMesh()->SetAnimInstanceClass(Anim);
	}
	GetCapsuleComponent()->SetHiddenInGame(true);
	bHasBody = true;
	PutOnShoes();
}

void ANHCharacter::PutOnShoes()
{
	const FSoftObjectPath Path = ShoeMesh.ToSoftObjectPath();
	UStaticMesh* Shoe = Path.IsValid() && FPackageName::DoesPackageExist(Path.GetLongPackageName()) ? ShoeMesh.LoadSynchronous() : nullptr;
	if (!Shoe)
	{
		return;
	}
	// Worked out in the body's reference pose, where it stands flat on z = 0 facing +Y, so it does not matter
	// which way a skeleton's foot bones happen to point.
	const FReferenceSkeleton& Skeleton = GetMesh()->GetSkeletalMeshAsset()->GetRefSkeleton();
	const FBox Box = Shoe->GetBoundingBox();
	const FVector Ankle(Box.Min.X + 0.25f * (Box.Max.X - Box.Min.X), 0.5f * (Box.Min.Y + Box.Max.Y), Box.Min.Z); // under the ankle, on the sole
	int32 Worn = 0;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FName Bone(Side == 0 ? TEXT("foot_r") : TEXT("foot_l"));
		const int32 Index = Skeleton.FindBoneIndex(Bone);
		if (Index == INDEX_NONE)
		{
			continue;
		}
		const FTransform Foot = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Index);
		const float Mirror = Side == 0 ? 1.f : -1.f;
		const FVector Scale(ShoeScale, ShoeScale * Mirror, ShoeScale);
		const FQuat Turn(FRotator(0.f, 90.f + ShoeYaw * Mirror, 0.f)); // toe to +Y
		const FVector Ground(Foot.GetLocation().X, Foot.GetLocation().Y, 0.f);
		const FTransform Placed(Turn, Ground - Turn.RotateVector(Ankle * Scale), Scale);

		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
		Part->SetStaticMesh(Shoe);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetupAttachment(GetMesh(), Bone);
		Part->SetRelativeTransform(Placed.GetRelativeTransform(Foot));
		Part->RegisterComponent();
		++Worn;
	}
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: the player is wearing %s on %d feet"), *Shoe->GetName(), Worn);
}

void ANHCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetController());
	if (!Input || !PC)
	{
		UE_LOG(LogNHGame, Warning, TEXT("NHCharacter: needs Enhanced Input and an NHPlayerController (check DefaultInput.ini and the game mode)."));
		return;
	}

	const UNHInputSet* Set = PC->GetInputSet();
	Input->BindAction(Set->Move, ETriggerEvent::Triggered, this, &ANHCharacter::OnMove);
	Input->BindAction(Set->Look, ETriggerEvent::Triggered, this, &ANHCharacter::OnLook);
	Input->BindAction(Set->LookStick, ETriggerEvent::Triggered, this, &ANHCharacter::OnLookStick);
	Input->BindAction(Set->Jump, ETriggerEvent::Started, this, &ACharacter::Jump);
	Input->BindAction(Set->Jump, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	Input->BindAction(Set->Sprint, ETriggerEvent::Started, this, &ANHCharacter::OnSprintStart);
	Input->BindAction(Set->Sprint, ETriggerEvent::Completed, this, &ANHCharacter::OnSprintStop);
	// F (get in a vehicle) and E (talk, call passengers) are bound by ANHPlayerController
}

void ANHCharacter::OnMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>(); // X = right, Y = forward
	if (!Controller) return;
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FRotationMatrix Basis(Yaw);
	AddMovementInput(Basis.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(Basis.GetUnitAxis(EAxis::Y), Axis.X);
}

void ANHCharacter::OnLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void ANHCharacter::OnLookStick(const FInputActionValue& Value)
{
	// A stick is a rate, not a delta: scale by frame time so turning speed doesn't depend on frame rate
	const FVector2D Axis = Value.Get<FVector2D>() * StickLookRate * GetWorld()->GetDeltaSeconds();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void ANHCharacter::OnSprintStart() { SetSprinting(true); }
void ANHCharacter::OnSprintStop() { SetSprinting(false); }

void ANHCharacter::SetSprinting(bool bSprint)
{
	bSprinting = bSprint;
	GetCharacterMovement()->MaxWalkSpeed = bSprint ? SprintSpeed : WalkSpeed;
}

void ANHCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// the camera follows what the body is doing: holding Sprint while standing still changes nothing
	const bool bRunning = bSprinting && GetVelocity().SizeSquared2D() > FMath::Square(WalkSpeed * 1.1f);
	SprintAlpha = FMath::FInterpTo(SprintAlpha, bRunning ? 1.f : 0.f, DeltaSeconds, bRunning ? 4.f : 2.5f);
	const float A = FMath::InterpEaseInOut(0.f, 1.f, SprintAlpha, 2.f);
	CameraBoom->TargetArmLength = FMath::Lerp(WalkArmLength, SprintArmLength, A);
	FollowCamera->SetFieldOfView(FMath::Lerp(WalkFOV, SprintFOV, A));

	// handheld sway: a small bob and roll in step with the run, fading out with the sprint
	SwayTime += DeltaSeconds;
	const float K = A * SprintSway, Step = SwayTime * 11.f;
	FollowCamera->SetRelativeLocationAndRotation(
		FVector(0.f, FMath::Sin(Step * 0.5f) * 1.2f * K, FMath::Sin(Step) * 1.6f * K),
		FRotator(FMath::Sin(Step + 0.6f) * 0.35f * K, FMath::Sin(Step * 0.5f) * 0.25f * K, FMath::Sin(Step * 0.5f + 1.3f) * 0.5f * K));
}
