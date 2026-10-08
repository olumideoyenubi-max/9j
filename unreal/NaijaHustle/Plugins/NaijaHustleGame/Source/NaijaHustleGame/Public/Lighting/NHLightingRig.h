#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NHLightingRig.generated.h"

class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class UVolumetricCloudComponent;
class UPostProcessComponent;
class UMaterialParameterCollection;
class UMaterialInterface;
class UTexture;

UENUM(BlueprintType)
enum class ENHLightingPreset : uint8
{
	Day,        // 10:00, clear: the daytime preset
	DustyNoon,  // 13:00 harmattan: harsh sun, warm beige haze (the browser demo's look)
	Sunset,     // 18:15: low orange sun across the lagoon, street lamps coming on
	NightRain,  // 22:00, raining: moonlight through cloud, wet roads and puddles, lamps, lit windows and signs
	HarshMorning, // 9:30: hard low sun, long shadows, warm dust hanging near the ground (8:00 to 11:30)
	GoldenEvening // 17:00: low amber sun from the west, long warm shadows, glowing haze (16:00 to 17:30)
};

/**
 * One lighting preset, in physical units: sun in lux, exposure in EV100 (DefaultEngine.ini sets
 * "Extend default luminance range", so auto exposure works in EV100). These are starting values;
 * every field is editable on the rig in the level for look development.
 */
USTRUCT(BlueprintType)
struct FNHLightingSettings
{
	GENERATED_BODY()

	/** Sun direction: pitch below the horizon is the sun's elevation (-50 = 50° up); yaw is the way the light travels (0 = towards +X, east) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun") float SunPitch = -50.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun") float SunYaw = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun", meta = (Units = "Lux")) float SunLux = 75000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun") FLinearColor SunColor = FLinearColor(1.f, 0.96f, 0.9f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") float SkyLightIntensity = 1.f;

	/**
	 * Faked bounce light (there is no global illumination without Lumen or a bake): the sky light's lower half glows
	 * as sunlit ground would, so walls in shade and the undersides of balconies pick up warm light from below.
	 * GroundBounce is the share of the sunlight the ground throws back (0 = none, as before), BounceColor its tint.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounce", meta = (ClampMin = "0", ClampMax = "1")) float GroundBounce = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounce") FLinearColor BounceColor = FLinearColor(0.75f, 0.6f, 0.45f);
	/** Screen-space ambient occlusion: contact shadows in corners and under balconies (0 = off) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounce", meta = (ClampMin = "0", ClampMax = "1")) float AmbientOcclusion = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float FogDensity = 0.02f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float FogHeightFalloff = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") FLinearColor FogColor = FLinearColor(0.45f, 0.5f, 0.6f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") bool bVolumetricFog = false;
	/** Multiplies FogColor into scene luminance (nits). At daylight exposure a fog colour of about 1 renders black, so a sunlit haze needs thousands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog", meta = (ClampMin = "0")) float FogLuminance = 1.f;
	/** A second, thin layer that reaches high: the pale fade of far buildings (0 = none) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog", meta = (ClampMin = "0")) float FogFarDensity = 0.f;
	/** Light-shaft bloom streaming from the sun past roof edges (0 = off) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog", meta = (ClampMin = "0", ClampMax = "1")) float LightShafts = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float ExposureMinEV100 = 13.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float ExposureMaxEV100 = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float ExposureBias = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade") float Saturation = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade") float Contrast = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade") float WhiteTemp = 6500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade") float Bloom = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade") float Vignette = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade", meta = (ClampMin = "0", ClampMax = "2")) float Sharpen = 0.f;
	/** Film curve: Toe crushes or opens the darks, Shoulder rolls the highlights off (the engine's filmic tone mapper; 0.55 and 0.26 are its defaults) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade", meta = (ClampMin = "0", ClampMax = "1")) float FilmToe = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade", meta = (ClampMin = "0", ClampMax = "1")) float FilmShoulder = 0.26f;
	/** Split tone: shadows are lifted towards ShadowTint by ShadowLift (0 = untouched), highlights are multiplied by HighlightTint */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade") FLinearColor ShadowTint = FLinearColor(0.25f, 0.8f, 0.9f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade", meta = (ClampMin = "0", ClampMax = "0.1")) float ShadowLift = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade") FLinearColor HighlightTint = FLinearColor::White;
	/** Film grain and lens colour fringing at the frame edges (0 = off) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade", meta = (ClampMin = "0", ClampMax = "1")) float FilmGrain = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade", meta = (ClampMin = "0", ClampMax = "5")) float ChromaticAberration = 0.f;
	/** How much of the rig's GradeLUT is mixed in (0 = none) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade", meta = (ClampMin = "0", ClampMax = "1")) float LutIntensity = 0.f;

	/** MPC_NHWeather: how wet surfaces are, how much water stands in puddles, rain strength (FX in step 7), night lights (lamps, windows, signs) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0", ClampMax = "1")) float Wetness = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0", ClampMax = "1")) float Puddles = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0", ClampMax = "1")) float Rain = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0", ClampMax = "1")) float NightLights = 0.f;
};

/**
 * The street block's whole lighting setup in one actor: sun (or moon), sky atmosphere, real-time sky light
 * (with a faked ground bounce), height fog, volumetric clouds and the one unbound post-process volume that grades the whole game, plus the weather parameters
 * the blockout material reads (wet roads and puddles, night glow) and the street lamps on every city tile.
 * Switch presets with the buttons in the Details panel, `NHLighting <Day|DustyNoon|Sunset|NightRain|HarshMorning|GoldenEvening>`
 * in the console, or L in game.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHLightingRig : public AActor
{
	GENERATED_BODY()

public:
	ANHLightingRig();

	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting") ENHLightingPreset Preset = ENHLightingPreset::NightRain;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting|Presets") FNHLightingSettings Day;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting|Presets") FNHLightingSettings DustyNoon;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting|Presets") FNHLightingSettings Sunset;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting|Presets") FNHLightingSettings NightRain;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting|Presets") FNHLightingSettings HarshMorning;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting|Presets") FNHLightingSettings GoldenEvening;

	/** MPC_NHWeather, made by Scripts/nh_blockout_materials.py */
	UPROPERTY(EditAnywhere, Category = "Lighting") TSoftObjectPtr<UMaterialParameterCollection> Weather;
	UPROPERTY(EditAnywhere, Category = "Lighting") TSoftObjectPtr<UMaterialInterface> CloudMaterial;
	/** T_NHGrade_LUT, a 256x16 colour lookup table made by Scripts/nh_grade_lut.py. Swap in any other LUT texture here. */
	UPROPERTY(EditAnywhere, Category = "Lighting") TSoftObjectPtr<UTexture> GradeLUT;
	/** How quickly auto exposure follows the scene, in stops a second. Low values stop the picture pumping when the camera swings from shade to sun. */
	UPROPERTY(EditAnywhere, Category = "Lighting", meta = (ClampMin = "0.02", ClampMax = "20")) float ExposureSpeed = 1.5f;

	UFUNCTION(BlueprintCallable, Category = "Lighting") void ApplyPreset(ENHLightingPreset NewPreset);
	UFUNCTION(BlueprintCallable, Category = "Lighting") void CyclePreset();
	UFUNCTION(BlueprintPure, Category = "Lighting") FNHLightingSettings SettingsFor(ENHLightingPreset P) const;

	UFUNCTION(CallInEditor, Category = "Lighting") void SetDay() { ApplyPreset(ENHLightingPreset::Day); }
	UFUNCTION(CallInEditor, Category = "Lighting") void SetDustyNoon() { ApplyPreset(ENHLightingPreset::DustyNoon); }
	UFUNCTION(CallInEditor, Category = "Lighting") void SetSunset() { ApplyPreset(ENHLightingPreset::Sunset); }
	UFUNCTION(CallInEditor, Category = "Lighting") void SetNightRain() { ApplyPreset(ENHLightingPreset::NightRain); }
	UFUNCTION(CallInEditor, Category = "Lighting") void SetHarshMorning() { ApplyPreset(ENHLightingPreset::HarshMorning); }
	UFUNCTION(CallInEditor, Category = "Lighting") void SetGoldenEvening() { ApplyPreset(ENHLightingPreset::GoldenEvening); }

	/** The first rig in the world, if any */
	static ANHLightingRig* Find(const UObject* WorldContext);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Lighting") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Lighting") TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY(VisibleAnywhere, Category = "Lighting") TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;
	UPROPERTY(VisibleAnywhere, Category = "Lighting") TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY(VisibleAnywhere, Category = "Lighting") TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY(VisibleAnywhere, Category = "Lighting") TObjectPtr<UVolumetricCloudComponent> Clouds;
	UPROPERTY(VisibleAnywhere, Category = "Lighting") TObjectPtr<UPostProcessComponent> Post;
};
