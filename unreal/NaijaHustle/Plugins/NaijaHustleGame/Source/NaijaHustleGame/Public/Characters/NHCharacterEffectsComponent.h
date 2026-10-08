// NHCharacterEffectsComponent.h
//
// Dynamic character state effects for MetaHuman-style skeletal meshes:
//   * SweatIntensity / Wetness driven by velocity, stamina, exertion and world weather.
//   * Pose-independent hit masks (bullet / blunt) evaluated in the material via PreSkinnedPosition.
//   * Pooled, cached UMaterialInstanceDynamic objects (UNHCharacterEffectsMIDPool) so spawning and
//     despawning crowds never allocates MIDs mid-game and never leaks them.
//
// ---------------------------------------------------------------------------------------------
// MATERIAL CONTRACT (add these to the MetaHuman body / face / clothing master materials):
//   Scalar  "SweatIntensity"            0..1   drives roughness down, specular/sheen up, sweat normal blend
//   Scalar  "Wetness"                   0..1   rain wetness (darken albedo, lower roughness)
//   Vector  "DamageHit_<i>_Sphere"      xyz = centre in PRE-SKINNED (reference pose) mesh space, w = radius (cm)
//   Vector  "DamageHit_<i>_Data"        r = bullet intensity, g = blunt intensity, b = age 0..1, a = seed
//   for i = 0 .. NH_CHARACTER_EFFECTS_MAX_HITS-1
//
//   In the material: Mask_i = 1 - smoothstep(Radius*0.6, Radius, distance(PreSkinnedPosition, Centre_i))
//   Because the centre lives in reference-pose space, the wound/bruise stays glued to the body through
//   any animation, with no UV-seam problems on MetaHuman texture atlases and no runtime vertex-buffer
//   rebuilds (vertex-colour painting on skinned meshes forces a buffer update per hit and MetaHumans
//   already use vertex colours for their own masks).
// ---------------------------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "NHCharacterEffectsComponent.generated.h"

class USkeletalMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMaterialParameterCollection;
class UMaterialParameterCollectionInstance;
class UDamageType;
class AController;

/** Must match the number of hit slots unrolled in the master material. */
#define NH_CHARACTER_EFFECTS_MAX_HITS 8

UENUM(BlueprintType)
enum class ENHCharacterHitType : uint8
{
	Bullet    UMETA(DisplayName = "Bullet / Penetrating"),
	Blunt    UMETA(DisplayName = "Blunt Force")
};

/* =============================================================================================
 *  MID POOL (one per world)
 * ============================================================================================= */

USTRUCT()
struct FNHCharacterEffectsMIDBucket
{
	GENERATED_BODY()

	/** MIDs that are free to hand out. Held by UPROPERTY so GC never collects them while pooled. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Free;

	/** Total MIDs ever created for this parent (in use + free). */
	int32 TotalCreated = 0;
};

/**
 * World-scoped pool of UMaterialInstanceDynamic objects keyed by parent material.
 * Pre-warm during loading screens so no MID is created during gameplay.
 */
UCLASS()
class NAIJAHUSTLEGAME_API UNHCharacterEffectsMIDPool : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Get a MID for Parent: reuses a pooled one, or creates one if the pool is empty. */
	UMaterialInstanceDynamic* AcquireMID(UMaterialInterface* Parent);

	/** Return a MID. Its parameter overrides are cleared so the next user starts clean. */
	void ReleaseMID(UMaterialInstanceDynamic* MID);

	/** Create Count MIDs for Parent ahead of time (call from a loading screen / level start). */
	UFUNCTION(BlueprintCallable, Category = "Character Effects|Pool")
	void PrewarmPool(UMaterialInterface* Parent, int32 Count);

	/** Hard cap on idle MIDs kept per parent; extras are dropped and garbage collected. */
	UPROPERTY(EditAnywhere, Category = "Character Effects|Pool")
	int32 MaxPooledPerParent = 128;

	UFUNCTION(BlueprintPure, Category = "Character Effects|Pool")
	int32 GetFreeCount(UMaterialInterface* Parent) const;

	virtual void Deinitialize() override;

private:
	static UMaterialInterface* ResolveBaseParent(UMaterialInterface* Material);

	UPROPERTY(Transient)
	TMap<TObjectPtr<UMaterialInterface>, FNHCharacterEffectsMIDBucket> Buckets;
};

/* =============================================================================================
 *  PER-CHARACTER STATE
 * ============================================================================================= */

/** One material slot on one mesh that this component drives. */
USTRUCT()
struct FNHCharacterEffectsSlot
{
	GENERATED_BODY()

	int32 SlotIndex = INDEX_NONE;

	/** Material that was on the slot before we took over (restored on release). */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OriginalMaterial = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID = nullptr;

	/** Cached parameter indices for the fast SetScalarParameterByIndex path. */
	int32 SweatParamIndex = INDEX_NONE;
	int32 WetnessParamIndex = INDEX_NONE;
	bool bHasDamageParams = false;

	/**
	 * False when the slot already held a MID created by someone else (e.g. the MetaHuman Blueprint's
	 * own runtime MIDs). We then drive that MID in place instead of replacing it, and never pool it.
	 */
	bool bFromPool = false;
};

USTRUCT()
struct FNHCharacterEffectsMeshEntry
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(Transient)
	TArray<FNHCharacterEffectsSlot> Slots;
};

/** A single wound / bruise, stored in reference-pose (pre-skinned) space per mesh. */
USTRUCT(BlueprintType)
struct FNHCharacterHitRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Character Effects")
	ENHCharacterHitType Type = ENHCharacterHitType::Bullet;

	UPROPERTY(BlueprintReadOnly, Category = "Character Effects")
	FName BoneName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Character Effects")
	float Radius = 3.f;

	/** 0..1 visual strength at full development. 0 = slot unused. */
	UPROPERTY(BlueprintReadOnly, Category = "Character Effects")
	float Strength = 0.f;

	/** Seconds since the hit. */
	UPROPERTY(BlueprintReadOnly, Category = "Character Effects")
	float Age = 0.f;

	/** Random value so the material can vary each wound's shape. */
	float Seed = 0.f;

	/** Pre-skinned centre, one per entry in MeshEntries (meshes have different reference poses). */
	TArray<FVector3f, TInlineAllocator<6>> PreSkinnedCentres;

	bool IsActive() const { return Strength > KINDA_SMALL_NUMBER; }
};

/* =============================================================================================
 *  COMPONENT
 * ============================================================================================= */

UCLASS(ClassGroup = (Character), meta = (BlueprintSpawnableComponent))
class NAIJAHUSTLEGAME_API UNHCharacterEffectsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNHCharacterEffectsComponent();

	// ---- Setup ------------------------------------------------------------------------------

	/**
	 * Only skeletal meshes with one of these component tags are driven. Leave empty to drive every
	 * USkeletalMeshComponent on the owner (MetaHuman Body, Face, Torso, Legs, Feet).
	 */
	UPROPERTY(EditAnywhere, Category = "Character Effects|Setup")
	TArray<FName> MeshComponentTags;

	/**
	 * Only these material slot names are driven. Leave empty to auto-detect: any slot whose material
	 * exposes "SweatIntensity" or "DamageHit_0_Sphere" is driven, everything else (eyes, teeth,
	 * lashes) is left untouched and never gets a MID.
	 */
	UPROPERTY(EditAnywhere, Category = "Character Effects|Setup")
	TArray<FName> TargetMaterialSlots;

	/** Collect meshes and take MIDs from the pool. Called automatically on BeginPlay. */
	UFUNCTION(BlueprintCallable, Category = "Character Effects")
	void InitializeEffects();

	/** Give MIDs back to the pool. bRestoreOriginalMaterials puts the authored materials back. */
	UFUNCTION(BlueprintCallable, Category = "Character Effects")
	void ReleaseEffects(bool bRestoreOriginalMaterials = true);

	/** Clear all wounds, sweat and wetness, e.g. on respawn or outfit change. */
	UFUNCTION(BlueprintCallable, Category = "Character Effects")
	void ResetState();

	// ---- Weather ----------------------------------------------------------------------------

	/**
	 * Global weather. Expected scalar parameters: "Temperature" (deg C), "Humidity" (0..1) and "Rain" (0..1).
	 * If unset, the game's MPC_NHWeather is used (made by Scripts/nh_blockout_materials.py and driven by
	 * ANHLightingRig, so sweat and wetness follow the lighting preset). If that is missing too, or a parameter
	 * is, the fallback values below apply.
	 */
	UPROPERTY(EditAnywhere, Category = "Character Effects|Weather")
	TObjectPtr<UMaterialParameterCollection> WeatherCollection = nullptr;

	UPROPERTY(EditAnywhere, Category = "Character Effects|Weather")
	FName TemperatureParam = TEXT("Temperature");

	UPROPERTY(EditAnywhere, Category = "Character Effects|Weather")
	FName HumidityParam = TEXT("Humidity");

	UPROPERTY(EditAnywhere, Category = "Character Effects|Weather")
	FName RainParam = TEXT("Rain");

	/** Lagos defaults: hot and humid. */
	UPROPERTY(EditAnywhere, Category = "Character Effects|Weather")
	float FallbackTemperature = 31.f;

	UPROPERTY(EditAnywhere, Category = "Character Effects|Weather", meta = (ClampMin = 0, ClampMax = 1))
	float FallbackHumidity = 0.8f;

	/** Trace upward to check for a roof / canopy before applying rain wetness. */
	UPROPERTY(EditAnywhere, Category = "Character Effects|Weather")
	bool bCheckRainShelter = true;

	UPROPERTY(EditAnywhere, Category = "Character Effects|Weather")
	TEnumAsByte<ECollisionChannel> ShelterTraceChannel = ECC_Visibility;

	// ---- Exertion / stamina -----------------------------------------------------------------

	/** Speeds (cm/s) at or below which the character is resting / at or above which it is sprinting. */
	UPROPERTY(EditAnywhere, Category = "Character Effects|Exertion")
	float RestSpeed = 150.f;

	UPROPERTY(EditAnywhere, Category = "Character Effects|Exertion")
	float SprintSpeed = 600.f;

	/** If false, stamina is only changed by SetStamina (e.g. your own stamina system drives it). */
	UPROPERTY(EditAnywhere, Category = "Character Effects|Exertion")
	bool bSimulateStamina = true;

	UPROPERTY(EditAnywhere, Category = "Character Effects|Exertion")
	float StaminaDrainPerSecond = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Character Effects|Exertion")
	float StaminaRegenPerSecond = 0.08f;

	/** Body heat build-up and cool-down rates (per second). */
	UPROPERTY(EditAnywhere, Category = "Character Effects|Exertion")
	float HeatUpRate = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Character Effects|Exertion")
	float CoolDownRate = 0.04f;

	/** Sweat appears slowly and dries even slower (humidity slows drying further). */
	UPROPERTY(EditAnywhere, Category = "Character Effects|Exertion")
	float SweatBuildRate = 0.15f;

	UPROPERTY(EditAnywhere, Category = "Character Effects|Exertion")
	float SweatDryRate = 0.02f;

	UPROPERTY(EditAnywhere, Category = "Character Effects|Exertion")
	float WetnessBuildRate = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Character Effects|Exertion")
	float WetnessDryRate = 0.015f;

	UFUNCTION(BlueprintCallable, Category = "Character Effects")
	void SetStamina(float NewStamina) { Stamina = FMath::Clamp(NewStamina, 0.f, 1.f); }

	UFUNCTION(BlueprintPure, Category = "Character Effects")
	float GetStamina() const { return Stamina; }

	UFUNCTION(BlueprintPure, Category = "Character Effects")
	float GetSweatIntensity() const { return Sweat; }

	UFUNCTION(BlueprintPure, Category = "Character Effects")
	float GetWetness() const { return Wetness; }

	// ---- Damage -----------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "Character Effects|Damage")
	float BulletWoundRadius = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Character Effects|Damage")
	float BluntBruiseRadius = 7.f;

	/** Bruises darken over this many seconds after the hit... */
	UPROPERTY(EditAnywhere, Category = "Character Effects|Damage")
	float BruiseDevelopTime = 6.f;

	/** ...and fully heal after this many seconds (0 = never heal). Bullet wounds never heal. */
	UPROPERTY(EditAnywhere, Category = "Character Effects|Damage")
	float BruiseHealTime = 240.f;

	/** Two hits closer than this (cm, pre-skinned space) merge into one stronger mark. */
	UPROPERTY(EditAnywhere, Category = "Character Effects|Damage")
	float MergeDistance = 4.f;

	/** Damage types treated as blunt when hits come from OnTakePointDamage; everything else is a bullet. */
	UPROPERTY(EditAnywhere, Category = "Character Effects|Damage")
	TArray<TSubclassOf<UDamageType>> BluntDamageTypes;

	/** Automatically listen to the owner's OnTakePointDamage. */
	UPROPERTY(EditAnywhere, Category = "Character Effects|Damage")
	bool bAutoBindPointDamage = true;

	/**
	 * Register a hit. BoneName may be None: the closest bone on the first driven mesh is used.
	 * Safe to call every frame of a shotgun blast; hits merge and the oldest weakest slot is recycled.
	 */
	UFUNCTION(BlueprintCallable, Category = "Character Effects")
	void ApplyHit(FVector WorldLocation, FName BoneName, ENHCharacterHitType HitType, float Strength = 1.f);

	UFUNCTION(BlueprintCallable, Category = "Character Effects")
	void ClearDamage();

	UFUNCTION(BlueprintPure, Category = "Character Effects")
	TArray<FNHCharacterHitRecord> GetActiveHits() const;

	// ---- Performance ------------------------------------------------------------------------

	/** Tick interval by distance from the local camera (cm). */
	UPROPERTY(EditAnywhere, Category = "Character Effects|Performance")
	float NearDistance = 1500.f;

	UPROPERTY(EditAnywhere, Category = "Character Effects|Performance")
	float FarDistance = 5000.f;

	UPROPERTY(EditAnywhere, Category = "Character Effects|Performance")
	float NearTickInterval = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Character Effects|Performance")
	float MidTickInterval = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Character Effects|Performance")
	float FarTickInterval = 0.6f;

	/** Scalar changes smaller than this are not pushed to the GPU. */
	UPROPERTY(EditAnywhere, Category = "Character Effects|Performance")
	float ParameterEpsilon = 0.004f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// Simulation
	void ReadWeather(float& OutTemperature, float& OutHumidity, float& OutRain);
	bool IsSheltered(float DeltaTime);
	void UpdateExertion(float DeltaTime, float Temperature, float Humidity, float Rain);
	bool UpdateHitAging(float DeltaTime);

	// GPU writes
	void PushScalars(bool bForce);
	void PushHitSlot(int32 HitIndex);
	void PushAllHits();

	// Helpers
	void UpdateTickSignificance();
	bool IsAnyMeshRendered() const;
	bool ComputePreSkinnedCentre(const USkeletalMeshComponent* Mesh, FName BoneName, const FVector& WorldLocation, FVector3f& OutCentre) const;
	float GetHitVisualIntensity(const FNHCharacterHitRecord& Hit) const;
	static bool MaterialHasScalar(const UMaterialInterface* Material, FName Param);
	static bool MaterialHasVector(const UMaterialInterface* Material, FName Param);

	UFUNCTION()
	void HandlePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation,
		UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection,
		const UDamageType* DamageType, AActor* DamageCauser);

	UPROPERTY(Transient)
	TArray<FNHCharacterEffectsMeshEntry> MeshEntries;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollectionInstance> CachedWeatherInstance = nullptr;

	UPROPERTY(Transient)
	TWeakObjectPtr<UNHCharacterEffectsMIDPool> Pool;

	FNHCharacterHitRecord Hits[NH_CHARACTER_EFFECTS_MAX_HITS];
	float LastPushedHitIntensity[NH_CHARACTER_EFFECTS_MAX_HITS];

	/** Bit i set = hit slot i changed while the character was not rendered and must be pushed. */
	uint32 DirtyHitMask = 0;

	float Stamina = 1.f;
	float Exertion = 0.f;
	float Sweat = 0.f;
	float Wetness = 0.f;
	float LastPushedSweat = -1.f;
	float LastPushedWetness = -1.f;

	float ShelterTimer = 0.f;
	bool bShelteredCached = false;
	float SignificanceTimer = 0.f;
	bool bInitialized = false;

	/** Precomputed FNames so no FName is constructed during gameplay. */
	FName SweatParamName;
	FName WetnessParamName;
	FName HitSphereParamNames[NH_CHARACTER_EFFECTS_MAX_HITS];
	FName HitDataParamNames[NH_CHARACTER_EFFECTS_MAX_HITS];
};
