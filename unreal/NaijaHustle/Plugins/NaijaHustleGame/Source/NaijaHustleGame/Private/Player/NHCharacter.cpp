#include "Player/NHCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/NHInputSet.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Player/NHClipAnimInstance.h"
#include "Player/NHPlayerController.h"
#include "NaijaHustleGame.h"

ANHCharacter::ANHCharacter()
{
	PrimaryActorTick.bCanEverTick = true; // the camera eases between walking and sprinting

	// The capsule shows until BeginPlay finds a body to put on
	GetCapsuleComponent()->InitCapsuleSize(42.f, 92.f);
	GetCapsuleComponent()->SetHiddenInGame(false);

	const auto Skin = [this](const TCHAR* Id, const TCHAR* Name, const TCHAR* Mesh, const TCHAR* Anim, float Height)
	{
		FNHPlayerSkin S;
		S.Id = Id;
		S.Name = Name;
		S.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(Mesh));
		S.AnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(Anim));
		S.Height = Height;
		Skins.Add(S);
	};
	// the Lagos Runner made in Blender: three outfits on one skeleton, moved by his own clips
	const auto Runner = [this](const TCHAR* Id, const TCHAR* Name, const TCHAR* Mesh)
	{
		FNHPlayerSkin S;
		S.Id = Id;
		S.Name = Name;
		S.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(Mesh));
		S.Clips = TEXT("/Game/Characters/Player/Runner/Anims/Runner_");
		S.bShod = true;
		Skins.Add(S);
	};
	Skin(TEXT("naija"), TEXT("Naija man"), TEXT("/Game/Characters/Player/Naija/Naija.Naija"), TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed_Naija.ABP_Unarmed_Naija_C"), 0.f);
	Skin(TEXT("hustler"), TEXT("Young hustler"), TEXT("/Game/Characters/Player/Hustler/scene/SkeletalMeshes/Hustler.Hustler"), TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed_Hustler.ABP_Unarmed_Hustler_C"), 180.f);
	Runner(TEXT("runner"), TEXT("Lagos runner"), TEXT("/Game/Characters/Player/Runner/Runner.Runner"));
	Runner(TEXT("dispatch"), TEXT("Lagos runner, dispatch rider"), TEXT("/Game/Characters/Player/Runner/Runner_Dispatch.Runner_Dispatch"));
	Runner(TEXT("suit"), TEXT("Lagos runner, suit"), TEXT("/Game/Characters/Player/Runner/Runner_Suit.Runner_Suit"));
	Skin(TEXT("mannequin"), TEXT("Mannequin"), TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"), TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"), 0.f);
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

	FString Saved;
	GConfig->GetString(TEXT("NaijaHustle"), TEXT("PlayerSkin"), Saved, GGameUserSettingsIni);
	if (!Saved.IsEmpty() && WearSkin(FName(*Saved)))
	{
		return;
	}
	for (const FNHPlayerSkin& Skin : Skins)
	{
		if (WearSkin(Skin.Id))
		{
			return;
		}
	}
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: the project has none of the player's %d skins; the player stays a capsule"), Skins.Num());
}

bool ANHCharacter::SkinAvailable(const FNHPlayerSkin& Skin)
{
	// asking for a package that is not there logs a warning, so look first
	const auto Exists = [](const FSoftObjectPath& Path) { return Path.IsValid() && FPackageName::DoesPackageExist(Path.GetLongPackageName()); };
	const FSoftObjectPath Anim = Skin.Clips.IsEmpty() ? Skin.AnimClass.ToSoftObjectPath() : FSoftObjectPath(Skin.Clips + TEXT("Idle"));
	return Exists(Skin.Mesh.ToSoftObjectPath()) && Exists(Anim);
}

bool ANHCharacter::WearSkin(FName Id, bool bRemember)
{
	const FNHPlayerSkin* Skin = Skins.FindByPredicate([Id](const FNHPlayerSkin& S) { return S.Id == Id; });
	USkeletalMesh* Mesh = Skin && SkinAvailable(*Skin) ? Skin->Mesh.LoadSynchronous() : nullptr;
	const bool bClips = Skin && !Skin->Clips.IsEmpty();
	UClass* Anim = !Mesh ? nullptr : bClips ? UNHClipAnimInstance::StaticClass() : Skin->AnimClass.LoadSynchronous();
	if (!Mesh || !Anim)
	{
		return false;
	}
	GetMesh()->SetAnimInstanceClass(nullptr);
	GetMesh()->SetSkeletalMesh(Mesh);
	GetMesh()->SetAnimInstanceClass(Anim);
	if (UNHClipAnimInstance* Player = bClips ? Cast<UNHClipAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr)
	{
		const auto Clip = [Skin](const TCHAR* Name) -> UAnimSequence*
		{
			const FString Path = Skin->Clips + Name;
			return FPackageName::DoesPackageExist(Path) ? LoadObject<UAnimSequence>(nullptr, *Path) : nullptr;
		};
		Player->SetClips(Clip(TEXT("Idle")), Clip(TEXT("Walk")), Clip(TEXT("Run")), Clip(TEXT("Sprint")), Clip(TEXT("Jump")));
		Player->WalkAt = 0.43f * WalkSpeed;
		Player->RunAt = WalkSpeed;
		Player->SprintAt = SprintSpeed;
	}
	// a downloaded character can be any size and face any way: stand it at its height, facing forward
	const float Tall = Mesh->GetBounds().BoxExtent.Z * 2.f;
	GetMesh()->SetRelativeScale3D(FVector(Skin->Height > 0.f && Tall > 1.f ? Skin->Height / Tall : 1.f));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -BodyFacingYaw(), 0.f));
	GetCapsuleComponent()->SetHiddenInGame(true);
	bHasBody = true;
	CurrentSkin = Id;
	PutOnShoes();
	if (bRemember)
	{
		GConfig->SetString(TEXT("NaijaHustle"), TEXT("PlayerSkin"), *Id.ToString(), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: the player is %s (skin %s), %.0f cm tall as made"), *Skin->Name, *Id.ToString(), Tall);
	return true;
}

FString ANHCharacter::SkinName() const
{
	const FNHPlayerSkin* Skin = Skins.FindByPredicate([this](const FNHPlayerSkin& S) { return S.Id == CurrentSkin; });
	return Skin ? Skin->Name : FString();
}

void ANHCharacter::ToggleTorch()
{
	if (!Torch)
	{
		Torch = NewObject<USpotLightComponent>(this);
		Torch->SetupAttachment(RootComponent);
		Torch->SetRelativeLocationAndRotation(FVector(30.f, 18.f, 30.f), FRotator(-6.f, 0.f, 0.f)); // held at the right hip, pointing ahead
		Torch->SetIntensityUnits(ELightUnits::Candelas);
		Torch->SetIntensity(2500.f);
		Torch->SetLightColor(FLinearColor(1.f, 0.95f, 0.85f));
		Torch->SetInnerConeAngle(12.f);
		Torch->SetOuterConeAngle(28.f);
		Torch->SetAttenuationRadius(2500.f);
		Torch->SetCastShadows(false);
		Torch->RegisterComponent();
		Torch->SetVisibility(false);
	}
	Torch->SetVisibility(!Torch->IsVisible());
}

bool ANHCharacter::TorchOn() const
{
	return Torch && Torch->IsVisible();
}

FString ANHCharacter::WearNextSkin(int32 Dir)
{
	const int32 Now = Skins.IndexOfByPredicate([this](const FNHPlayerSkin& S) { return S.Id == CurrentSkin; });
	for (int32 Step = 1; Step <= Skins.Num(); ++Step)
	{
		const FNHPlayerSkin& Next = Skins[((FMath::Max(Now, 0) + Step * (Dir < 0 ? -1 : 1)) % Skins.Num() + Skins.Num()) % Skins.Num()];
		if (WearSkin(Next.Id, true))
		{
			return Next.Name;
		}
	}
	return FString();
}

float ANHCharacter::BodyFacingYaw() const
{
	return FacingYawOf(GetMesh()->GetSkeletalMeshAsset());
}

float ANHCharacter::FacingYawOf(const USkeletalMesh* Mesh)
{
	if (!Mesh)
	{
		return 90.f;
	}
	const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
	FVector Toes = FVector::ZeroVector;
	for (const TCHAR* Side : { TEXT("r"), TEXT("l") })
	{
		const int32 Foot = Skeleton.FindBoneIndex(FName(*FString::Printf(TEXT("foot_%s"), Side)));
		const int32 Ball = Skeleton.FindBoneIndex(FName(*FString::Printf(TEXT("ball_%s"), Side)));
		if (Foot != INDEX_NONE && Ball != INDEX_NONE)
		{
			Toes += FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Ball).GetLocation() - FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Foot).GetLocation();
		}
	}
	return Toes.SizeSquared2D() > 1.f ? FMath::RadiansToDegrees(FMath::Atan2(Toes.Y, Toes.X)) : 90.f;
}

void ANHCharacter::PutOnShoes()
{
	for (UStaticMeshComponent* Old : ShoeParts)
	{
		if (Old)
		{
			Old->DestroyComponent();
		}
	}
	ShoeParts.Reset();
	const FNHPlayerSkin* Skin = Skins.FindByPredicate([this](const FNHPlayerSkin& S) { return S.Id == CurrentSkin; });
	if (Skin && Skin->bShod)
	{
		return;
	}
	const FSoftObjectPath Path = ShoeMesh.ToSoftObjectPath();
	UStaticMesh* Shoe = Path.IsValid() && FPackageName::DoesPackageExist(Path.GetLongPackageName()) ? ShoeMesh.LoadSynchronous() : nullptr;
	if (!Shoe)
	{
		return;
	}
	// Worked out in the body's reference pose, where it stands flat on z = 0, so it does not matter which way a
	// skeleton's foot bones happen to point. The shoes keep their own size whatever the body is scaled by.
	const float Facing = BodyFacingYaw();
	const float BodyScale = FMath::Max(GetMesh()->GetRelativeScale3D().X, 0.01f);
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
		const FVector Scale(ShoeScale / BodyScale, ShoeScale * Mirror / BodyScale, ShoeScale / BodyScale);
		const FQuat Turn(FRotator(0.f, Facing + ShoeYaw * Mirror, 0.f)); // toe to the front
		const FVector Ground(Foot.GetLocation().X, Foot.GetLocation().Y, 0.f);
		const FTransform Placed(Turn, Ground - Turn.RotateVector(Ankle * Scale), Scale);

		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
		Part->SetStaticMesh(Shoe);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetupAttachment(GetMesh(), Bone);
		Part->SetRelativeTransform(Placed.GetRelativeTransform(Foot));
		Part->RegisterComponent();
		ShoeParts.Add(Part);
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
	const ANHPlayerController* PC = Cast<ANHPlayerController>(Controller);
	const FVector2D Axis = Value.Get<FVector2D>() * (PC ? PC->LookScale : 1.f);
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
