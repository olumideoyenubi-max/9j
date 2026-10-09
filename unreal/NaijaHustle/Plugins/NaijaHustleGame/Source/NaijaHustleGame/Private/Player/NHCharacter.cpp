#include "Player/NHCharacter.h"

#include "World/NHShapes.h"
#include "EngineUtils.h"
#include "UI/NHHUD.h"
#include "Vehicles/NHVehicle.h"
#include "Core/NHHustleSubsystem.h"
#include "Gameplay/NHPerson.h"
#include "Audio/NHAudioSubsystem.h"
#include "Components/PointLightComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimMontage.h"
#include "AnimationRuntime.h"
#include "Camera/CameraComponent.h"
#include "Characters/NHOutfitComponent.h"
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
	// A character in its own folder with the mannequin's animations retargeted onto it, wearing its own shoes: the
	// people built by build_people_makehuman.py, then Sketchfab downloads brought in by prep_character_gltf.py
	const auto Download = [this, &Skin](const TCHAR* Id, const TCHAR* Name, const TCHAR* Folder, float Height)
	{
		Skin(Id, Name, *FString::Printf(TEXT("/Game/Characters/Player/%s/%s.%s"), Folder, Folder, Folder),
			*FString::Printf(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed_%s.ABP_Unarmed_%s_C"), Folder, Folder), Height);
		Skins.Last().bShod = true;
	};
	Download(TEXT("tunde"), TEXT("Tunde, afro and denim"), TEXT("Tunde"), 180.f);
	Download(TEXT("emeka"), TEXT("Emeka, big man in a suit"), TEXT("Emeka"), 184.f);
	Download(TEXT("dayo"), TEXT("Dayo, cornrows and shorts"), TEXT("Dayo"), 178.f);
	Download(TEXT("amaka"), TEXT("Amaka, side-swept hair"), TEXT("Amaka"), 168.f);
	Download(TEXT("zainab"), TEXT("Zainab, afro puffs"), TEXT("Zainab"), 166.f);
	Download(TEXT("ngozi"), TEXT("Ngozi, low cut, gym wear"), TEXT("Ngozi"), 170.f);
	Download(TEXT("mark"), TEXT("Mark, brown hair, striped shirt"), TEXT("Mark"), 180.f);
	Download(TEXT("kate"), TEXT("Kate, blonde ponytail"), TEXT("Kate"), 168.f);
	Download(TEXT("chen"), TEXT("Chen, fringe and jacket"), TEXT("Chen"), 174.f);
	Download(TEXT("mei"), TEXT("Mei, black bob"), TEXT("Mei"), 162.f);
	Download(TEXT("priya"), TEXT("Priya, long dark hair"), TEXT("Priya"), 164.f);
	Download(TEXT("lowpoly"), TEXT("Area boy, orange tee"), TEXT("Lowpoly"), 175.f);
	Download(TEXT("africanman"), TEXT("Gym man, cargo trousers"), TEXT("AfricanMan"), 180.f);
	Download(TEXT("nathan"), TEXT("Bearded man, grey tee"), TEXT("Nathan"), 180.f);
	Download(TEXT("eric"), TEXT("Office man, waistcoat"), TEXT("Eric"), 180.f);
	Download(TEXT("tarzan"), TEXT("Blond man, green polo"), TEXT("Tarzan"), 180.f);
	Download(TEXT("indianman"), TEXT("Man in dhoti"), TEXT("IndianMan"), 172.f);
	Download(TEXT("kuratchi"), TEXT("Man in check shirt"), TEXT("Kuratchi"), 172.f);
	Download(TEXT("woman3"), TEXT("Woman in beanie"), TEXT("Woman3"), 166.f);
	Download(TEXT("carla"), TEXT("Office woman, curly hair"), TEXT("Carla"), 166.f);
	Download(TEXT("claudia"), TEXT("Office woman, blonde ponytail"), TEXT("Claudia"), 168.f);
	Download(TEXT("sophia"), TEXT("Woman, long brown hair"), TEXT("Sophia"), 168.f);
	Download(TEXT("teenblack"), TEXT("Woman, long black hair"), TEXT("TeenBlack"), 162.f);
	Skin(TEXT("mannequin"), TEXT("Mannequin"), TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"), TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"), 0.f);
	Outfit = CreateDefaultSubobject<UNHOutfitComponent>(TEXT("Outfit"));
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
	MeshTurn = GetMesh()->GetRelativeRotation();
	MeshHome = FVector(0.f, 0.f, -92.f);
	GetMesh()->SetRelativeLocation(MeshHome);
	GetCapsuleComponent()->SetHiddenInGame(true);
	bHasBody = true;
	CurrentSkin = Id;
	bFrontShown = false;
	// a person with a wardrobe swaps the one-piece mesh for a bare body and the clothes last chosen for them
	Outfit->Undress();
	FString Worn;
	GConfig->GetString(TEXT("NaijaHustle"), *FString::Printf(TEXT("Outfit_%s"), *Id.ToString()), Worn, GGameUserSettingsIni);
	Outfit->Dress(GetMesh(), FPaths::GetBaseFilename(Skin->Mesh.GetLongPackageName()), Worn);
	PutOnShoes();
	if (bRemember)
	{
		GConfig->SetString(TEXT("NaijaHustle"), TEXT("PlayerSkin"), *Id.ToString(), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: the player is %s (skin %s), %.0f cm tall as made"), *Skin->Name, *Id.ToString(), Tall);
	return true;
}

void ANHCharacter::ChangeOutfit(ENHOutfitSlot Slot, int32 Dir, bool bColour)
{
	if (!Outfit->HasWardrobe())
	{
		return;
	}
	bColour ? Outfit->StepColour(Slot, Dir) : Outfit->Step(Slot, Dir);
	GConfig->SetString(TEXT("NaijaHustle"), *FString::Printf(TEXT("Outfit_%s"), *CurrentSkin.ToString()), *Outfit->Describe(), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void ANHCharacter::ShowFront(bool bFront)
{
	if (bFront != bFrontShown && bHasBody)
	{
		bFrontShown = bFront;
		// toward the camera, which sits behind and to the right
		const float CameraYaw = (FollowCamera->GetComponentLocation() - GetActorLocation()).Rotation().Yaw - GetActorRotation().Yaw;
		GetMesh()->SetRelativeRotation(bFront ? MeshTurn + FRotator(0.f, CameraYaw, 0.f) : MeshTurn);
	}
}

FString ANHCharacter::SkinName() const
{
	const FNHPlayerSkin* Skin = Skins.FindByPredicate([this](const FNHPlayerSkin& S) { return S.Id == CurrentSkin; });
	return Skin ? Skin->Name : FString();
}

// ------------------------------------------------------------------------------------------------- weapons
FString ANHCharacter::WeaponName(FName InWeapon)
{
	return InWeapon == TEXT("machete") ? TEXT("Machete") : InWeapon == TEXT("pistol") ? TEXT("Pistol") : InWeapon == TEXT("ak47") ? TEXT("AK-47") : FString();
}

FName ANHCharacter::Equip(FName InWeapon)
{
	const FName Was = Weapon;
	Weapon = InWeapon == Weapon ? NAME_None : InWeapon;
	BuildWeapon();
	bTrigger = false;
	SwingLeft = 0.f;
	if (UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this); Audio && Was != Weapon)
	{
		Audio->PlayShot((Weapon.IsNone() ? Was : Weapon) == TEXT("machete") ? ENHShot::DrawBlade : ENHShot::Draw, GetActorLocation(), ENHSoundKind::Weapon, 0.8f);
	}
	return Weapon;
}

UAnimSequence* ANHCharacter::ActionClip(const USkeletalMesh* Mesh, const FString& Clip)
{
	// By the skeleton, not the mesh: a body in changeable clothes is another mesh (Wardrobe/Body_Tunde) on the same Tunde_Skeleton.
	const USkeleton* Bones = Mesh ? Mesh->GetSkeleton() : nullptr;
	if (!Bones)
	{
		return nullptr;
	}
	// looked for once: a body without the clips is asked about them every frame
	static TMap<FString, TWeakObjectPtr<UAnimSequence>> Found;
	static TSet<FString> Missing;
	FString Who = Bones->GetName();
	Who.RemoveFromEnd(TEXT("_Skeleton"));
	const FString Path = FPackageName::GetLongPackagePath(Bones->GetPackage()->GetName()) / TEXT("Anims") / (Who + TEXT("_") + Clip);
	if (Missing.Contains(Path))
	{
		return nullptr;
	}
	if (const TWeakObjectPtr<UAnimSequence>* Have = Found.Find(Path); Have && Have->IsValid())
	{
		return Have->Get();
	}
	UAnimSequence* Sequence = FPackageName::DoesPackageExist(Path) ? LoadObject<UAnimSequence>(nullptr, *Path) : nullptr;
	if (Sequence)
	{
		// The clips were made on one body and brought in for each: every bone keeps its own body's length (only the
		// hips' height, and the IK bones, are the clip's), or a shorter woman would be stretched to his arms and legs.
		static TSet<TWeakObjectPtr<USkeleton>> Fitted;
		if (USkeleton* Skeleton = Sequence->GetSkeleton(); Skeleton && !Fitted.Contains(Skeleton))
		{
			Fitted.Add(Skeleton);
			const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
			for (int32 I = 0; I < Ref.GetNum(); ++I)
			{
				const FString Bone = Ref.GetBoneName(I).ToString();
				const bool bOwn = I == 0 || Bone.StartsWith(TEXT("ik_"));
				Skeleton->SetBoneTranslationRetargetingMode(I, bOwn ? EBoneTranslationRetargetingMode::Animation
					: Bone == TEXT("pelvis") ? EBoneTranslationRetargetingMode::AnimationScaled : EBoneTranslationRetargetingMode::Skeleton);
			}
		}
		Sequence->AddToRoot(); // a handful of short clips, kept for the session
		Found.Add(Path, Sequence);
	}
	else
	{
		Missing.Add(Path);
	}
	return Sequence;
}

UAnimSequence* ANHCharacter::Clip(const TCHAR* Name) const
{
	return ActionClip(GetMesh()->GetSkeletalMeshAsset(), Name);
}

void ANHCharacter::ToggleCrouch()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else if (Clip(TEXT("Crouch_Idle")) && RollLeft <= 0.f && ClimbTime <= 0.f && GetCharacterMovement()->IsMovingOnGround())
	{
		GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
		GetCharacterMovement()->SetCrouchedHalfHeight(62.f);
		GetCharacterMovement()->MaxWalkSpeedCrouched = 112.f; // the crouched walk clip covers 80 cm a second: it is played 1.4 times as fast
		SetSprinting(false);
		Crouch();
	}
}

void ANHCharacter::PlayShot(const TCHAR* Name, float Rate)
{
	UAnimInstance* Anim = GetMesh()->GetAnimInstance();
	UAnimSequence* Sequence = Clip(Name);
	if (!Anim || !Sequence)
	{
		return;
	}
	if (HoldMontage)
	{
		Anim->Montage_Stop(0.08f, HoldMontage);
		HoldMontage = nullptr;
	}
	HoldClip = NAME_None; // whatever was held comes back when this has played
	if (Anim->PlaySlotAnimationAsDynamicMontage(Sequence, TEXT("DefaultSlot"), 0.06f, 0.18f, Rate, 1))
	{
		ShotClip = Name;
		ShotLeft = Sequence->GetPlayLength() / Rate - 0.12f;
	}
}

void ANHCharacter::UpdateActions(float DeltaSeconds)
{
	UAnimInstance* Anim = GetMesh()->GetAnimInstance();
	if (!Anim)
	{
		return;
	}
	ShotLeft = FMath::Max(0.f, ShotLeft - DeltaSeconds);
	const bool bBusy = RollLeft > 0.f || ClimbTime > 0.f || !GetCharacterMovement()->IsMovingOnGround();
	const bool bMoving = GetVelocity().SizeSquared2D() > 400.f;
	const bool bPistol = Weapon == TEXT("pistol"), bRifle = Weapon == TEXT("ak47");
	SinceShot += DeltaSeconds;
	// a gun is raised while aiming (right mouse), walking or not, and for a moment after a shot; otherwise it hangs in the hand
	const bool bRaised = IsAiming() || (SinceShot < 1.4f && !bMoving);
	// The clips are the whole body standing in one place, so guns are aimed and the machete held on guard only while
	// stood still; walking, the body's own animation carries on with the weapon in the hand.
	const TCHAR* Want = nullptr;
	if (bBusy)
	{
		if (bIsCrouched && RollLeft > 0.f)
		{
			UnCrouch();
		}
	}
	else if (bIsCrouched)
	{
		Want = bMoving ? TEXT("Crouch_Walk") : bPistol ? TEXT("Crouch_Pistol_Aim") : bRifle ? TEXT("Crouch_Rifle_Aim") : TEXT("Crouch_Idle");
	}
	else if (bPistol || bRifle)
	{
		Want = !bRaised ? nullptr : bPistol ? TEXT("Pistol_Aim") : bTrigger ? TEXT("Rifle_Fire") : TEXT("Rifle_Aim");
	}
	else if (!bMoving && Weapon == TEXT("machete"))
	{
		Want = TEXT("Machete_Idle");
	}
	if (ShotLeft > 0.f && !bBusy)
	{
		return; // a shot or a cut is playing through
	}
	const FName WantName = Want ? FName(Want) : NAME_None;
	if (WantName == HoldClip)
	{
		return;
	}
	if (HoldMontage)
	{
		Anim->Montage_Stop(0.2f, HoldMontage);
		HoldMontage = nullptr;
	}
	HoldClip = NAME_None;
	if (UAnimSequence* Sequence = Want ? Clip(Want) : nullptr)
	{
		HoldMontage = Anim->PlaySlotAnimationAsDynamicMontage(Sequence, TEXT("DefaultSlot"), 0.2f, 0.2f, WantName == TEXT("Crouch_Walk") ? 1.4f : 1.f, 100000);
		HoldClip = HoldMontage ? WantName : NAME_None;
	}
}

void ANHCharacter::Hurt(float Damage)
{
	Health = FMath::Max(0.f, Health - Damage);
	SinceHurt = 0.f;
}

void ANHCharacter::SetTrigger(bool bHeld)
{
	bTriggerFresh = bHeld && !bTrigger;
	bTrigger = bHeld;
}

void ANHCharacter::Attack()
{
	UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this);
	const FVector Muzzle = WeaponPivot ? WeaponPivot->GetComponentLocation() + GetActorForwardVector() * 40.f : GetActorLocation();
	++Attacks;
	if (Weapon == TEXT("machete"))
	{
		// with the clips: a cut across, the cut back, then a chop down, each landing as the blade comes through
		static const TCHAR* Cuts[] = { TEXT("Machete_Slash"), TEXT("Machete_Backslash"), TEXT("Machete_Chop") };
		const bool bClips = Clip(Cuts[0]) != nullptr && GetCharacterMovement()->IsMovingOnGround() && RollLeft <= 0.f && ClimbTime <= 0.f;
		if (bClips)
		{
			if (bIsCrouched)
			{
				UnCrouch();
			}
			PlayShot(Cuts[Swings++ % 3], 1.5f);
		}
		AttackWait = bClips ? 0.64f : 0.5f;
		SwingLength = bClips ? 0.5f : 0.28f;
		SwingLandsAt = bClips ? 0.23f : 0.14f;
		SwingLeft = SwingLength;
		bSwingLanded = false;
		if (Audio)
		{
			Audio->PlayShot(ENHShot::MacheteSwing, Muzzle);
		}
		return;
	}
	const bool bRifle = Weapon == TEXT("ak47");
	AttackWait = bRifle ? AttackWait + 0.1f : 0.16f; // 600 rounds a minute; a pistol as fast as the finger
	SinceShot = 0.f;
	if (!bRifle && !bIsCrouched && GetVelocity().SizeSquared2D() < 400.f && GetCharacterMovement()->IsMovingOnGround())
	{
		PlayShot(TEXT("Pistol_Fire"), 1.f); // the kick; the rifle's is held while the trigger is (UpdateActions)
	}
	if (Audio)
	{
		Audio->PlayShot(bRifle ? ENHShot::Rifle : ENHShot::Pistol, Muzzle);
		Audio->PlayShot(bRifle ? ENHShot::RifleTail : ENHShot::PistolTail, Muzzle, ENHSoundKind::WeaponTail, 0.7f);
	}
	// where the camera looks, out to 150 m: the bullet lands there with a knock. It hurts nothing yet.
	FVector From = GetActorLocation();
	FRotator Aim = GetActorRotation();
	if (const AController* Who = GetController())
	{
		Who->GetPlayerViewPoint(From, Aim);
	}
	const FVector Spread = FMath::VRandCone(Aim.Vector(), FMath::DegreesToRadians(bRifle ? 1.6f : 0.8f));
	FCollisionQueryParams Query(SCENE_QUERY_STAT(NHShot), false, this);
	FHitResult Hit;
	const bool bWall = GetWorld()->LineTraceSingleByChannel(Hit, From, From + Spread * 15000.f, ECC_Visibility, Query);
	// people do not stop rays (their bodies have no collision), so they are looked for along the bullet's line up to whatever it hit
	float Reach = 0.f;
	if (ANHPerson* Person = ANHPerson::OnRay(GetWorld(), From, Spread, bWall ? Hit.Distance : 15000.f, Reach))
	{
		Land(Person, nullptr, From + Spread * Reach, bRifle ? 38.f : 45.f, 0.f, false);
	}
	else if (bWall)
	{
		Land(nullptr, Hit.GetActor(), Hit.ImpactPoint, 0.f, bRifle ? 9.f : 6.f, false);
	}
	// a gun going off: everybody within 45 m runs, and it is noticed
	ANHPerson::ScareAround(GetWorld(), GetActorLocation(), 4500.f);
	if (UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this))
	{
		Hustle->AddHeat(0.12f);
	}
	if (!MuzzleFlash)
	{
		MuzzleFlash = NewObject<UPointLightComponent>(this);
		MuzzleFlash->SetupAttachment(GetRootComponent());
		MuzzleFlash->SetLightColor(FLinearColor(1.f, 0.72f, 0.35f));
		MuzzleFlash->SetAttenuationRadius(600.f);
		MuzzleFlash->SetCastShadows(false);
		MuzzleFlash->RegisterComponent();
	}
	MuzzleFlash->SetWorldLocation(Muzzle);
	MuzzleFlash->SetIntensity(bRifle ? 9000.f : 6000.f);
	FlashLeft = 0.05f;
}

void ANHCharacter::SwingLand()
{
	// half way through the swing: anything within arm's and blade's reach in front rings
	bSwingLanded = true;
	const FVector From = GetActorLocation() + FVector(0.f, 0.f, 30.f), Ahead = GetActorForwardVector();
	// a person within reach in front first (their bodies stop no rays), then anything solid
	ANHPerson* Near = nullptr;
	float NearSq = FMath::Square(170.f);
	for (TActorIterator<ANHPerson> It(GetWorld()); It; ++It)
	{
		const FVector To = It->GetActorLocation() - GetActorLocation();
		if (!It->IsDown() && To.SizeSquared2D() < NearSq && FVector::DotProduct(To.GetSafeNormal2D(), Ahead) > 0.5f)
		{
			NearSq = To.SizeSquared2D();
			Near = *It;
		}
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(NHSwing), false, this);
	FHitResult Hit;
	if (Near)
	{
		Land(Near, nullptr, Near->GetActorLocation() + FVector(0.f, 0.f, 120.f), 60.f, 0.f, true);
	}
	else if (GetWorld()->SweepSingleByChannel(Hit, From, From + Ahead * 130.f, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(25.f), Query))
	{
		Land(nullptr, Hit.GetActor(), Hit.ImpactPoint, 0.f, 4.f, true);
	}
}

void ANHCharacter::Land(ANHPerson* Person, AActor* Other, const FVector& At, float PersonDamage, float VehicleDamage, bool bBlade)
{
	UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this);
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	if (Person)
	{
		const bool bKilled = Person->Hurt(PersonDamage, GetActorLocation());
		if (Audio)
		{
			Audio->PlayShot(ENHShot::BodyHit, At, ENHSoundKind::Impact);
		}
		if (!Person->bEssential)
		{
			++PeopleHit;
			PeopleDown += bKilled ? 1 : 0;
			if (Hustle)
			{
				Hustle->AddHeat(bKilled ? 1.5f : 0.6f); // hurting somebody is a star; killing them is more
			}
			if (bBlade)
			{
				ANHPerson::ScareAround(GetWorld(), GetActorLocation(), 1500.f); // a blade is quiet: only those who see it run
			}
		}
		return;
	}
	if (Audio)
	{
		Audio->PlayShot(bBlade ? ENHShot::MacheteHit : ENHShot::BulletHit, At, ENHSoundKind::Impact);
	}
	if (ANHVehicle* Car = Cast<ANHVehicle>(Other))
	{
		// the same health a crash takes from: at nothing the vehicle is a wreck and will not drive
		const bool bWas = Car->IsWrecked();
		Car->Health = FMath::Max(0.f, Car->Health - VehicleDamage);
		++VehiclesHit;
		if (Hustle)
		{
			Hustle->AddHeat(0.2f);
		}
		if (!bWas && Car->IsWrecked())
		{
			ANHHUD::Toast(this, FString::Printf(TEXT("The %s is wrecked"), *Car->DisplayName()), 2);
		}
	}
}

void ANHCharacter::BuildWeapon()
{
	if (WeaponPivot)
	{
		TArray<USceneComponent*> Pieces;
		WeaponPivot->GetChildrenComponents(true, Pieces);
		for (USceneComponent* Piece : Pieces)
		{
			Piece->DestroyComponent();
		}
		WeaponPivot->DestroyComponent();
		WeaponPivot = nullptr;
	}
	if (Weapon.IsNone())
	{
		return;
	}
	// On the right hand where the body has one. The pivot keeps the world's rotation and scale, not the hand's, and
	// Tick points it the way the player faces: skeletons disagree about which way a hand bone points.
	const bool bHand = GetMesh()->GetSkeletalMeshAsset() && GetMesh()->GetBoneIndex(TEXT("hand_r")) != INDEX_NONE;
	WeaponPivot = NewObject<USceneComponent>(this);
	WeaponPivot->SetupAttachment(bHand ? static_cast<USceneComponent*>(GetMesh()) : GetRootComponent(), bHand ? FName(TEXT("hand_r")) : NAME_None);
	WeaponPivot->SetRelativeLocation(bHand ? FVector::ZeroVector : FVector(25.f, 22.f, 5.f));
	WeaponPivot->SetUsingAbsoluteRotation(true);
	WeaponPivot->SetUsingAbsoluteScale(true);
	WeaponPivot->RegisterComponent();

	// placeholder models from boxes and a cylinder, X along the barrel or blade, sizes in cm
	const FNHSurface Steel(FLinearColor(0.55f, 0.56f, 0.58f), 0.35f, 0.f, 0.9f), Dark(FLinearColor(0.04f, 0.04f, 0.045f), 0.5f, 0.f, 0.6f), Wood(FLinearColor(0.3f, 0.16f, 0.07f), 0.7f);
	const auto Piece = [this](ENHShape Shape, const FVector& At, const FVector& Size, const FNHSurface& Surface, const FRotator& Turn = FRotator::ZeroRotator)
	{
		// 9 cm out from the hand, clear of the leg, so it shows from behind
		if (UStaticMeshComponent* Part = NHShapes::AddPiece(this, WeaponPivot, Shape, At + FVector(4.f, 9.f, 0.f), Size, Surface, Turn))
		{
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Part->SetCanEverAffectNavigation(false);
		}
	};
	if (Weapon == TEXT("machete"))
	{
		Piece(ENHShape::Box, FVector(0.f, 0.f, 0.f), FVector(13.f, 3.f, 2.6f), Wood);    // handle
		Piece(ENHShape::Box, FVector(31.f, 0.f, 1.f), FVector(50.f, 0.6f, 5.5f), Steel); // blade
	}
	else if (Weapon == TEXT("pistol"))
	{
		Piece(ENHShape::Box, FVector(0.f, 0.f, -4.f), FVector(4.5f, 3.f, 11.f), Dark, FRotator(-12.f, 0.f, 0.f)); // grip
		Piece(ENHShape::Box, FVector(6.f, 0.f, 3.f), FVector(19.f, 3.2f, 4.5f), Dark);                             // slide
	}
	else
	{
		Piece(ENHShape::Box, FVector(-24.f, 0.f, -1.f), FVector(26.f, 3.5f, 6.f), Wood, FRotator(8.f, 0.f, 0.f)); // stock
		Piece(ENHShape::Box, FVector(6.f, 0.f, 2.f), FVector(34.f, 4.f, 6.5f), Dark);                             // receiver
		Piece(ENHShape::Box, FVector(-2.f, 0.f, -6.f), FVector(4.f, 3.f, 10.f), Wood, FRotator(-15.f, 0.f, 0.f)); // grip
		Piece(ENHShape::Box, FVector(12.f, 0.f, -10.f), FVector(6.f, 3.f, 18.f), Dark, FRotator(20.f, 0.f, 0.f)); // the curved magazine
		Piece(ENHShape::Box, FVector(32.f, 0.f, 1.5f), FVector(18.f, 4.5f, 5.5f), Wood);                          // handguard
		Piece(ENHShape::Cylinder, FVector(55.f, 0.f, 2.5f), FVector(2.2f, 2.2f, 30.f), Dark, FRotator(90.f, 0.f, 0.f)); // barrel
	}
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
void ANHCharacter::OnSprintStop() { SetSprinting(bRunLocked); }

void ANHCharacter::ToggleRun()
{
	bRunLocked = !bRunLocked;
	SetSprinting(bRunLocked);
}

void ANHCharacter::Roll()
{
	if (RollLeft > 0.f || RollWait > 0.f || ClimbTime > 0.f || !GetCharacterMovement()->IsMovingOnGround())
	{
		return;
	}
	const FVector Moving = GetVelocity().GetSafeNormal2D();
	RollDir = Moving.IsNearlyZero() ? GetActorForwardVector() : Moving;
	RollLeft = 0.6f;
	RollWait = 1.f;
	SetActorRotation(FRotator(0.f, RollDir.Rotation().Yaw, 0.f));
}

bool ANHCharacter::TryClimb()
{
	if (ClimbTime > 0.f || RollLeft > 0.f)
	{
		return false;
	}
	if (bIsCrouched)
	{
		UnCrouch();
	}
	// something solid in front at waist height, with a top within reach and room to stand on it
	const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, Half), Fwd = GetActorForwardVector();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(NHClimb), false, this);
	FCollisionObjectQueryParams Solid;
	Solid.AddObjectTypesToQuery(ECC_WorldStatic);
	Solid.AddObjectTypesToQuery(ECC_Vehicle);
	FHitResult Wall, Top;
	bool bWall = false;
	for (const float Height : { 60.f, 120.f, 30.f, 180.f })
	{
		if (GetWorld()->LineTraceSingleByObjectType(Wall, Feet + FVector(0.f, 0.f, Height), Feet + FVector(0.f, 0.f, Height) + Fwd * (Radius + 70.f), Solid, Q))
		{
			bWall = true;
			break;
		}
	}
	if (!bWall)
	{
		return false;
	}
	const FVector Over = FVector(Wall.ImpactPoint.X, Wall.ImpactPoint.Y, 0.f) + Fwd * (Radius + 12.f);
	if (!GetWorld()->LineTraceSingleByObjectType(Top, FVector(Over.X, Over.Y, Feet.Z + 330.f), FVector(Over.X, Over.Y, Feet.Z + 40.f), Solid, Q) || Top.bStartPenetrating) // 3.3 m: a jump and a pull up
	{
		return false; // too high, or nothing to stand on
	}
	const float Up = Top.ImpactPoint.Z - Feet.Z;
	const FVector Stand(Over.X, Over.Y, Top.ImpactPoint.Z + Half + 3.f);
	if (Up < 40.f || Top.ImpactNormal.Z < 0.7f
		|| GetWorld()->OverlapBlockingTestByChannel(Stand + FVector(0.f, 0.f, 4.f), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius * 0.9f, Half * 0.95f), Q))
	{
		return false;
	}
	ClimbFrom = GetActorLocation();
	ClimbTo = Stand;
	ClimbLength = 0.35f + Up / 380.f; // a kerb in a hop, a wall in most of a second
	ClimbTime = ClimbLength;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	SetActorEnableCollision(false);
	return true;
}

void ANHCharacter::Jump()
{
	if (bIsCrouched)
	{
		UnCrouch(); // up first
		return;
	}
	if (!TryClimb())
	{
		Super::Jump();
	}
}

void ANHCharacter::SetSprinting(bool bSprint)
{
	bSprinting = bSprint;
	GetCharacterMovement()->MaxWalkSpeed = bSprint ? SprintSpeed : WalkSpeed;
}

void ANHCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SinceHurt += DeltaSeconds;
	if (SinceHurt > 6.f && Health > 0.f)
	{
		Health = FMath::Min(100.f, Health + 8.f * DeltaSeconds); // six quiet seconds, then it comes back
	}
	// while the AK-47 is firing the time owed is carried over, so its rate does not depend on the frame rate
	AttackWait = FMath::Max(bTrigger && Weapon == TEXT("ak47") ? -0.1f : 0.f, AttackWait - DeltaSeconds);
	if (!Weapon.IsNone() && AttackWait <= 0.f && (bTriggerFresh || (bTrigger && Weapon == TEXT("ak47"))))
	{
		Attack();
	}
	bTriggerFresh = false;
	if (SwingLeft > 0.f)
	{
		SwingLeft -= DeltaSeconds;
		if (!bSwingLanded && SwingLeft < SwingLandsAt)
		{
			SwingLand();
		}
	}
	if (MuzzleFlash && FlashLeft > 0.f)
	{
		FlashLeft -= DeltaSeconds;
		if (FlashLeft <= 0.f)
		{
			MuzzleFlash->SetIntensity(0.f);
		}
	}
	UpdateActions(DeltaSeconds);
	if (WeaponPivot)
	{
		const USkeletalMeshComponent* Body = GetMesh();
		if ((ShotLeft > 0.f || !HoldClip.IsNone()) && Body->GetBoneIndex(TEXT("index_01_r")) != INDEX_NONE && Body->GetBoneIndex(TEXT("pinky_01_r")) != INDEX_NONE)
		{
			// a clip is posing the hand: the weapon lies in the fist, a barrel along the knuckles' way, a blade out of the thumb side
			const FVector Wrist = Body->GetBoneLocation(TEXT("hand_r")), Index = Body->GetBoneLocation(TEXT("index_01_r")), Pinky = Body->GetBoneLocation(TEXT("pinky_01_r"));
			const FVector Along = ((Index + Pinky) * 0.5f - Wrist).GetSafeNormal(), Thumb = (Index - Pinky).GetSafeNormal();
			const FVector Palm = FVector::CrossProduct(Thumb, Along).GetSafeNormal() * (FVector::DotProduct(FVector::CrossProduct(Thumb, Along), Body->GetBoneLocation(TEXT("thumb_01_r")) - Wrist) < 0.f ? -1.f : 1.f);
			const FRotator Held = Weapon == TEXT("machete") ? FRotationMatrix::MakeFromXZ(Thumb, Along).Rotator() : FRotationMatrix::MakeFromXZ(Along, Thumb).Rotator();
			WeaponPivot->SetWorldLocationAndRotation(Wrist + Along * 8.5f + Palm * 2.f - Held.RotateVector(FVector(4.f, 9.f, 0.f)), Held); // the pieces are built 4 and 9 cm off the pivot
		}
		else
		{
			// held pointing where the player faces: the machete up at an angle (and chopping down through a swing), the guns a little down
			const float Chop = SwingLeft > 0.f ? -110.f * FMath::Sin(UE_PI * (1.f - SwingLeft / SwingLength)) : 0.f;
			WeaponPivot->SetRelativeLocation(Body->GetBoneIndex(TEXT("hand_r")) != INDEX_NONE ? FVector::ZeroVector : FVector(25.f, 22.f, 5.f));
			WeaponPivot->SetWorldRotation(GetActorRotation() + FRotator(Weapon == TEXT("machete") ? 35.f + Chop : -8.f, 0.f, 0.f));
		}
	}

	// running without the key held stops by itself once you have stood still a moment
	StillFor = GetVelocity().SizeSquared2D() < 100.f ? StillFor + DeltaSeconds : 0.f;
	if (bRunLocked && StillFor > 1.5f)
	{
		bRunLocked = false;
		SetSprinting(false);
	}
	RollWait = FMath::Max(0.f, RollWait - DeltaSeconds);
	if (ClimbTime > 0.f)
	{
		// up the face first, then over the edge onto the top
		ClimbTime = FMath::Max(0.f, ClimbTime - DeltaSeconds);
		const float A = 1.f - ClimbTime / ClimbLength;
		const float Rise = FMath::InterpEaseOut(0.f, 1.f, FMath::Clamp(A / 0.65f, 0.f, 1.f), 2.f), Over = FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp((A - 0.45f) / 0.55f, 0.f, 1.f), 2.f);
		SetActorLocation(FVector(FMath::Lerp(ClimbFrom.X, ClimbTo.X, Over), FMath::Lerp(ClimbFrom.Y, ClimbTo.Y, Over), FMath::Lerp(ClimbFrom.Z, ClimbTo.Z, Rise)));
		GetMesh()->SetRelativeRotation((FQuat(FVector::RightVector, FMath::DegreesToRadians(-22.f * FMath::Sin(A * PI))) * MeshTurn.Quaternion()).Rotator()); // leaning into it
		if (ClimbTime <= 0.f)
		{
			SetActorEnableCollision(true);
			GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			GetMesh()->SetRelativeRotation(MeshTurn);
		}
	}
	if (RollLeft > 0.f)
	{
		// the body tucks and turns once about its middle, close to the ground, while it is carried forward
		RollLeft = FMath::Max(0.f, RollLeft - DeltaSeconds);
		const float A = 1.f - RollLeft / 0.6f;
		GetCharacterMovement()->Velocity = FVector(RollDir.X * 720.f, RollDir.Y * 720.f, GetCharacterMovement()->Velocity.Z);
		const FQuat Turn(FVector::RightVector, FMath::DegreesToRadians(360.f * A)); // head over heels, forward
		const FVector Middle(0.f, 0.f, -92.f + 55.f - 30.f * FMath::Sin(A * PI));
		GetMesh()->SetRelativeLocationAndRotation(Middle + Turn.RotateVector(FVector(0.f, 0.f, -55.f)), (Turn * MeshTurn.Quaternion()).Rotator());
		if (RollLeft <= 0.f)
		{
			GetMesh()->SetRelativeLocationAndRotation(MeshHome, MeshTurn);
		}
	}

	// the camera follows what the body is doing: holding Sprint while standing still changes nothing
	const bool bRunning = bSprinting && GetVelocity().SizeSquared2D() > FMath::Square(WalkSpeed * 1.1f);
	SprintAlpha = FMath::FInterpTo(SprintAlpha, bRunning ? 1.f : 0.f, DeltaSeconds, bRunning ? 4.f : 2.5f);
	const float A = FMath::InterpEaseInOut(0.f, 1.f, SprintAlpha, 2.f);
	// aiming: in over the right shoulder with a narrower view, the body turned to where the camera looks, and no running
	AimK = FMath::FInterpTo(AimK, IsAiming() ? 1.f : 0.f, DeltaSeconds, 9.f);
	CameraBoom->TargetArmLength = FMath::Lerp(FMath::Lerp(WalkArmLength, SprintArmLength, A), WalkArmLength * 0.5f, AimK);
	CameraBoom->SocketOffset = FMath::Lerp(FVector(0.f, 50.f, 45.f), FVector(0.f, 62.f, 52.f), AimK);
	FollowCamera->SetFieldOfView(FMath::Lerp(FMath::Lerp(WalkFOV, SprintFOV, A), WalkFOV * 0.72f, AimK));
	if (IsAiming())
	{
		if (bSprinting)
		{
			bRunLocked = false;
			SetSprinting(false);
		}
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, GetControlRotation().Yaw, 0.f), DeltaSeconds, 14.f));
	}
	GetCharacterMovement()->bOrientRotationToMovement = !IsAiming();

	// handheld sway: a small bob and roll in step with the run, fading out with the sprint
	SwayTime += DeltaSeconds;
	const float K = A * SprintSway, Step = SwayTime * 11.f;
	FollowCamera->SetRelativeLocationAndRotation(
		FVector(0.f, FMath::Sin(Step * 0.5f) * 1.2f * K, FMath::Sin(Step) * 1.6f * K),
		FRotator(FMath::Sin(Step + 0.6f) * 0.35f * K, FMath::Sin(Step * 0.5f) * 0.25f * K, FMath::Sin(Step * 0.5f + 1.3f) * 0.5f * K));
}
