// NHVehicleMaterialComponent.h
//
// Dynamic paint, wetness and damage for vehicles, driven through pooled dynamic material instances:
//   * Wetness: follows the game's weather (MPC_NHWeather "Rain"); a roof overhead keeps the car dry, and a wet
//     car dries faster at speed. "RainIntensity" drives the animated ripples, "Wetness" the glossy wet surface.
//   * Clear coat paint: colour, metal flake and the two coat roughnesses, set per vehicle with SetPaint.
//   * Damage: impact points are stored in each mesh's own local space and passed as spheres, which the material
//     turns into scratched primer on paint and cracks on glass.
//
// ---------------------------------------------------------------------------------------------
// MATERIAL CONTRACT (M_NHCarPaint from Scripts/nh_car_paint.py implements it; any material may):
//   Scalar "RainIntensity"         0..1  rain falling on the car now: ripple strength
//   Scalar "Wetness"               0..1  water on the surface: lower roughness, slightly darker
//   Vector "PaintColor", Scalars "Metallic", "BaseRoughness", "ClearCoat", "ClearCoatRoughness", "FlakeIntensity"
//   Scalar "Glass"                 0 or 1, authored on the material instance: 1 = a damaged area cracks instead of scratching
//   Vector "DamageHit_<i>_Sphere"  xyz = centre in the mesh's LOCAL space, w = radius in local units
//   Vector "DamageHit_<i>_Data"    r = scratch strength, g = glass crack strength, b = unused, a = random seed
//   for i = 0 .. NH_VEHICLE_MAX_HITS-1
//
//   A slot is driven if its material has "RainIntensity", "ClearCoat" or "DamageHit_0_Sphere". Tyres, lights and
//   interiors without them are left alone and never get a dynamic instance.
// ---------------------------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NHVehicleMaterialComponent.generated.h"

class UMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMaterialParameterCollection;
class UMaterialParameterCollectionInstance;
class UNHCharacterEffectsMIDPool;

/** Must match the number of hit slots unrolled in the material. */
#define NH_VEHICLE_MAX_HITS 6

/** A two-layer paint job: metallic flake base under a glossy clear coat. */
USTRUCT(BlueprintType)
struct FNHVehiclePaint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint") FLinearColor Color = FLinearColor(0.35f, 0.02f, 0.02f);
	/** How metallic the base layer is (0 = solid paint, 1 = full metal flake) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint", meta = (ClampMin = "0", ClampMax = "1")) float Metallic = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint", meta = (ClampMin = "0", ClampMax = "1")) float BaseRoughness = 0.4f;
	/** Strength of the clear coat (0 = matt wrap) and how polished it is (low = showroom) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint", meta = (ClampMin = "0", ClampMax = "1")) float ClearCoat = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint", meta = (ClampMin = "0", ClampMax = "1")) float ClearCoatRoughness = 0.05f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint", meta = (ClampMin = "0", ClampMax = "1")) float FlakeIntensity = 0.25f;
};

/** One dent, scrape or cracked pane. */
USTRUCT(BlueprintType)
struct FNHVehicleHit
{
	GENERATED_BODY()

	/** Where it happened, relative to the vehicle (actor space, cm) */
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle Material") FVector ActorLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle Material") float Radius = 0.f;
	/** 0..1. 0 = slot unused. */
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle Material") float Strength = 0.f;
	float Seed = 0.f;

	bool IsActive() const { return Strength > KINDA_SMALL_NUMBER; }
};

USTRUCT()
struct FNHVehicleMaterialSlot
{
	GENERATED_BODY()

	int32 SlotIndex = INDEX_NONE;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> OriginalMaterial = nullptr;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> MID = nullptr;
	/** Cached indices for the per-tick scalars, so they are written without a name lookup */
	int32 RainParamIndex = INDEX_NONE;
	int32 WetnessParamIndex = INDEX_NONE;
	bool bHasPaintParams = false;
	bool bHasDamageParams = false;
	bool bGlass = false;
	/** False when the slot already held someone else's dynamic instance: it is driven in place and never pooled */
	bool bFromPool = false;
};

USTRUCT()
struct FNHVehicleMaterialMesh
{
	GENERATED_BODY()

	UPROPERTY(Transient) TWeakObjectPtr<UMeshComponent> Mesh;
	UPROPERTY(Transient) TArray<FNHVehicleMaterialSlot> Slots;
};

UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent))
class NAIJAHUSTLEGAME_API UNHVehicleMaterialComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNHVehicleMaterialComponent();

	// ---- Setup ------------------------------------------------------------------------------

	/** Only mesh components with one of these tags are driven. Empty = every static, instanced and skeletal mesh on the owner. */
	UPROPERTY(EditAnywhere, Category = "Vehicle Material|Setup") TArray<FName> MeshComponentTags;

	/** Collect the meshes and take dynamic instances from the pool. Called on BeginPlay; call again after swapping the body. */
	UFUNCTION(BlueprintCallable, Category = "Vehicle Material") void InitializeEffects();
	/** Give the dynamic instances back to the pool */
	UFUNCTION(BlueprintCallable, Category = "Vehicle Material") void ReleaseEffects(bool bRestoreOriginalMaterials = true);
	UFUNCTION(BlueprintPure, Category = "Vehicle Material") int32 GetDrivenSlotCount() const;

	// ---- Paint ------------------------------------------------------------------------------

	/** Repaint every paint slot (glass is left alone). Cheap, but meant for spawn and the paint shop, not every frame. */
	UFUNCTION(BlueprintCallable, Category = "Vehicle Material") void SetPaint(const FNHVehiclePaint& NewPaint);
	UFUNCTION(BlueprintPure, Category = "Vehicle Material") const FNHVehiclePaint& GetPaint() const { return Paint; }

	// ---- Weather ----------------------------------------------------------------------------

	/** Weather source; "Rain" (0..1) is read from it. Unset = the game's MPC_NHWeather. */
	UPROPERTY(EditAnywhere, Category = "Vehicle Material|Weather") TObjectPtr<UMaterialParameterCollection> WeatherCollection = nullptr;
	UPROPERTY(EditAnywhere, Category = "Vehicle Material|Weather") FName RainParam = TEXT("Rain");
	/** Trace upward for a roof, bridge or canopy before letting rain land on the car */
	UPROPERTY(EditAnywhere, Category = "Vehicle Material|Weather") bool bCheckRainShelter = true;
	UPROPERTY(EditAnywhere, Category = "Vehicle Material|Weather") TEnumAsByte<ECollisionChannel> ShelterTraceChannel = ECC_Visibility;
	/** Per second: how fast rain soaks the surface, and how fast it dries when the rain stops */
	UPROPERTY(EditAnywhere, Category = "Vehicle Material|Weather") float WetnessBuildRate = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Vehicle Material|Weather") float WetnessDryRate = 0.02f;
	/** Driving dries the car: at this speed (cm/s) it dries four times as fast */
	UPROPERTY(EditAnywhere, Category = "Vehicle Material|Weather") float DryingAirSpeed = 2500.f;

	UFUNCTION(BlueprintPure, Category = "Vehicle Material") float GetWetness() const { return Wetness; }
	UFUNCTION(BlueprintPure, Category = "Vehicle Material") float GetRainOnCar() const { return RainOnCar; }

	// ---- Damage -----------------------------------------------------------------------------

	/** Radius (cm) of a full-strength mark; weaker hits leave smaller ones */
	UPROPERTY(EditAnywhere, Category = "Vehicle Material|Damage") float ImpactRadius = 45.f;
	/** Two impacts closer than this (cm) become one bigger, deeper mark */
	UPROPERTY(EditAnywhere, Category = "Vehicle Material|Damage") float MergeDistance = 35.f;
	/** Glass only cracks from hits at least this strong */
	UPROPERTY(EditAnywhere, Category = "Vehicle Material|Damage", meta = (ClampMin = "0", ClampMax = "1")) float GlassCrackThreshold = 0.35f;
	/** For physics-simulated vehicles: listen to the owner's OnActorHit and turn impulses into marks */
	UPROPERTY(EditAnywhere, Category = "Vehicle Material|Damage") bool bAutoBindActorHit = false;
	/** The impulse (kg cm/s) that leaves a full-strength mark, and the smallest one that leaves any */
	UPROPERTY(EditAnywhere, Category = "Vehicle Material|Damage") float FullDamageImpulse = 900000.f;
	UPROPERTY(EditAnywhere, Category = "Vehicle Material|Damage") float MinDamageImpulse = 120000.f;

	/** Leave a mark at a world location. Safe to call for every contact of a long scrape: nearby hits merge. */
	UFUNCTION(BlueprintCallable, Category = "Vehicle Material") void ApplyImpact(FVector WorldLocation, float Strength = 1.f);
	/** The body shop: all marks gone */
	UFUNCTION(BlueprintCallable, Category = "Vehicle Material") void ClearDamage();
	UFUNCTION(BlueprintPure, Category = "Vehicle Material") TArray<FNHVehicleHit> GetActiveHits() const;

	// ---- Performance ------------------------------------------------------------------------

	/** Seconds between updates while the car is wet or it is raining, and while nothing is changing */
	UPROPERTY(EditAnywhere, Category = "Vehicle Material|Performance") float ActiveTickInterval = 0.1f;
	UPROPERTY(EditAnywhere, Category = "Vehicle Material|Performance") float IdleTickInterval = 0.5f;
	/** Scalar changes smaller than this are not sent to the material */
	UPROPERTY(EditAnywhere, Category = "Vehicle Material|Performance") float ParameterEpsilon = 0.005f;
	/** Skip material writes while no driven mesh is on screen (the state still updates) */
	UPROPERTY(EditAnywhere, Category = "Vehicle Material|Performance") bool bOnlyUpdateWhenRendered = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float ReadRain() const;
	bool IsSheltered(float DeltaTime);
	bool IsAnyMeshRendered() const;
	void PushWeather(bool bForce);
	void PushPaint();
	void PushHit(int32 HitIndex);
	void PushAllHits();

	UFUNCTION()
	void HandleActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(Transient) TArray<FNHVehicleMaterialMesh> Meshes;
	UPROPERTY(Transient) TObjectPtr<UMaterialParameterCollectionInstance> WeatherInstance = nullptr;
	UPROPERTY(Transient) TWeakObjectPtr<UNHCharacterEffectsMIDPool> Pool;

	UPROPERTY(EditAnywhere, Category = "Vehicle Material|Paint") FNHVehiclePaint Paint;
	/** False until SetPaint is called: materials keep the paint they were authored with */
	bool bPaintSet = false;

	FNHVehicleHit Hits[NH_VEHICLE_MAX_HITS];
	/** Bit i set = hit i changed while off screen and still has to be sent */
	uint32 DirtyHits = 0;

	float Wetness = 0.f;
	float RainOnCar = 0.f;
	float LastPushedWetness = -1.f;
	float LastPushedRain = -1.f;
	float ShelterTimer = 0.f;
	bool bSheltered = false;
	bool bInitialized = false;

	/** Made once, so no name is built while driving */
	FName HitSphereNames[NH_VEHICLE_MAX_HITS];
	FName HitDataNames[NH_VEHICLE_MAX_HITS];
};
