#include "Player/NHCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/NHInputSet.h"
#include "Player/NHPlayerController.h"
#include "NaijaHustleGame.h"

ANHCharacter::ANHCharacter()
{
	PrimaryActorTick.bCanEverTick = true; // the camera eases between walking and sprinting

	// Visible capsule until the MetaHuman arrives in step 3 (in your own project, swap the pawn class in NHGameMode)
	GetCapsuleComponent()->InitCapsuleSize(42.f, 92.f);
	GetCapsuleComponent()->SetHiddenInGame(false);

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
