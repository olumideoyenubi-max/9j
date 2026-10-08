#include "Lighting/NHLightingRig.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "EngineUtils.h"
#include "HAL/PlatformMemory.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "NaijaHustleGame.h"
#include "World/NHCityTile.h"

ANHLightingRig::ANHLightingRig()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetAtmosphereSunLight(true);
	Sun->LightSourceAngle = 0.53f; // the sun's real angular size: crisp but not razor-sharp shadows

	SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
	SkyAtmosphere->SetupAttachment(Root);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;
	SkyLight->bRealTimeCapture = true;
	SkyLight->bLowerHemisphereIsBlack = true; // "is a solid colour": the ground's bounce, set per preset

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("HeightFog"));
	Fog->SetupAttachment(Root);

	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(Root);

	Post = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	Post->SetupAttachment(Root);
	Post->bUnbound = true;

	Weather = TSoftObjectPtr<UMaterialParameterCollection>(FSoftObjectPath(TEXT("/Game/NaijaHustle/Lighting/Presets/MPC_NHWeather.MPC_NHWeather")));
	CloudMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst")));

	// ---- presets (see FNHLightingSettings for units)
	Day = FNHLightingSettings(); // the struct defaults are the clear 10:00 day

	DustyNoon.SunPitch = -78.f; DustyNoon.SunYaw = 180.f; DustyNoon.SunLux = 95000.f; DustyNoon.SunColor = FLinearColor(1.f, 0.97f, 0.92f);
	DustyNoon.SkyLightIntensity = 0.9f;
	DustyNoon.FogDensity = 0.05f; DustyNoon.FogHeightFalloff = 0.35f; DustyNoon.FogColor = FLinearColor(0.7f, 0.6f, 0.45f);
	DustyNoon.ExposureMinEV100 = 14.f; DustyNoon.ExposureMaxEV100 = 16.f;
	DustyNoon.Saturation = 0.9f; DustyNoon.Contrast = 1.08f; DustyNoon.WhiteTemp = 6000.f; DustyNoon.Bloom = 0.6f;

	Sunset.SunPitch = -7.f; Sunset.SunYaw = 5.f; Sunset.SunLux = 6000.f; Sunset.SunColor = FLinearColor(1.f, 0.62f, 0.35f);
	Sunset.FogDensity = 0.035f; Sunset.FogHeightFalloff = 0.25f; Sunset.FogColor = FLinearColor(0.6f, 0.42f, 0.3f); Sunset.bVolumetricFog = true;
	Sunset.ExposureMinEV100 = 9.f; Sunset.ExposureMaxEV100 = 12.f;
	Sunset.Saturation = 1.05f; Sunset.Contrast = 1.05f; Sunset.WhiteTemp = 5600.f; Sunset.Bloom = 0.8f; Sunset.NightLights = 0.3f;

	NightRain.SunPitch = -40.f; NightRain.SunYaw = 140.f; NightRain.SunLux = 0.3f; NightRain.SunColor = FLinearColor(0.6f, 0.7f, 1.f);
	NightRain.SkyLightIntensity = 1.f;
	NightRain.FogDensity = 0.06f; NightRain.FogHeightFalloff = 0.3f; NightRain.FogColor = FLinearColor(0.05f, 0.06f, 0.08f); NightRain.bVolumetricFog = true;
	NightRain.ExposureMinEV100 = 1.5f; NightRain.ExposureMaxEV100 = 5.5f; NightRain.ExposureBias = 0.5f;
	NightRain.Saturation = 0.95f; NightRain.Contrast = 1.1f; NightRain.WhiteTemp = 7000.f; NightRain.Bloom = 1.f; NightRain.Vignette = 0.45f;
	NightRain.Wetness = 1.f; NightRain.Puddles = 1.f; NightRain.Rain = 1.f; NightRain.NightLights = 1.f;

	// sun 40 degrees up in the east-south-east, raking across the streets; exposure stays physical (EV100 for full sun)
	HarshMorning.SunPitch = -40.f; HarshMorning.SunYaw = 160.f; HarshMorning.SunLux = 100000.f; HarshMorning.SunColor = FLinearColor(1.f, 0.95f, 0.86f);
	HarshMorning.SkyLightIntensity = 1.3f;
	HarshMorning.GroundBounce = 0.3f; HarshMorning.BounceColor = FLinearColor(0.8f, 0.6f, 0.42f); HarshMorning.AmbientOcclusion = 0.7f;
	HarshMorning.FogFarDensity = 0.004f; HarshMorning.LightShafts = 0.2f;
	HarshMorning.FogDensity = 0.02f; HarshMorning.FogHeightFalloff = 0.6f; HarshMorning.FogColor = FLinearColor(0.78f, 0.66f, 0.48f); HarshMorning.FogLuminance = 5000.f; HarshMorning.bVolumetricFog = true;
	HarshMorning.ExposureMinEV100 = 13.5f; HarshMorning.ExposureMaxEV100 = 15.5f;
	HarshMorning.Saturation = 0.9f; HarshMorning.Contrast = 1.1f; HarshMorning.WhiteTemp = 5800.f; HarshMorning.Bloom = 0.4f; HarshMorning.Vignette = 0.3f; HarshMorning.Sharpen = 0.6f;

	// sun 12 degrees up in the west: amber light along the east-west streets, shadows several houses long
	GoldenEvening.SunPitch = -12.f; GoldenEvening.SunYaw = 15.f; GoldenEvening.SunLux = 24000.f; GoldenEvening.SunColor = FLinearColor(1.f, 0.7f, 0.4f);
	GoldenEvening.SkyLightIntensity = 1.4f;
	GoldenEvening.GroundBounce = 0.4f; GoldenEvening.BounceColor = FLinearColor(0.9f, 0.6f, 0.38f); GoldenEvening.AmbientOcclusion = 0.7f;
	GoldenEvening.FogDensity = 0.025f; GoldenEvening.FogHeightFalloff = 0.5f; GoldenEvening.FogColor = FLinearColor(0.95f, 0.62f, 0.36f); GoldenEvening.FogLuminance = 1400.f;
	GoldenEvening.FogFarDensity = 0.005f; GoldenEvening.LightShafts = 0.35f; GoldenEvening.bVolumetricFog = true;
	GoldenEvening.ExposureMinEV100 = 11.f; GoldenEvening.ExposureMaxEV100 = 13.5f;
	GoldenEvening.Saturation = 1.f; GoldenEvening.Contrast = 1.08f; GoldenEvening.WhiteTemp = 6200.f; GoldenEvening.Bloom = 0.6f; GoldenEvening.Vignette = 0.35f; GoldenEvening.Sharpen = 0.4f;
	GoldenEvening.NightLights = 0.12f;
}

FNHLightingSettings ANHLightingRig::SettingsFor(ENHLightingPreset P) const
{
	switch (P)
	{
	case ENHLightingPreset::DustyNoon: return DustyNoon;
	case ENHLightingPreset::Sunset:    return Sunset;
	case ENHLightingPreset::NightRain: return NightRain;
	case ENHLightingPreset::HarshMorning: return HarshMorning;
	case ENHLightingPreset::GoldenEvening: return GoldenEvening;
	default:                           return Day;
	}
}

void ANHLightingRig::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyPreset(Preset);
}

void ANHLightingRig::BeginPlay()
{
	Super::BeginPlay();
	ApplyPreset(Preset);
}

void ANHLightingRig::CyclePreset()
{
	ApplyPreset(static_cast<ENHLightingPreset>((static_cast<uint8>(Preset) + 1) % 6));
}

void ANHLightingRig::ApplyPreset(ENHLightingPreset NewPreset)
{
	Preset = NewPreset;
	const FNHLightingSettings S = SettingsFor(Preset);

	Sun->SetWorldRotation(FRotator(S.SunPitch, S.SunYaw, 0.f));
	Sun->SetIntensity(S.SunLux);
	Sun->SetLightColor(S.SunColor);
	SkyLight->SetIntensity(S.SkyLightIntensity);

	// the faked bounce: sunlit ground as a light source below. Luminance of a matt surface = illuminance x reflectance / pi
	const float GroundLux = S.SunLux * FMath::Sin(FMath::DegreesToRadians(FMath::Clamp(-S.SunPitch, 0.f, 90.f)));
	SkyLight->SetLowerHemisphereColor(S.SunColor * S.BounceColor * (GroundLux * S.GroundBounce / PI));

	// light shafts: bloom from the sun's disc, cut off by whatever stands in front of it
	Sun->SetEnableLightShaftBloom(S.LightShafts > 0.f);
	Sun->SetBloomScale(S.LightShafts);
	Sun->SetBloomThreshold(1.f);     // after exposure: only what is brighter than white (the sky round the sun) streams
	Sun->SetBloomMaxBrightness(3.f);
	Sun->SetBloomTint(S.SunColor.ToFColor(true));

	Fog->SetFogDensity(S.FogDensity);
	Fog->SetFogHeightFalloff(S.FogHeightFalloff);
	Fog->SetFogInscatteringColor(S.FogColor * S.FogLuminance);
	Fog->SetSecondFogDensity(S.FogFarDensity);
	Fog->SetSecondFogHeightFalloff(0.03f);
	Fog->SetDirectionalInscatteringColor(FLinearColor::Black); // the fog's own sun glow washes the whole view out at daylight exposure; the light shafts do that job
	// Lumen, volumetric fog and volumetric clouds together run an 8 GB machine out of GPU memory, so skip them there
	const bool bLowMemory = FPlatformMemory::GetPhysicalGBRam() <= 8;
	Fog->SetVolumetricFog(S.bVolumetricFog && !bLowMemory);
	Clouds->SetVisibility(!bLowMemory);

	if (UMaterialInterface* CloudMat = CloudMaterial.LoadSynchronous())
	{
		Clouds->SetMaterial(CloudMat);
	}

	FPostProcessSettings& P = Post->Settings;
	P.bOverride_AutoExposureMethod = true;      P.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
	P.bOverride_AutoExposureMinBrightness = true; P.AutoExposureMinBrightness = S.ExposureMinEV100;
	P.bOverride_AutoExposureMaxBrightness = true; P.AutoExposureMaxBrightness = S.ExposureMaxEV100;
	P.bOverride_AutoExposureBias = true;        P.AutoExposureBias = S.ExposureBias;
	P.bOverride_ColorSaturation = true;         P.ColorSaturation = FVector4(S.Saturation, S.Saturation, S.Saturation, 1.f);
	P.bOverride_ColorContrast = true;           P.ColorContrast = FVector4(S.Contrast, S.Contrast, S.Contrast, 1.f);
	P.bOverride_WhiteTemp = true;               P.WhiteTemp = S.WhiteTemp;
	P.bOverride_BloomIntensity = true;          P.BloomIntensity = S.Bloom;
	P.bOverride_VignetteIntensity = true;       P.VignetteIntensity = S.Vignette;
	P.bOverride_Sharpen = true;                 P.Sharpen = S.Sharpen;
	P.bOverride_AmbientOcclusionIntensity = true; P.AmbientOcclusionIntensity = S.AmbientOcclusion;
	P.bOverride_AmbientOcclusionRadius = true;  P.AmbientOcclusionRadius = 140.f;
	if (bLowMemory)
	{
		P.bOverride_DynamicGlobalIlluminationMethod = true; P.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
		P.bOverride_ReflectionMethod = true;                P.ReflectionMethod = EReflectionMethod::ScreenSpace;
	}

	if (UWorld* World = GetWorld())
	{
		if (UMaterialParameterCollection* MPC = Weather.LoadSynchronous())
		{
			UKismetMaterialLibrary::SetScalarParameterValue(this, MPC, TEXT("Wetness"), S.Wetness);
			UKismetMaterialLibrary::SetScalarParameterValue(this, MPC, TEXT("Puddles"), S.Puddles);
			UKismetMaterialLibrary::SetScalarParameterValue(this, MPC, TEXT("Rain"), S.Rain);
			UKismetMaterialLibrary::SetScalarParameterValue(this, MPC, TEXT("NightLights"), S.NightLights);
		}
		else
		{
			UE_LOG(LogNHGame, Warning, TEXT("NHLightingRig: MPC_NHWeather not found; run Scripts/nh_blockout_materials.py"));
		}
		for (TActorIterator<ANHCityTile> It(World); It; ++It)
		{
			It->SetNightLights(S.NightLights);
		}
	}
}

ANHLightingRig* ANHLightingRig::Find(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<ANHLightingRig> It(World);
	return It ? *It : nullptr;
}
