#include "Player/NHCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/NHGameData.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/NHInputSet.h"
#include "Net/UnrealNetwork.h"
#include "Player/NHPlayerController.h"
#include "Vehicles/NHVehicle.h"
#include "NaijaHustleGame.h"

namespace NHLead
{
	const FName HustleRush(TEXT("hustle_rush"));
	const FName Unlock(TEXT("unlock"));
	const float CapsuleHalfHeight = 92.f;
}

ANHCharacter::ANHCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// the blockout body is the visible part; the capsule only collides
	GetCapsuleComponent()->InitCapsuleSize(42.f, NHLead::CapsuleHalfHeight);
	GetCapsuleComponent()->SetHiddenInGame(true);
	BodyRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BodyRoot"));
	BodyRoot->SetupAttachment(RootComponent);
	BodyRoot->SetRelativeLocation(FVector(0.f, 0.f, -NHLead::CapsuleHalfHeight));

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
	// a lead you switched away from keeps standing (and falling) with nobody controlling it
	Move->bRunPhysicsWithNoController = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 380.f;
	CameraBoom->SocketOffset = FVector(0.f, 45.f, 60.f); // slightly over the right shoulder
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void ANHCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANHCharacter, LeadId);
	DOREPLIFETIME_CONDITION(ANHCharacter, AbilityMeter, COND_OwnerOnly);
	DOREPLIFETIME(ANHCharacter, bRushing);
	DOREPLIFETIME(ANHCharacter, bSprinting);
}

void ANHCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (!bBodyBuilt)
	{
		BuildBody();
	}
}

// ---------------------------------------------------------------------------------------------------- who this is
void ANHCharacter::SetLead(FName NewLead)
{
	if (!HasAuthority())
	{
		return;
	}
	LeadId = NewLead;
	AbilityMeter = 0.f;
	OnRep_Lead(); // the server gets no OnRep of its own
}

void ANHCharacter::OnRep_Lead()
{
	BuildBody();
}

FString ANHCharacter::DisplayName() const
{
	const UNHGameData* Data = UNHGameData::Get(this);
	const FNHCharacterInfo* Info = Data ? Data->FindCharacter(LeadId) : nullptr;
	if (Info)
	{
		return Info->Name;
	}
	const APlayerState* PS = GetPlayerState();
	return PS ? PS->GetPlayerName() : TEXT("You");
}

FName ANHCharacter::AbilityId() const
{
	const UNHGameData* Data = UNHGameData::Get(this);
	const FNHCharacterInfo* Info = Data ? Data->FindCharacter(LeadId) : nullptr;
	return Info ? Info->Ability : NAME_None;
}

FString ANHCharacter::AbilityName() const
{
	const FName A = AbilityId();
	return A == NHLead::HustleRush ? TEXT("Hustle Rush") : A == NHLead::Unlock ? TEXT("Unlock") : FString();
}

void ANHCharacter::BuildBody()
{
	// throw away the old body (the lead changed)
	TArray<USceneComponent*> Old;
	BodyRoot->GetChildrenComponents(true, Old);
	for (USceneComponent* C : Old)
	{
		if (C)
		{
			C->DestroyComponent();
		}
	}

	FNHBodyLook Look;
	const UNHGameData* Data = UNHGameData::Get(this);
	if (const FNHCharacterInfo* Info = Data ? Data->FindCharacter(LeadId) : nullptr)
	{
		Look.Top = Info->Top;
		Look.Bottom = Info->Bottom;
		Look.Skin = Info->Skin;
		Look.Height = Info->Height;
		Look.bHeadTie = Info->bHeadTie;
	}
	else
	{
		// a player's own character: a look picked from their player id until the character creator exists
		const APlayerState* PS = GetPlayerState();
		const int32 Seed = PS ? PS->GetPlayerId() : GetUniqueID();
		Look = FNHBodyLook::FromSeed(Seed, FLinearColor(0.05f, 0.2f, 0.7f), false, 1.f);
	}
	const FNHBody Body = FNHBody::Build(this, BodyRoot, Look);
	HipL = Body.HipL;
	HipR = Body.HipR;
	ShoulderL = Body.ShoulderL;
	ShoulderR = Body.ShoulderR;
	bBodyBuilt = true;
}

// ---------------------------------------------------------------------------------------------------- every frame
void ANHCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Speed2D = GetVelocity().Size2D();

	// legs and arms swing with the walking speed (on every machine)
	float Swing = 0.f;
	if (Speed2D > 20.f && !GetCharacterMovement()->IsFalling())
	{
		WalkPhase += DeltaSeconds * Speed2D / 30.f;
		Swing = FMath::Sin(WalkPhase) * FMath::Clamp(Speed2D / 12.f, 10.f, 38.f);
	}
	FNHBody Limbs;
	Limbs.HipL = HipL;
	Limbs.HipR = HipR;
	Limbs.ShoulderL = ShoulderL;
	Limbs.ShoulderR = ShoulderR;
	Limbs.Pose(Swing);

	if (!HasAuthority())
	{
		return;
	}
	const UNHGameData* Data = UNHGameData::Get(this);
	if (!Data || AbilityId().IsNone())
	{
		return;
	}
	const FNHAbilityRules& R = Data->Abilities;
	if (bRushing)
	{
		AbilityLeft -= DeltaSeconds;
		if (AbilityLeft <= 0.f)
		{
			bRushing = false;
			ApplySpeed();
		}
	}
	// the meter fills while this lead is being played, faster on the move
	else if (Controller || IsHidden()) // hidden = driving
	{
		const bool bMoving = Speed2D > 100.f || IsHidden();
		AbilityMeter = FMath::Min(100.f, AbilityMeter + DeltaSeconds * (bMoving ? R.MeterPerSecondMoving : R.MeterPerSecond));
	}
}

// ---------------------------------------------------------------------------------------------------- abilities
ANHVehicle* ANHCharacter::UnlockTarget() const
{
	const UNHGameData* Data = UNHGameData::Get(this);
	if (!Data || AbilityId() != NHLead::Unlock)
	{
		return nullptr;
	}
	// Amaka may be inside a vehicle (attached to it): never target her own ride
	const AActor* Ride = GetAttachParentActor();
	ANHVehicle* Best = nullptr;
	float BestD = Data->Abilities.UnlockRadius;
	for (TActorIterator<ANHVehicle> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), GetActorLocation());
		if (*It != Ride && !It->IsWrecked() && D < BestD)
		{
			Best = *It;
			BestD = D;
		}
	}
	return Best;
}

FString ANHCharacter::UseAbility(ANHVehicle* Vehicle)
{
	const UNHGameData* Data = UNHGameData::Get(this);
	const FName A = AbilityId();
	if (!HasAuthority() || !Data || A.IsNone())
	{
		return TEXT("No special ability.");
	}
	if (bRushing)
	{
		return FString();
	}
	if (AbilityMeter < 100.f)
	{
		return FString::Printf(TEXT("%s isn't ready yet (%d%%)."), *AbilityName(), FMath::FloorToInt(AbilityMeter));
	}
	const FNHAbilityRules& R = Data->Abilities;
	if (A == NHLead::HustleRush)
	{
		AbilityMeter = 0.f;
		bRushing = true;
		AbilityLeft = R.RushSeconds;
		ApplySpeed();
		if (Vehicle)
		{
			Vehicle->Boost(R.RushSeconds, R.RushVehicleBoost);
		}
		return TEXT("HUSTLE RUSH!");
	}
	if (A == NHLead::Unlock)
	{
		ANHVehicle* Target = UnlockTarget();
		if (!Target)
		{
			return TEXT("Nothing to unlock nearby.");
		}
		AbilityMeter = 0.f;
		Target->KillEngine(R.UnlockSeconds);
		return FString::Printf(TEXT("UNLOCK: %s engine cut!"), *Target->DisplayName());
	}
	return FString();
}

void ANHCharacter::ApplySpeed()
{
	const UNHGameData* Data = UNHGameData::Get(this);
	const float Mult = (bRushing && Data) ? Data->Abilities.RushSpeed : 1.f;
	GetCharacterMovement()->MaxWalkSpeed = (bSprinting ? SprintSpeed : WalkSpeed) * Mult;
}

void ANHCharacter::OnRep_Speed()
{
	ApplySpeed();
}

// ---------------------------------------------------------------------------------------------------- input
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
	// F (vehicles), E (talk, call passengers), Alt (switch lead) and Caps Lock (ability) are bound by ANHPlayerController
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

// sprinting changes the speed here at once (so movement prediction agrees) and on the server
void ANHCharacter::OnSprintStart()
{
	bSprinting = true;
	ApplySpeed();
	if (!HasAuthority())
	{
		ServerSetSprint(true);
	}
}

void ANHCharacter::OnSprintStop()
{
	bSprinting = false;
	ApplySpeed();
	if (!HasAuthority())
	{
		ServerSetSprint(false);
	}
}

void ANHCharacter::ServerSetSprint_Implementation(bool bOn)
{
	bSprinting = bOn;
	ApplySpeed();
}
