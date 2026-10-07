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

UENUM(BlueprintType)
enum class ENHLightingPreset : uint8
{
	Day,        // 10:00, clear: the daytime preset
	DustyNoon,  // 13:00 harmattan: harsh sun, warm beige haze (the browser demo's look)
	Sunset,     // 18:15: low orange sun across the lagoon, street lamps coming on
	NightRain   // 22:00, raining: moonlight through cloud, wet roads and puddles, lamps, lit windows and signs
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float FogDensity = 0.02f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float FogHeightFalloff = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") FLinearColor FogColor = FLinearColor(0.45f, 0.5f, 0.6f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") bool bVolumetricFog = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float ExposureMinEV100 = 13.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float ExposureMaxEV100 = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float ExposureBias = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade") float Saturation = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade") float Contrast = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade") float WhiteTemp = 6500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade") float Bloom = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grade") float Vignette = 0.3f;

	/** MPC_NHWeather: how wet surfaces are, how much water stands in puddles, rain strength (FX in step 7), night lights (lamps, windows, signs) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0", ClampMax = "1")) float Wetness = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0", ClampMax = "1")) float Puddles = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0", ClampMax = "1")) float Rain = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0", ClampMax = "1")) float NightLights = 0.f;
};

/**
 * The street block's whole lighting setup in one actor: sun (or moon), sky atmosphere, real-time sky
 * light, height fog, volumetric clouds and an unbound post-process volume, plus the weather parameters
 * the blockout material reads (wet roads and puddles, night glow) and the street lamps on every city tile.
 * Switch presets with the buttons in the Details panel, `NHLighting <Day|DustyNoon|Sunset|NightRain>`
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

	/** MPC_NHWeather, made by Scripts/nh_blockout_materials.py */
	UPROPERTY(EditAnywhere, Category = "Lighting") TSoftObjectPtr<UMaterialParameterCollection> Weather;
	UPROPERTY(EditAnywhere, Category = "Lighting") TSoftObjectPtr<UMaterialInterface> CloudMaterial;

	UFUNCTION(BlueprintCallable, Category = "Lighting") void ApplyPreset(ENHLightingPreset NewPreset);
	UFUNCTION(BlueprintCallable, Category = "Lighting") void CyclePreset();
	UFUNCTION(BlueprintPure, Category = "Lighting") FNHLightingSettings SettingsFor(ENHLightingPreset P) const;

	UFUNCTION(CallInEditor, Category = "Lighting") void SetDay() { ApplyPreset(ENHLightingPreset::Day); }
	UFUNCTION(CallInEditor, Category = "Lighting") void SetDustyNoon() { ApplyPreset(ENHLightingPreset::DustyNoon); }
	UFUNCTION(CallInEditor, Category = "Lighting") void SetSunset() { ApplyPreset(ENHLightingPreset::Sunset); }
	UFUNCTION(CallInEditor, Category = "Lighting") void SetNightRain() { ApplyPreset(ENHLightingPreset::NightRain); }

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
