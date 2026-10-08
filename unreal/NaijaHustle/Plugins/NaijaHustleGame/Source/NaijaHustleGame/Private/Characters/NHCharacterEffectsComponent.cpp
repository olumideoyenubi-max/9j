// NHCharacterEffectsComponent.cpp

#include "Characters/NHCharacterEffectsComponent.h"

#include "AnimationRuntime.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

static_assert(NH_CHARACTER_EFFECTS_MAX_HITS <= 32, "DirtyHitMask is a uint32");

namespace NHCharacterEffects
{
	/** Pre-skinned centre used for meshes that don't contain the hit bone (keeps the mask off). */
	static const FVector3f FarAwayCentre(1.0e6f, 1.0e6f, 1.0e6f);

	static constexpr float ShelterCheckInterval = 0.5f;
	static constexpr float SignificanceInterval = 0.5f;

	/** Move Current toward Target at different rates for rising and falling. */
	FORCEINLINE float Approach(float Current, float Target, float RiseRate, float FallRate, float DeltaTime)
	{
		const float Rate = (Target > Current) ? RiseRate : FallRate;
		return FMath::FInterpConstantTo(Current, Target, DeltaTime, Rate);
	}
}

/* =============================================================================================
 *  UNHCharacterEffectsMIDPool
 * ============================================================================================= */

UMaterialInterface* UNHCharacterEffectsMIDPool::ResolveBaseParent(UMaterialInterface* Material)
{
	// A MID is never a valid pool key: key on the asset it was made from.
	if (const UMaterialInstanceDynamic* AsMID = Cast<UMaterialInstanceDynamic>(Material))
	{
		return AsMID->Parent;
	}
	return Material;
}

UMaterialInstanceDynamic* UNHCharacterEffectsMIDPool::AcquireMID(UMaterialInterface* Parent)
{
	Parent = ResolveBaseParent(Parent);
	if (!Parent)
	{
		return nullptr;
	}

	FNHCharacterEffectsMIDBucket& Bucket = Buckets.FindOrAdd(Parent);
	while (Bucket.Free.Num() > 0)
	{
		UMaterialInstanceDynamic* MID = Bucket.Free.Pop(EAllowShrinking::No);
		if (IsValid(MID))
		{
			return MID;
		}
	}

	// Pool empty: this is the only allocation path. PrewarmPool during loading to avoid it in gameplay.
	UMaterialInstanceDynamic* NewMID = UMaterialInstanceDynamic::Create(Parent, this);
	++Bucket.TotalCreated;
	return NewMID;
}

void UNHCharacterEffectsMIDPool::ReleaseMID(UMaterialInstanceDynamic* MID)
{
	if (!IsValid(MID) || !MID->Parent)
	{
		return;
	}

	// Wipe every override so the next character starts from the parent's defaults.
	MID->ClearParameterValues();

	FNHCharacterEffectsMIDBucket& Bucket = Buckets.FindOrAdd(MID->Parent);
	if (Bucket.Free.Num() < MaxPooledPerParent)
	{
		Bucket.Free.Add(MID);
	}
	// Otherwise drop it; nothing references it any more, so GC reclaims it.
}

void UNHCharacterEffectsMIDPool::PrewarmPool(UMaterialInterface* Parent, int32 Count)
{
	Parent = ResolveBaseParent(Parent);
	if (!Parent || Count <= 0)
	{
		return;
	}

	FNHCharacterEffectsMIDBucket& Bucket = Buckets.FindOrAdd(Parent);
	const int32 Target = FMath::Min(Count, MaxPooledPerParent);
	Bucket.Free.Reserve(Target);
	while (Bucket.Free.Num() < Target)
	{
		Bucket.Free.Add(UMaterialInstanceDynamic::Create(Parent, this));
		++Bucket.TotalCreated;
	}
}

int32 UNHCharacterEffectsMIDPool::GetFreeCount(UMaterialInterface* Parent) const
{
	const FNHCharacterEffectsMIDBucket* Bucket = Buckets.Find(ResolveBaseParent(Parent));
	return Bucket ? Bucket->Free.Num() : 0;
}

void UNHCharacterEffectsMIDPool::Deinitialize()
{
	Buckets.Empty();
	Super::Deinitialize();
}

/* =============================================================================================
 *  UNHCharacterEffectsComponent
 * ============================================================================================= */

UNHCharacterEffectsComponent::UNHCharacterEffectsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// Runs after animation so bone transforms used by ApplyHit are this frame's.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	PrimaryComponentTick.TickInterval = 0.05f;

	SweatParamName = TEXT("SweatIntensity");
	WetnessParamName = TEXT("Wetness");
	for (int32 i = 0; i < NH_CHARACTER_EFFECTS_MAX_HITS; ++i)
	{
		HitSphereParamNames[i] = FName(*FString::Printf(TEXT("DamageHit_%d_Sphere"), i));
		HitDataParamNames[i] = FName(*FString::Printf(TEXT("DamageHit_%d_Data"), i));
		LastPushedHitIntensity[i] = -1.f;
	}
}

void UNHCharacterEffectsComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeEffects();

	if (bAutoBindPointDamage)
	{
		if (AActor* Owner = GetOwner())
		{
			Owner->OnTakePointDamage.AddUniqueDynamic(this, &UNHCharacterEffectsComponent::HandlePointDamage);
		}
	}
}

void UNHCharacterEffectsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakePointDamage.RemoveDynamic(this, &UNHCharacterEffectsComponent::HandlePointDamage);
	}

	// Always hand MIDs back; restoring materials only matters if the actor outlives us.
	ReleaseEffects(EndPlayReason == EEndPlayReason::RemovedFromWorld);
	Super::EndPlay(EndPlayReason);
}

bool UNHCharacterEffectsComponent::MaterialHasScalar(const UMaterialInterface* Material, FName Param)
{
	float Dummy = 0.f;
	return Material && Material->GetScalarParameterValue(FHashedMaterialParameterInfo(FMaterialParameterInfo(Param)), Dummy);
}

bool UNHCharacterEffectsComponent::MaterialHasVector(const UMaterialInterface* Material, FName Param)
{
	FLinearColor Dummy;
	return Material && Material->GetVectorParameterValue(FHashedMaterialParameterInfo(FMaterialParameterInfo(Param)), Dummy);
}

void UNHCharacterEffectsComponent::InitializeEffects()
{
	if (bInitialized)
	{
		return;
	}

	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	Pool = World->GetSubsystem<UNHCharacterEffectsMIDPool>();
	if (!WeatherCollection)
	{
		// the game's own weather, which the lighting rig drives
		WeatherCollection = TSoftObjectPtr<UMaterialParameterCollection>(FSoftObjectPath(TEXT("/Game/NaijaHustle/Lighting/Presets/MPC_NHWeather.MPC_NHWeather"))).LoadSynchronous();
	}
	CachedWeatherInstance = WeatherCollection ? World->GetParameterCollectionInstance(WeatherCollection) : nullptr;

	TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes(Owner);
	MeshEntries.Reset(SkeletalMeshes.Num());

	for (USkeletalMeshComponent* Mesh : SkeletalMeshes)
	{
		if (!Mesh || !Mesh->GetSkinnedAsset())
		{
			continue;
		}

		if (MeshComponentTags.Num() > 0)
		{
			const bool bTagged = MeshComponentTags.ContainsByPredicate([Mesh](const FName& Tag) { return Mesh->ComponentHasTag(Tag); });
			if (!bTagged)
			{
				continue;
			}
		}

		FNHCharacterEffectsMeshEntry Entry;
		Entry.Mesh = Mesh;

		const TArray<FName> SlotNames = Mesh->GetMaterialSlotNames();
		const int32 NumMaterials = Mesh->GetNumMaterials();

		for (int32 SlotIndex = 0; SlotIndex < NumMaterials; ++SlotIndex)
		{
			UMaterialInterface* Current = Mesh->GetMaterial(SlotIndex);
			if (!Current)
			{
				continue;
			}

			if (TargetMaterialSlots.Num() > 0)
			{
				if (!SlotNames.IsValidIndex(SlotIndex) || !TargetMaterialSlots.Contains(SlotNames[SlotIndex]))
				{
					continue;
				}
			}

			const bool bHasSweat = MaterialHasScalar(Current, SweatParamName);
			const bool bHasDamage = MaterialHasVector(Current, HitSphereParamNames[0]);
			if (!bHasSweat && !bHasDamage)
			{
				continue; // eyes, teeth, lashes, etc. never get a MID
			}

			FNHCharacterEffectsSlot Slot;
			Slot.SlotIndex = SlotIndex;
			Slot.OriginalMaterial = Current;
			Slot.bHasDamageParams = bHasDamage;

			if (UMaterialInstanceDynamic* ExistingMID = Cast<UMaterialInstanceDynamic>(Current))
			{
				// Someone else already made a MID for this slot: drive it in place.
				Slot.MID = ExistingMID;
				Slot.bFromPool = false;
			}
			else if (UNHCharacterEffectsMIDPool* PoolPtr = Pool.Get())
			{
				Slot.MID = PoolPtr->AcquireMID(Current);
				Slot.bFromPool = true;
			}
			else
			{
				Slot.MID = UMaterialInstanceDynamic::Create(Current, this);
				Slot.bFromPool = false;
			}

			if (!Slot.MID)
			{
				continue;
			}

			if (Slot.bFromPool || Slot.MID != Current)
			{
				Mesh->SetMaterial(SlotIndex, Slot.MID);
			}

			// Cache parameter indices once; per-tick writes then skip the name lookup entirely.
			if (bHasSweat)
			{
				Slot.MID->InitializeScalarParameterAndGetIndex(SweatParamName, Sweat, Slot.SweatParamIndex);
			}
			if (MaterialHasScalar(Current, WetnessParamName))
			{
				Slot.MID->InitializeScalarParameterAndGetIndex(WetnessParamName, Wetness, Slot.WetnessParamIndex);
			}

			Entry.Slots.Add(Slot);
		}

		if (Entry.Slots.Num() > 0)
		{
			MeshEntries.Add(MoveTemp(Entry));
		}
	}

	bInitialized = MeshEntries.Num() > 0;

	if (bInitialized)
	{
		// Make sure hit records have one centre per driven mesh.
		for (FNHCharacterHitRecord& Hit : Hits)
		{
			Hit.PreSkinnedCentres.Init(NHCharacterEffects::FarAwayCentre, MeshEntries.Num());
		}
		PushScalars(true);
		PushAllHits();
		UpdateTickSignificance();
	}
	SetComponentTickEnabled(bInitialized);
}

void UNHCharacterEffectsComponent::ReleaseEffects(bool bRestoreOriginalMaterials)
{
	UNHCharacterEffectsMIDPool* PoolPtr = Pool.Get();

	for (FNHCharacterEffectsMeshEntry& Entry : MeshEntries)
	{
		USkeletalMeshComponent* Mesh = Entry.Mesh.Get();
		for (FNHCharacterEffectsSlot& Slot : Entry.Slots)
		{
			if (Mesh && bRestoreOriginalMaterials && Slot.MID != Slot.OriginalMaterial)
			{
				Mesh->SetMaterial(Slot.SlotIndex, Slot.OriginalMaterial);
			}

			if (Slot.bFromPool && PoolPtr)
			{
				PoolPtr->ReleaseMID(Slot.MID);
			}
			Slot.MID = nullptr;
		}
	}

	MeshEntries.Reset();
	bInitialized = false;
	SetComponentTickEnabled(false);
}

void UNHCharacterEffectsComponent::ResetState()
{
	Stamina = 1.f;
	Exertion = 0.f;
	Sweat = 0.f;
	Wetness = 0.f;
	ClearDamage();
	PushScalars(true);
}

/* ---- Tick -------------------------------------------------------------------------------- */

void UNHCharacterEffectsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bInitialized)
	{
		return;
	}

	// With a tick interval, DeltaTime is the real time since our last tick.
	SignificanceTimer -= DeltaTime;
	if (SignificanceTimer <= 0.f)
	{
		SignificanceTimer = NHCharacterEffects::SignificanceInterval;
		UpdateTickSignificance();
	}

	float Temperature, Humidity, Rain;
	ReadWeather(Temperature, Humidity, Rain);

	// Simulation is a handful of floats: always run it so state is correct when the character
	// comes back into view. GPU writes are what we skip when nobody can see the character.
	UpdateExertion(DeltaTime, Temperature, Humidity, Rain);
	const bool bHitsChanged = UpdateHitAging(DeltaTime);

	if (!IsAnyMeshRendered())
	{
		return;
	}

	PushScalars(false);

	if (bHitsChanged || DirtyHitMask != 0)
	{
		for (int32 i = 0; i < NH_CHARACTER_EFFECTS_MAX_HITS; ++i)
		{
			if (DirtyHitMask & (1u << i))
			{
				PushHitSlot(i);
			}
		}
		DirtyHitMask = 0;
	}
}

void UNHCharacterEffectsComponent::UpdateTickSignificance()
{
	const UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	float Interval = NearTickInterval;

	const APlayerController* PC = World->GetFirstPlayerController();
	if (PC && PC->PlayerCameraManager)
	{
		const float DistSq = FVector::DistSquared(PC->PlayerCameraManager->GetCameraLocation(), Owner->GetActorLocation());
		if (DistSq > FMath::Square(FarDistance))
		{
			Interval = FarTickInterval;
		}
		else if (DistSq > FMath::Square(NearDistance))
		{
			Interval = MidTickInterval;
		}
	}

	if (!IsAnyMeshRendered())
	{
		Interval = FMath::Max(Interval, MidTickInterval);
	}

	SetComponentTickInterval(Interval);
}

bool UNHCharacterEffectsComponent::IsAnyMeshRendered() const
{
	for (const FNHCharacterEffectsMeshEntry& Entry : MeshEntries)
	{
		const USkeletalMeshComponent* Mesh = Entry.Mesh.Get();
		if (Mesh && Mesh->WasRecentlyRendered(0.25f))
		{
			return true;
		}
	}
	return false;
}

/* ---- Weather & exertion ------------------------------------------------------------------ */

void UNHCharacterEffectsComponent::ReadWeather(float& OutTemperature, float& OutHumidity, float& OutRain)
{
	OutTemperature = FallbackTemperature;
	OutHumidity = FallbackHumidity;
	OutRain = 0.f;

	if (UMaterialParameterCollectionInstance* Weather = CachedWeatherInstance.Get())
	{
		Weather->GetScalarParameterValue(TemperatureParam, OutTemperature);
		Weather->GetScalarParameterValue(HumidityParam, OutHumidity);
		Weather->GetScalarParameterValue(RainParam, OutRain);
	}

	OutHumidity = FMath::Clamp(OutHumidity, 0.f, 1.f);
	OutRain = FMath::Clamp(OutRain, 0.f, 1.f);
}

bool UNHCharacterEffectsComponent::IsSheltered(float DeltaTime)
{
	if (!bCheckRainShelter)
	{
		return false;
	}

	ShelterTimer -= DeltaTime;
	if (ShelterTimer > 0.f)
	{
		return bShelteredCached;
	}
	ShelterTimer = NHCharacterEffects::ShelterCheckInterval;

	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return bShelteredCached = false;
	}

	static const FName ShelterTraceTag(TEXT("CharacterEffectsShelter"));
	FCollisionQueryParams Params(ShelterTraceTag, /*bTraceComplex*/ false, Owner);
	const FVector Start = Owner->GetActorLocation() + FVector(0.f, 0.f, 90.f);
	const FVector End = Start + FVector(0.f, 0.f, 3000.f);

	FHitResult Hit;
	bShelteredCached = World->LineTraceSingleByChannel(Hit, Start, End, ShelterTraceChannel, Params);
	return bShelteredCached;
}

void UNHCharacterEffectsComponent::UpdateExertion(float DeltaTime, float Temperature, float Humidity, float Rain)
{
	const AActor* Owner = GetOwner();
	const float Speed = Owner ? Owner->GetVelocity().Size2D() : 0.f;

	// 0 at rest, 1 at full sprint.
	const float Effort = FMath::Clamp(FMath::GetRangePct(RestSpeed, SprintSpeed, Speed), 0.f, 1.f);

	if (bSimulateStamina)
	{
		if (Effort > 0.1f)
		{
			Stamina -= StaminaDrainPerSecond * Effort * DeltaTime;
		}
		else
		{
			Stamina += StaminaRegenPerSecond * DeltaTime;
		}
		Stamina = FMath::Clamp(Stamina, 0.f, 1.f);
	}

	// How hot and humid it is: 0 below 20 C, 1 at 35 C, amplified by humidity.
	const float Heat01 = FMath::Clamp((Temperature - 20.f) / 15.f, 0.f, 1.f);
	const float Climate = FMath::Clamp(Heat01 * FMath::Lerp(0.6f, 1.25f, Humidity), 0.f, 1.f);

	// Body heat follows effort and fatigue, and cools slower in hot weather.
	const float ExertionTarget = FMath::Max(Effort, 1.f - Stamina);
	const float CoolRate = CoolDownRate * FMath::Lerp(1.5f, 0.5f, Climate);
	Exertion = NHCharacterEffects::Approach(Exertion, ExertionTarget, HeatUpRate, CoolRate, DeltaTime);

	// Lagos baseline sheen even at rest, plus exertion scaled by climate.
	const float SweatTarget = FMath::Clamp(Climate * 0.25f + Exertion * (0.4f + 0.6f * Climate), 0.f, 1.f);
	const float DryRate = SweatDryRate * FMath::Lerp(1.5f, 0.4f, Humidity);
	Sweat = NHCharacterEffects::Approach(Sweat, SweatTarget, SweatBuildRate, DryRate, DeltaTime);

	// Rain wetness, unless under a roof or canopy.
	const float WetTarget = (Rain > 0.f && !IsSheltered(DeltaTime)) ? Rain : 0.f;
	const float WetDry = WetnessDryRate * FMath::Lerp(1.5f, 0.5f, Humidity) * FMath::Lerp(0.6f, 1.4f, Heat01);
	Wetness = NHCharacterEffects::Approach(Wetness, WetTarget, WetnessBuildRate, WetDry, DeltaTime);
}

void UNHCharacterEffectsComponent::PushScalars(bool bForce)
{
	const bool bPushSweat = bForce || FMath::Abs(Sweat - LastPushedSweat) > ParameterEpsilon;
	const bool bPushWet = bForce || FMath::Abs(Wetness - LastPushedWetness) > ParameterEpsilon;
	if (!bPushSweat && !bPushWet)
	{
		return;
	}

	for (FNHCharacterEffectsMeshEntry& Entry : MeshEntries)
	{
		for (FNHCharacterEffectsSlot& Slot : Entry.Slots)
		{
			if (!Slot.MID)
			{
				continue;
			}
			if (bPushSweat && Slot.SweatParamIndex != INDEX_NONE)
			{
				Slot.MID->SetScalarParameterByIndex(Slot.SweatParamIndex, Sweat);
			}
			if (bPushWet && Slot.WetnessParamIndex != INDEX_NONE)
			{
				Slot.MID->SetScalarParameterByIndex(Slot.WetnessParamIndex, Wetness);
			}
		}
	}

	if (bPushSweat)
	{
		LastPushedSweat = Sweat;
	}
	if (bPushWet)
	{
		LastPushedWetness = Wetness;
	}
}

/* ---- Damage ------------------------------------------------------------------------------ */

bool UNHCharacterEffectsComponent::ComputePreSkinnedCentre(const USkeletalMeshComponent* Mesh, FName BoneName,
	const FVector& WorldLocation, FVector3f& OutCentre) const
{
	if (!Mesh || !Mesh->GetSkinnedAsset() || BoneName.IsNone())
	{
		return false;
	}

	const int32 BoneIndex = Mesh->GetBoneIndex(BoneName);
	if (BoneIndex == INDEX_NONE)
	{
		return false; // e.g. the face mesh has no leg bones
	}

	// World -> current bone space (works for leader-pose followers too) -> reference-pose mesh space,
	// which is exactly what the material's PreSkinnedPosition node returns.
	const FTransform BoneWorld = Mesh->GetBoneTransform(BoneIndex);
	const FVector LocalToBone = BoneWorld.InverseTransformPosition(WorldLocation);

	const FTransform RefPoseCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(
		Mesh->GetSkinnedAsset()->GetRefSkeleton(), BoneIndex);

	OutCentre = FVector3f(RefPoseCS.TransformPosition(LocalToBone));
	return true;
}

void UNHCharacterEffectsComponent::ApplyHit(FVector WorldLocation, FName BoneName, ENHCharacterHitType HitType, float Strength)
{
	if (!bInitialized || MeshEntries.Num() == 0)
	{
		return;
	}

	Strength = FMath::Clamp(Strength, 0.f, 1.f);
	if (Strength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (BoneName.IsNone())
	{
		if (const USkeletalMeshComponent* FirstMesh = MeshEntries[0].Mesh.Get())
		{
			BoneName = FirstMesh->FindClosestBone(WorldLocation);
		}
		if (BoneName.IsNone())
		{
			return;
		}
	}

	// Centres for every driven mesh.
	TArray<FVector3f, TInlineAllocator<6>> Centres;
	Centres.SetNumUninitialized(MeshEntries.Num());
	bool bAnyMeshHasBone = false;
	for (int32 e = 0; e < MeshEntries.Num(); ++e)
	{
		FVector3f Centre;
		if (ComputePreSkinnedCentre(MeshEntries[e].Mesh.Get(), BoneName, WorldLocation, Centre))
		{
			Centres[e] = Centre;
			bAnyMeshHasBone = true;
		}
		else
		{
			Centres[e] = NHCharacterEffects::FarAwayCentre;
		}
	}
	if (!bAnyMeshHasBone)
	{
		return;
	}

	// Find a reference centre for merge tests (first mesh that has the bone).
	int32 RefEntry = 0;
	while (RefEntry < Centres.Num() && Centres[RefEntry] == NHCharacterEffects::FarAwayCentre)
	{
		++RefEntry;
	}

	// 1) Merge with a nearby hit of the same type.
	const float MergeDistSq = FMath::Square(MergeDistance);
	for (int32 i = 0; i < NH_CHARACTER_EFFECTS_MAX_HITS; ++i)
	{
		FNHCharacterHitRecord& Existing = Hits[i];
		if (!Existing.IsActive() || Existing.Type != HitType || !Existing.PreSkinnedCentres.IsValidIndex(RefEntry))
		{
			continue;
		}
		if (FVector3f::DistSquared(Existing.PreSkinnedCentres[RefEntry], Centres[RefEntry]) <= MergeDistSq)
		{
			Existing.Strength = FMath::Min(1.f, Existing.Strength + Strength * 0.5f);
			Existing.Radius = FMath::Min(Existing.Radius * 1.15f, (HitType == ENHCharacterHitType::Blunt ? BluntBruiseRadius : BulletWoundRadius) * 2.f);
			if (HitType == ENHCharacterHitType::Blunt)
			{
				Existing.Age = FMath::Min(Existing.Age, BruiseDevelopTime); // re-struck: stop healing
			}
			DirtyHitMask |= (1u << i);
			if (IsAnyMeshRendered())
			{
				PushHitSlot(i);
				DirtyHitMask &= ~(1u << i);
			}
			return;
		}
	}

	// 2) Pick a slot: a free one, else the least important existing mark.
	int32 Chosen = INDEX_NONE;
	float LowestScore = TNumericLimits<float>::Max();
	for (int32 i = 0; i < NH_CHARACTER_EFFECTS_MAX_HITS; ++i)
	{
		const FNHCharacterHitRecord& Existing = Hits[i];
		if (!Existing.IsActive())
		{
			Chosen = i;
			break;
		}
		// Bullet wounds are worth keeping more than bruises; older marks are cheaper to recycle.
		const float TypeWeight = Existing.Type == ENHCharacterHitType::Bullet ? 2.f : 1.f;
		const float Score = GetHitVisualIntensity(Existing) * TypeWeight - Existing.Age * 0.001f;
		if (Score < LowestScore)
		{
			LowestScore = Score;
			Chosen = i;
		}
	}

	FNHCharacterHitRecord& Hit = Hits[Chosen];
	Hit.Type = HitType;
	Hit.BoneName = BoneName;
	Hit.Radius = HitType == ENHCharacterHitType::Blunt ? BluntBruiseRadius : BulletWoundRadius;
	Hit.Strength = Strength;
	Hit.Age = 0.f;
	Hit.Seed = FMath::FRand();
	Hit.PreSkinnedCentres = Centres;

	DirtyHitMask |= (1u << Chosen);
	if (IsAnyMeshRendered())
	{
		PushHitSlot(Chosen);
		DirtyHitMask &= ~(1u << Chosen);
	}
}

float UNHCharacterEffectsComponent::GetHitVisualIntensity(const FNHCharacterHitRecord& Hit) const
{
	if (!Hit.IsActive())
	{
		return 0.f;
	}
	if (Hit.Type == ENHCharacterHitType::Bullet)
	{
		return Hit.Strength;
	}

	// Bruise: immediate redness, darkens over BruiseDevelopTime, then heals over BruiseHealTime.
	const float Develop = BruiseDevelopTime > 0.f ? FMath::SmoothStep(0.f, 1.f, Hit.Age / BruiseDevelopTime) : 1.f;
	float Heal = 1.f;
	if (BruiseHealTime > 0.f && Hit.Age > BruiseDevelopTime)
	{
		Heal = 1.f - FMath::Clamp((Hit.Age - BruiseDevelopTime) / BruiseHealTime, 0.f, 1.f);
	}
	return Hit.Strength * FMath::Lerp(0.3f, 1.f, Develop) * Heal;
}

bool UNHCharacterEffectsComponent::UpdateHitAging(float DeltaTime)
{
	bool bAnyChanged = false;
	for (int32 i = 0; i < NH_CHARACTER_EFFECTS_MAX_HITS; ++i)
	{
		FNHCharacterHitRecord& Hit = Hits[i];
		if (!Hit.IsActive())
		{
			continue;
		}

		Hit.Age += DeltaTime;
		if (Hit.Type == ENHCharacterHitType::Bullet)
		{
			continue; // wounds don't change on their own
		}

		const float Intensity = GetHitVisualIntensity(Hit);
		if (Intensity <= KINDA_SMALL_NUMBER)
		{
			Hit.Strength = 0.f; // fully healed: free the slot
			DirtyHitMask |= (1u << i);
			bAnyChanged = true;
		}
		else if (FMath::Abs(Intensity - LastPushedHitIntensity[i]) > ParameterEpsilon * 2.f)
		{
			DirtyHitMask |= (1u << i);
			bAnyChanged = true;
		}
	}
	return bAnyChanged;
}

void UNHCharacterEffectsComponent::PushHitSlot(int32 HitIndex)
{
	const FNHCharacterHitRecord& Hit = Hits[HitIndex];
	const float Intensity = GetHitVisualIntensity(Hit);
	const float LifeSpan = FMath::Max(BruiseDevelopTime + BruiseHealTime, 1.f);
	const float Age01 = FMath::Clamp(Hit.Age / LifeSpan, 0.f, 1.f);

	const FLinearColor Data(
		Hit.Type == ENHCharacterHitType::Bullet ? Intensity : 0.f,
		Hit.Type == ENHCharacterHitType::Blunt ? Intensity : 0.f,
		Age01,
		Hit.Seed);

	for (int32 e = 0; e < MeshEntries.Num(); ++e)
	{
		const bool bActive = Intensity > KINDA_SMALL_NUMBER && Hit.PreSkinnedCentres.IsValidIndex(e);
		const FVector3f Centre = bActive ? Hit.PreSkinnedCentres[e] : NHCharacterEffects::FarAwayCentre;
		const FLinearColor Sphere(Centre.X, Centre.Y, Centre.Z, bActive ? Hit.Radius : 0.f);

		for (FNHCharacterEffectsSlot& Slot : MeshEntries[e].Slots)
		{
			if (Slot.MID && Slot.bHasDamageParams)
			{
				Slot.MID->SetVectorParameterValue(HitSphereParamNames[HitIndex], Sphere);
				Slot.MID->SetVectorParameterValue(HitDataParamNames[HitIndex], Data);
			}
		}
	}

	LastPushedHitIntensity[HitIndex] = Intensity;
}

void UNHCharacterEffectsComponent::PushAllHits()
{
	for (int32 i = 0; i < NH_CHARACTER_EFFECTS_MAX_HITS; ++i)
	{
		PushHitSlot(i);
	}
	DirtyHitMask = 0;
}

void UNHCharacterEffectsComponent::ClearDamage()
{
	for (FNHCharacterHitRecord& Hit : Hits)
	{
		Hit.Strength = 0.f;
		Hit.Age = 0.f;
	}
	if (bInitialized)
	{
		PushAllHits();
	}
}

TArray<FNHCharacterHitRecord> UNHCharacterEffectsComponent::GetActiveHits() const
{
	TArray<FNHCharacterHitRecord> Result;
	for (const FNHCharacterHitRecord& Hit : Hits)
	{
		if (Hit.IsActive())
		{
			Result.Add(Hit);
		}
	}
	return Result;
}

void UNHCharacterEffectsComponent::HandlePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy,
	FVector HitLocation, UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection,
	const UDamageType* DamageType, AActor* DamageCauser)
{
	ENHCharacterHitType Type = ENHCharacterHitType::Bullet;
	if (DamageType)
	{
		for (const TSubclassOf<UDamageType>& BluntClass : BluntDamageTypes)
		{
			if (BluntClass && DamageType->IsA(BluntClass))
			{
				Type = ENHCharacterHitType::Blunt;
				break;
			}
		}
	}

	// Map damage to visual strength: ~40 damage reads as a full-strength mark.
	const float Strength = FMath::Clamp(Damage / 40.f, 0.25f, 1.f);
	ApplyHit(HitLocation, BoneName, Type, Strength);
}
