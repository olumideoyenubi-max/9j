#include "Lighting/NHLightingRig.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "EngineUtils.h"
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
}

FNHLightingSettings ANHLightingRig::SettingsFor(ENHLightingPreset P) const
{
	switch (P)
	{
	case ENHLightingPreset::DustyNoon: return DustyNoon;
	case ENHLightingPreset::Sunset:    return Sunset;
	case ENHLightingPreset::NightRain: return NightRain;
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
	ApplyPreset(static_cast<ENHLightingPreset>((static_cast<uint8>(Preset) + 1) % 4));
}

void ANHLightingRig::ApplyPreset(ENHLightingPreset NewPreset)
{
	Preset = NewPreset;
	const FNHLightingSettings S = SettingsFor(Preset);

	Sun->SetWorldRotation(FRotator(S.SunPitch, S.SunYaw, 0.f));
	Sun->SetIntensity(S.SunLux);
	Sun->SetLightColor(S.SunColor);
	SkyLight->SetIntensity(S.SkyLightIntensity);

	Fog->SetFogDensity(S.FogDensity);
	Fog->SetFogHeightFalloff(S.FogHeightFalloff);
	Fog->SetFogInscatteringColor(S.FogColor);
	Fog->SetVolumetricFog(S.bVolumetricFog);

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
