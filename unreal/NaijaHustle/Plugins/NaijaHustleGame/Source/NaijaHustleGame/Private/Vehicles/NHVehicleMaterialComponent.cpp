// NHVehicleMaterialComponent.cpp

#include "Vehicles/NHVehicleMaterialComponent.h"

#include "Characters/NHCharacterEffectsComponent.h" // the per-world dynamic material pool
#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

static_assert(NH_VEHICLE_MAX_HITS <= 32, "DirtyHits is a uint32");

namespace NHVehicleMaterial
{
	static const FName Rain(TEXT("RainIntensity"));
	static const FName Wetness(TEXT("Wetness"));
	static const FName Glass(TEXT("Glass"));
	static const FName PaintColor(TEXT("PaintColor"));
	static const FName Metallic(TEXT("Metallic"));
	static const FName BaseRoughness(TEXT("BaseRoughness"));
	static const FName ClearCoat(TEXT("ClearCoat"));
	static const FName ClearCoatRoughness(TEXT("ClearCoatRoughness"));
	static const FName FlakeIntensity(TEXT("FlakeIntensity"));

	static constexpr float ShelterCheckInterval = 0.5f;

	static bool HasScalar(const UMaterialInterface* Material, FName Param, float* OutValue = nullptr)
	{
		float Value = 0.f;
		const bool bFound = Material && Material->GetScalarParameterValue(FHashedMaterialParameterInfo(FMaterialParameterInfo(Param)), Value);
		if (bFound && OutValue)
		{
			*OutValue = Value;
		}
		return bFound;
	}

	static bool HasVector(const UMaterialInterface* Material, FName Param)
	{
		FLinearColor Value;
		return Material && Material->GetVectorParameterValue(FHashedMaterialParameterInfo(FMaterialParameterInfo(Param)), Value);
	}
}

UNHVehicleMaterialComponent::UNHVehicleMaterialComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // switched on only when a material with the contract is found
	PrimaryComponentTick.TickInterval = 0.5f;

	for (int32 i = 0; i < NH_VEHICLE_MAX_HITS; ++i)
	{
		HitSphereNames[i] = FName(*FString::Printf(TEXT("DamageHit_%d_Sphere"), i));
		HitDataNames[i] = FName(*FString::Printf(TEXT("DamageHit_%d_Data"), i));
	}
}

void UNHVehicleMaterialComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeEffects();

	if (bAutoBindActorHit)
	{
		if (AActor* Owner = GetOwner())
		{
			Owner->OnActorHit.AddUniqueDynamic(this, &UNHVehicleMaterialComponent::HandleActorHit);
		}
	}
}

void UNHVehicleMaterialComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* Owner = GetOwner())
	{
		Owner->OnActorHit.RemoveDynamic(this, &UNHVehicleMaterialComponent::HandleActorHit);
	}
	ReleaseEffects(EndPlayReason == EEndPlayReason::RemovedFromWorld);
	Super::EndPlay(EndPlayReason);
}

/* ---- Setup ------------------------------------------------------------------------------- */

void UNHVehicleMaterialComponent::InitializeEffects()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}
	if (bInitialized)
	{
		ReleaseEffects(true); // the body was swapped: start again from the materials that are on it now
	}

	Pool = World->GetSubsystem<UNHCharacterEffectsMIDPool>();
	if (!WeatherCollection)
	{
		WeatherCollection = TSoftObjectPtr<UMaterialParameterCollection>(FSoftObjectPath(TEXT("/Game/NaijaHustle/Lighting/Presets/MPC_NHWeather.MPC_NHWeather"))).LoadSynchronous();
	}
	WeatherInstance = WeatherCollection ? World->GetParameterCollectionInstance(WeatherCollection) : nullptr;

	TInlineComponentArray<UMeshComponent*> Found(Owner);
	Meshes.Reset(Found.Num());
	for (UMeshComponent* Mesh : Found)
	{
		if (!Mesh)
		{
			continue;
		}
		if (MeshComponentTags.Num() > 0 && !MeshComponentTags.ContainsByPredicate([Mesh](const FName& Tag) { return Mesh->ComponentHasTag(Tag); }))
		{
			continue;
		}

		FNHVehicleMaterialMesh Entry;
		Entry.Mesh = Mesh;
		const TArray<FName> SlotNames = Mesh->GetMaterialSlotNames();
		const int32 NumMaterials = Mesh->GetNumMaterials();
		for (int32 SlotIndex = 0; SlotIndex < NumMaterials; ++SlotIndex)
		{
			UMaterialInterface* Current = Mesh->GetMaterial(SlotIndex);
			const bool bRain = NHVehicleMaterial::HasScalar(Current, NHVehicleMaterial::Rain);
			const bool bPaint = NHVehicleMaterial::HasScalar(Current, NHVehicleMaterial::ClearCoat);
			const bool bDamage = NHVehicleMaterial::HasVector(Current, HitSphereNames[0]);
			if (!bRain && !bPaint && !bDamage)
			{
				continue; // tyres, lights, interior: never given a dynamic instance
			}

			FNHVehicleMaterialSlot Slot;
			Slot.SlotIndex = SlotIndex;
			Slot.OriginalMaterial = Current;
			Slot.bHasPaintParams = bPaint;
			Slot.bHasDamageParams = bDamage;

			float GlassValue = 0.f;
			const FString SlotName = SlotNames.IsValidIndex(SlotIndex) ? SlotNames[SlotIndex].ToString() : FString();
			Slot.bGlass = (NHVehicleMaterial::HasScalar(Current, NHVehicleMaterial::Glass, &GlassValue) && GlassValue > 0.5f)
				|| SlotName.Contains(TEXT("glass")) || SlotName.Contains(TEXT("window")) || SlotName.Contains(TEXT("windscreen")) || SlotName.Contains(TEXT("windshield"));

			if (UMaterialInstanceDynamic* Existing = Cast<UMaterialInstanceDynamic>(Current))
			{
				Slot.MID = Existing; // somebody else's: drive it where it is
			}
			else if (UNHCharacterEffectsMIDPool* PoolPtr = Pool.Get())
			{
				Slot.MID = PoolPtr->AcquireMID(Current);
				Slot.bFromPool = true;
			}
			else
			{
				Slot.MID = UMaterialInstanceDynamic::Create(Current, this);
			}
			if (!Slot.MID)
			{
				continue;
			}
			if (Slot.MID != Current)
			{
				Mesh->SetMaterial(SlotIndex, Slot.MID);
			}
			if (bRain)
			{
				Slot.MID->InitializeScalarParameterAndGetIndex(NHVehicleMaterial::Rain, RainOnCar, Slot.RainParamIndex);
			}
			if (NHVehicleMaterial::HasScalar(Current, NHVehicleMaterial::Wetness))
			{
				Slot.MID->InitializeScalarParameterAndGetIndex(NHVehicleMaterial::Wetness, Wetness, Slot.WetnessParamIndex);
			}
			Entry.Slots.Add(Slot);
		}
		if (Entry.Slots.Num() > 0)
		{
			Meshes.Add(MoveTemp(Entry));
		}
	}

	bInitialized = Meshes.Num() > 0;
	if (bInitialized)
	{
		PushWeather(true);
		PushAllHits();
		if (bPaintSet)
		{
			PushPaint();
		}
	}
	SetComponentTickEnabled(bInitialized);
}

void UNHVehicleMaterialComponent::ReleaseEffects(bool bRestoreOriginalMaterials)
{
	UNHCharacterEffectsMIDPool* PoolPtr = Pool.Get();
	for (FNHVehicleMaterialMesh& Entry : Meshes)
	{
		UMeshComponent* Mesh = Entry.Mesh.Get();
		for (FNHVehicleMaterialSlot& Slot : Entry.Slots)
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
	Meshes.Reset();
	bInitialized = false;
	SetComponentTickEnabled(false);
}

int32 UNHVehicleMaterialComponent::GetDrivenSlotCount() const
{
	int32 Count = 0;
	for (const FNHVehicleMaterialMesh& Entry : Meshes)
	{
		Count += Entry.Slots.Num();
	}
	return Count;
}

bool UNHVehicleMaterialComponent::IsAnyMeshRendered() const
{
	if (!bOnlyUpdateWhenRendered)
	{
		return true;
	}
	for (const FNHVehicleMaterialMesh& Entry : Meshes)
	{
		const UMeshComponent* Mesh = Entry.Mesh.Get();
		if (Mesh && Mesh->WasRecentlyRendered(0.25f))
		{
			return true;
		}
	}
	return false;
}

/* ---- Tick: weather ----------------------------------------------------------------------- */

float UNHVehicleMaterialComponent::ReadRain() const
{
	float Rain = 0.f;
	if (UMaterialParameterCollectionInstance* Weather = WeatherInstance.Get())
	{
		Weather->GetScalarParameterValue(RainParam, Rain);
	}
	return FMath::Clamp(Rain, 0.f, 1.f);
}

bool UNHVehicleMaterialComponent::IsSheltered(float DeltaTime)
{
	if (!bCheckRainShelter)
	{
		return false;
	}
	ShelterTimer -= DeltaTime;
	if (ShelterTimer > 0.f)
	{
		return bSheltered;
	}
	ShelterTimer = NHVehicleMaterial::ShelterCheckInterval;

	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return bSheltered = false;
	}
	static const FName TraceTag(TEXT("NHVehicleShelter"));
	const FCollisionQueryParams Params(TraceTag, /*bTraceComplex*/ false, Owner);
	FVector Origin, Extent;
	Owner->GetActorBounds(/*bOnlyCollidingComponents*/ true, Origin, Extent);
	const FVector Start = Origin + FVector(0.f, 0.f, Extent.Z + 20.f); // from the roof up
	FHitResult Hit;
	bSheltered = World->LineTraceSingleByChannel(Hit, Start, Start + FVector(0.f, 0.f, 3000.f), ShelterTraceChannel, Params);
	return bSheltered;
}

void UNHVehicleMaterialComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bInitialized)
	{
		return;
	}

	// With a tick interval, DeltaTime is the real time since the last tick
	const float Rain = ReadRain();
	RainOnCar = (Rain > 0.f && !IsSheltered(DeltaTime)) ? Rain : 0.f;

	const AActor* Owner = GetOwner();
	const float Speed = Owner ? Owner->GetVelocity().Size() : 0.f;
	const float DryRate = WetnessDryRate * (1.f + 3.f * FMath::Clamp(Speed / FMath::Max(DryingAirSpeed, 1.f), 0.f, 1.f));
	Wetness = FMath::FInterpConstantTo(Wetness, RainOnCar, DeltaTime, RainOnCar > Wetness ? WetnessBuildRate : DryRate);

	// nothing falling and nothing left to dry: drop to the slow tick
	const bool bActive = Rain > 0.f || Wetness > 0.f || DirtyHits != 0;
	const float Want = bActive ? ActiveTickInterval : IdleTickInterval;
	if (!FMath::IsNearlyEqual(PrimaryComponentTick.TickInterval, Want))
	{
		SetComponentTickInterval(Want);
	}

	if (!IsAnyMeshRendered())
	{
		return; // the state is up to date; the materials catch up when the car is next seen
	}
	PushWeather(false);
	if (DirtyHits != 0)
	{
		for (int32 i = 0; i < NH_VEHICLE_MAX_HITS; ++i)
		{
			if (DirtyHits & (1u << i))
			{
				PushHit(i);
			}
		}
		DirtyHits = 0;
	}
}

void UNHVehicleMaterialComponent::PushWeather(bool bForce)
{
	const bool bPushRain = bForce || FMath::Abs(RainOnCar - LastPushedRain) > ParameterEpsilon;
	const bool bPushWet = bForce || FMath::Abs(Wetness - LastPushedWetness) > ParameterEpsilon;
	if (!bPushRain && !bPushWet)
	{
		return;
	}
	for (FNHVehicleMaterialMesh& Entry : Meshes)
	{
		for (FNHVehicleMaterialSlot& Slot : Entry.Slots)
		{
			if (!Slot.MID)
			{
				continue;
			}
			if (bPushRain && Slot.RainParamIndex != INDEX_NONE)
			{
				Slot.MID->SetScalarParameterByIndex(Slot.RainParamIndex, RainOnCar);
			}
			if (bPushWet && Slot.WetnessParamIndex != INDEX_NONE)
			{
				Slot.MID->SetScalarParameterByIndex(Slot.WetnessParamIndex, Wetness);
			}
		}
	}
	if (bPushRain)
	{
		LastPushedRain = RainOnCar;
	}
	if (bPushWet)
	{
		LastPushedWetness = Wetness;
	}
}

/* ---- Paint ------------------------------------------------------------------------------- */

void UNHVehicleMaterialComponent::SetPaint(const FNHVehiclePaint& NewPaint)
{
	Paint = NewPaint;
	bPaintSet = true;
	if (bInitialized)
	{
		PushPaint();
	}
}

void UNHVehicleMaterialComponent::PushPaint()
{
	for (FNHVehicleMaterialMesh& Entry : Meshes)
	{
		for (FNHVehicleMaterialSlot& Slot : Entry.Slots)
		{
			if (!Slot.MID || !Slot.bHasPaintParams || Slot.bGlass)
			{
				continue;
			}
			Slot.MID->SetVectorParameterValue(NHVehicleMaterial::PaintColor, Paint.Color);
			Slot.MID->SetScalarParameterValue(NHVehicleMaterial::Metallic, Paint.Metallic);
			Slot.MID->SetScalarParameterValue(NHVehicleMaterial::BaseRoughness, Paint.BaseRoughness);
			Slot.MID->SetScalarParameterValue(NHVehicleMaterial::ClearCoat, Paint.ClearCoat);
			Slot.MID->SetScalarParameterValue(NHVehicleMaterial::ClearCoatRoughness, Paint.ClearCoatRoughness);
			Slot.MID->SetScalarParameterValue(NHVehicleMaterial::FlakeIntensity, Paint.FlakeIntensity);
		}
	}
}

/* ---- Damage ------------------------------------------------------------------------------ */

void UNHVehicleMaterialComponent::ApplyImpact(FVector WorldLocation, float Strength)
{
	const AActor* Owner = GetOwner();
	Strength = FMath::Clamp(Strength, 0.f, 1.f);
	if (!Owner || Strength <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	// Marks are kept in the vehicle's own space, so they survive the car moving and the body being swapped
	const FVector Local = Owner->GetActorTransform().InverseTransformPosition(WorldLocation);
	const float Radius = ImpactRadius * FMath::Lerp(0.5f, 1.f, Strength);

	int32 Chosen = INDEX_NONE;
	for (int32 i = 0; i < NH_VEHICLE_MAX_HITS; ++i) // 1) the same dent, hit again
	{
		FNHVehicleHit& Existing = Hits[i];
		if (Existing.IsActive() && FVector::DistSquared(Existing.ActorLocation, Local) <= FMath::Square(MergeDistance))
		{
			Existing.Strength = FMath::Min(1.f, Existing.Strength + Strength * 0.5f);
			Existing.Radius = FMath::Min(FMath::Max(Existing.Radius, Radius) * 1.15f, ImpactRadius * 2.f);
			Chosen = i;
			break;
		}
	}
	if (Chosen == INDEX_NONE) // 2) a free slot, or the faintest old mark
	{
		float Weakest = TNumericLimits<float>::Max();
		for (int32 i = 0; i < NH_VEHICLE_MAX_HITS; ++i)
		{
			if (!Hits[i].IsActive())
			{
				Chosen = i;
				break;
			}
			if (Hits[i].Strength < Weakest)
			{
				Weakest = Hits[i].Strength;
				Chosen = i;
			}
		}
		FNHVehicleHit& Hit = Hits[Chosen];
		Hit.ActorLocation = Local;
		Hit.Radius = Radius;
		Hit.Strength = Strength;
		Hit.Seed = FMath::FRand();
	}

	if (bInitialized && IsAnyMeshRendered())
	{
		PushHit(Chosen);
	}
	else
	{
		DirtyHits |= (1u << Chosen);
	}
}

void UNHVehicleMaterialComponent::PushHit(int32 HitIndex)
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	const FNHVehicleHit& Hit = Hits[HitIndex];
	const bool bActive = Hit.IsActive();
	const FVector World = Owner->GetActorTransform().TransformPosition(Hit.ActorLocation);
	const FLinearColor Data(Hit.Strength, Hit.Strength >= GlassCrackThreshold ? Hit.Strength : 0.f, 0.f, Hit.Seed);

	for (FNHVehicleMaterialMesh& Entry : Meshes)
	{
		const UMeshComponent* Mesh = Entry.Mesh.Get();
		if (!Mesh)
		{
			continue;
		}
		// the material compares against local position, which is in the mesh's own unscaled units
		const FTransform& ToWorld = Mesh->GetComponentTransform();
		const FVector Centre = ToWorld.InverseTransformPosition(World);
		const float Scale = FMath::Max(ToWorld.GetScale3D().GetAbsMax(), KINDA_SMALL_NUMBER);
		const FLinearColor Sphere(Centre.X, Centre.Y, Centre.Z, bActive ? Hit.Radius / Scale : 0.f);
		for (FNHVehicleMaterialSlot& Slot : Entry.Slots)
		{
			if (Slot.MID && Slot.bHasDamageParams)
			{
				Slot.MID->SetVectorParameterValue(HitSphereNames[HitIndex], Sphere);
				Slot.MID->SetVectorParameterValue(HitDataNames[HitIndex], bActive ? Data : FLinearColor(0.f, 0.f, 0.f, 0.f));
			}
		}
	}
}

void UNHVehicleMaterialComponent::PushAllHits()
{
	for (int32 i = 0; i < NH_VEHICLE_MAX_HITS; ++i)
	{
		PushHit(i);
	}
	DirtyHits = 0;
}

void UNHVehicleMaterialComponent::ClearDamage()
{
	for (FNHVehicleHit& Hit : Hits)
	{
		Hit = FNHVehicleHit();
	}
	if (bInitialized)
	{
		PushAllHits();
	}
}

TArray<FNHVehicleHit> UNHVehicleMaterialComponent::GetActiveHits() const
{
	TArray<FNHVehicleHit> Result;
	for (const FNHVehicleHit& Hit : Hits)
	{
		if (Hit.IsActive())
		{
			Result.Add(Hit);
		}
	}
	return Result;
}

void UNHVehicleMaterialComponent::HandleActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
	const float Impulse = NormalImpulse.Size();
	if (Impulse < MinDamageImpulse)
	{
		return; // kerbs, speed bumps and gentle nudges leave the paint alone
	}
	ApplyImpact(Hit.ImpactPoint, FMath::GetMappedRangeValueClamped(FVector2D(MinDamageImpulse, FullDamageImpulse), FVector2D(0.2f, 1.f), Impulse));
}
