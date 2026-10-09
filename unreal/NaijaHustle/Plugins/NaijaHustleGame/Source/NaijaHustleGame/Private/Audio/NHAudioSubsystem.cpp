#include "Audio/NHAudioSubsystem.h"

#include "Audio/NHAudioZone.h"
#include "Audio/NHSubmixEffectMono.h"
#include "AudioDevice.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Dom/JsonObject.h"
#include "Gameplay/NHGameDirector.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"
#include "UI/NHHUD.h"
#include "Misc/ConfigCacheIni.h"
#include "NaijaHustleGame.h"
#include "Phone/NHPhone.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundSubmix.h"
#include "SubmixEffects/AudioMixerSubmixEffectEQ.h"
#include "SubmixEffects/AudioMixerSubmixEffectReverb.h"
#include "Vehicles/NHVehicle.h"

namespace NHAudio
{
	const TCHAR* Section = TEXT("NaijaHustle");
	const TCHAR* ClassNames[] = { TEXT("Master"), TEXT("Music"), TEXT("Radio"), TEXT("Dialogue"), TEXT("Barks"), TEXT("SFX"), TEXT("Vehicles"), TEXT("Weapons"),
		TEXT("Footsteps"), TEXT("UI"), TEXT("Phone"), TEXT("Ambience"), TEXT("VoiceChat") };
	const TCHAR* SubmixNames[] = { TEXT("Music"), TEXT("Radio"), TEXT("Dialogue"), TEXT("World"), TEXT("UI"), TEXT("Voice") };
	const TCHAR* SpaceNames[] = { TEXT("Street"), TEXT("Market"), TEXT("MotorPark"), TEXT("Interior"), TEXT("UnderBridge"), TEXT("Tunnel") };
	const TCHAR* VolumeNames[] = { TEXT("Master"), TEXT("Music"), TEXT("Radio"), TEXT("Dialogue"), TEXT("Effects"), TEXT("Ambience"), TEXT("Voice chat") };
	const TCHAR* VolumeKeys[] = { TEXT("AudioMaster"), TEXT("AudioMusic"), TEXT("AudioRadio"), TEXT("AudioDialogue"), TEXT("AudioSFX"), TEXT("AudioAmbience"), TEXT("AudioVoiceChat") };
	const int32 VolumeDefaults[] = { 100, 80, 80, 100, 100, 90, 100 };
	const ENHSoundClass VolumeClass[] = { ENHSoundClass::Master, ENHSoundClass::Music, ENHSoundClass::Radio, ENHSoundClass::Dialogue, ENHSoundClass::SFX, ENHSoundClass::Ambience, ENHSoundClass::VoiceChat };
	const TCHAR* MixNames[] = { TEXT("Dialogue"), TEXT("PhoneCall"), TEXT("Cabin"), TEXT("RadioMuffled") };

	struct FClassDef
	{
		ENHSoundClass Parent;
		ENHSubmix Submix;
		/** Takes the space's reverb, and the muffling of an indoor zone */
		bool bWorld;
		/** Carries on while the game is paused */
		bool bUI;
	};
	const FClassDef ClassDefs[] = {
		{ ENHSoundClass::Count, ENHSubmix::World, false, false },   // Master
		{ ENHSoundClass::Master, ENHSubmix::Music, false, false },  // Music
		{ ENHSoundClass::Master, ENHSubmix::Radio, false, false },  // Radio
		{ ENHSoundClass::Master, ENHSubmix::Dialogue, false, false }, // Dialogue
		{ ENHSoundClass::Dialogue, ENHSubmix::World, true, false }, // Barks
		{ ENHSoundClass::Master, ENHSubmix::World, true, false },   // SFX
		{ ENHSoundClass::SFX, ENHSubmix::World, true, false },      // Vehicles
		{ ENHSoundClass::SFX, ENHSubmix::World, true, false },      // Weapons
		{ ENHSoundClass::SFX, ENHSubmix::World, true, false },      // Footsteps
		{ ENHSoundClass::SFX, ENHSubmix::UI, false, true },         // UI
		{ ENHSoundClass::SFX, ENHSubmix::UI, false, true },         // Phone
		{ ENHSoundClass::Master, ENHSubmix::World, true, false },   // Ambience
		{ ENHSoundClass::Master, ENHSubmix::Voice, false, false } };  // VoiceChat

	/** Inner and Falloff in cm: full volume inside Inner, silent Falloff beyond it. Priority 0..100, the higher keeps its voice. */
	struct FKindDef
	{
		const TCHAR* Name;
		ENHSoundClass Class;
		bool bSpatial;
		float Inner, Falloff;
		bool bOcclusion;
		int32 MaxCount;
		EMaxConcurrentResolutionRule::Type Rule;
		float Priority;
	};
	const FKindDef KindDefs[] = {
		{ TEXT("Music"), ENHSoundClass::Music, false, 0.f, 0.f, false, 2, EMaxConcurrentResolutionRule::StopOldest, 100.f },
		{ TEXT("RadioCabin"), ENHSoundClass::Radio, false, 0.f, 0.f, false, 1, EMaxConcurrentResolutionRule::StopOldest, 60.f },
		{ TEXT("RadioWorld"), ENHSoundClass::Radio, true, 300.f, 3500.f, true, 3, EMaxConcurrentResolutionRule::StopFarthestThenOldest, 20.f },
		{ TEXT("Dialogue"), ENHSoundClass::Dialogue, false, 0.f, 0.f, false, 2, EMaxConcurrentResolutionRule::StopOldest, 95.f },
		{ TEXT("Bark"), ENHSoundClass::Barks, true, 200.f, 2500.f, true, 4, EMaxConcurrentResolutionRule::StopFarthestThenPreventNew, 40.f },
		{ TEXT("VehicleEngine"), ENHSoundClass::Vehicles, true, 400.f, 6000.f, true, 8, EMaxConcurrentResolutionRule::StopFarthestThenOldest, 50.f },
		{ TEXT("Horn"), ENHSoundClass::Vehicles, true, 500.f, 9000.f, true, 4, EMaxConcurrentResolutionRule::StopFarthestThenOldest, 45.f },
		{ TEXT("VehicleDetail"), ENHSoundClass::Vehicles, true, 150.f, 2000.f, true, 6, EMaxConcurrentResolutionRule::StopFarthestThenPreventNew, 25.f },
		{ TEXT("Weapon"), ENHSoundClass::Weapons, true, 600.f, 12000.f, true, 8, EMaxConcurrentResolutionRule::StopFarthestThenOldest, 80.f },
		{ TEXT("WeaponTail"), ENHSoundClass::Weapons, true, 2000.f, 30000.f, false, 4, EMaxConcurrentResolutionRule::StopFarthestThenOldest, 35.f },
		{ TEXT("Impact"), ENHSoundClass::SFX, true, 200.f, 3500.f, true, 6, EMaxConcurrentResolutionRule::StopFarthestThenOldest, 30.f },
		{ TEXT("Footstep"), ENHSoundClass::Footsteps, true, 100.f, 1500.f, false, 6, EMaxConcurrentResolutionRule::StopFarthestThenPreventNew, 15.f },
		{ TEXT("UI"), ENHSoundClass::UI, false, 0.f, 0.f, false, 4, EMaxConcurrentResolutionRule::StopOldest, 90.f },
		{ TEXT("Phone"), ENHSoundClass::Phone, false, 0.f, 0.f, false, 2, EMaxConcurrentResolutionRule::StopOldest, 92.f },
		{ TEXT("AmbienceBed"), ENHSoundClass::Ambience, false, 0.f, 0.f, false, 4, EMaxConcurrentResolutionRule::StopOldest, 55.f },
		{ TEXT("AmbienceSpot"), ENHSoundClass::Ambience, true, 300.f, 5000.f, true, 8, EMaxConcurrentResolutionRule::StopFarthestThenPreventNew, 10.f },
		{ TEXT("Crowd"), ENHSoundClass::Ambience, true, 400.f, 4000.f, true, 6, EMaxConcurrentResolutionRule::StopFarthestThenPreventNew, 12.f },
		{ TEXT("VoiceChat"), ENHSoundClass::VoiceChat, true, 200.f, 3000.f, false, 8, EMaxConcurrentResolutionRule::StopFarthestThenOldest, 85.f } };

	/** A space's sound: how long it rings and how much of that you hear, and three EQ bands (Hz, dB) */
	struct FSpaceDef
	{
		float Decay, Wet, Density, Diffusion, GainHF;
		float LowHz, LowDb, MidHz, MidDb, HighHz, HighDb;
	};
	constexpr int32 SpaceNumbers = sizeof(FSpaceDef) / sizeof(float);
	/** How long the sound of one space takes to become the next, seconds */
	constexpr float SpaceGlide = 1.5f;
	const FSpaceDef SpaceDefs[] = {
		{ 0.9f, 0.06f, 0.6f, 0.7f, 0.75f, 200.f, 0.f, 1200.f, 0.f, 6000.f, 0.f },     // Street: open air, hard road
		{ 0.6f, 0.08f, 0.9f, 0.9f, 0.5f, 400.f, 1.5f, 1500.f, 0.f, 6000.f, -2.5f },   // Market: stalls, cloth and people soak up the top
		{ 1.4f, 0.12f, 0.7f, 0.75f, 0.8f, 200.f, 0.f, 2500.f, 1.5f, 7000.f, 0.f },    // Motor park: wide hard ground, metal bus sides
		{ 0.5f, 0.22f, 0.95f, 0.95f, 0.45f, 250.f, 2.f, 1200.f, 0.f, 5000.f, -3.f },  // Interior: a small furnished room
		{ 2.2f, 0.3f, 0.8f, 0.8f, 0.6f, 180.f, 2.f, 1000.f, 0.f, 4000.f, -1.5f },     // Under a bridge: a concrete slab overhead
		{ 3.5f, 0.42f, 0.85f, 0.85f, 0.55f, 220.f, 3.f, 1000.f, 0.f, 5000.f, -3.f } };  // Tunnel: concrete all round

	FSoundClassAdjuster Adjust(USoundClass* Class, float Volume, bool bChildren, float LowPass = MAX_FILTER_FREQUENCY)
	{
		FSoundClassAdjuster A;
		A.SoundClassObject = Class;
		A.VolumeAdjuster = Volume;
		A.LowPassFilterFrequency = LowPass;
		A.bApplyToChildren = bChildren;
		return A;
	}
}

UNHAudioSubsystem* UNHAudioSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	return Game ? Game->GetSubsystem<UNHAudioSubsystem>() : nullptr;
}

void UNHAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	for (int32& State : Forced)
	{
		State = -1;
	}
	LoadSettings();
	LoadStations();
}

void UNHAudioSubsystem::Deinitialize()
{
	if (FAudioDeviceHandle Device = FAudioDevice::GetMainAudioDevice(); Device && bBuilt)
	{
		Device->PopSoundMixModifier(UserMix);
		for (int32 I = 0; I < Mixes.Num(); ++I)
		{
			if (bPushed[I])
			{
				Device->PopSoundMixModifier(Mixes[I]);
			}
		}
		for (USoundSubmix* Submix : Submixes)
		{
			Submix->DynamicDisconnect(Device);
		}
		for (USoundClass* Class : Classes)
		{
			Device->UnregisterSoundClass(Class);
		}
	}
	bBuilt = false;
	Super::Deinitialize();
}

UWorld* UNHAudioSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* Game = GetGameInstance();
	return Game ? Game->GetWorld() : nullptr;
}

const TCHAR* UNHAudioSubsystem::SpaceName(ENHAudioSpace InSpace)
{
	return InSpace < ENHAudioSpace::Count ? NHAudio::SpaceNames[static_cast<int32>(InSpace)] : TEXT("(none)");
}

const TCHAR* UNHAudioSubsystem::VolumeName(ENHVolume Slider)
{
	return NHAudio::VolumeNames[FMath::Clamp(static_cast<int32>(Slider), 0, static_cast<int32>(ENHVolume::Count) - 1)];
}

ENHSoundClass UNHAudioSubsystem::ClassOf(ENHSoundKind Kind)
{
	return NHAudio::KindDefs[static_cast<int32>(Kind)].Class;
}

int32 UNHAudioSubsystem::MaxVoices(ENHSoundKind Kind)
{
	return NHAudio::KindDefs[static_cast<int32>(Kind)].MaxCount;
}

// ---------------------------------------------------------------------------------------------- building
void UNHAudioSubsystem::Build(UWorld* World)
{
	FAudioDeviceHandle Device = World->GetAudioDevice();
	if (!Device)
	{
		return; // a server, or -nosound
	}
	static_assert(UE_ARRAY_COUNT(NHAudio::ClassDefs) == static_cast<int32>(ENHSoundClass::Count), "one entry per sound class");
	static_assert(UE_ARRAY_COUNT(NHAudio::KindDefs) == static_cast<int32>(ENHSoundKind::Count), "one entry per sound kind");
	static_assert(UE_ARRAY_COUNT(NHAudio::SpaceDefs) == static_cast<int32>(ENHAudioSpace::Count), "one entry per space");
	static_assert(NHAudio::SpaceNumbers == UE_ARRAY_COUNT(SpaceNow), "SpaceNow holds one FSpaceDef");
	BuildSubmixes(World);
	BuildClasses();
	BuildMixes();
	BuildKinds();
	BuildSpaces();
	for (USoundClass* Class : Classes)
	{
		Device->RegisterSoundClass(Class); // parents come first in the list
	}
	Device->SetMaxChannels(VoiceBudget);
	bBuilt = true;
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: audio built: %d sound classes, %d submixes, %d mixes, %d kinds of sound, %d spaces, %d voices at most"),
		Classes.Num(), Submixes.Num(), Mixes.Num() + 1, Attenuations.Num(), static_cast<int32>(ENHAudioSpace::Count), VoiceBudget);
}

void UNHAudioSubsystem::BuildSubmixes(UWorld* World)
{
	FAudioDeviceHandle Device = World->GetAudioDevice();
	USoundSubmix& Main = Device->GetMainSubmixObject();
	WorldEQ = NewObject<USubmixEffectSubmixEQPreset>(this, TEXT("NH_World_EQ"));
	WorldReverb = NewObject<USubmixEffectReverbPreset>(this, TEXT("NH_World_Reverb"));
	FMemory::Memcpy(SpaceNow, &NHAudio::SpaceDefs[0], sizeof(SpaceNow));
	PushSpace();
	for (int32 I = 0; I < static_cast<int32>(ENHSubmix::Count); ++I)
	{
		USoundSubmix* Submix = NewObject<USoundSubmix>(this, *FString::Printf(TEXT("NH_Submix_%s"), NHAudio::SubmixNames[I]));
		if (I == static_cast<int32>(ENHSubmix::World))
		{
			Submix->SubmixEffectChain = { WorldEQ, WorldReverb };
		}
		// made at run time, so it is a dynamic submix, joined to the main one by hand (the flag is not public)
		if (const FBoolProperty* Dynamic = FindFProperty<FBoolProperty>(USoundSubmixWithParentBase::StaticClass(), TEXT("bIsDynamic")))
		{
			Dynamic->SetPropertyValue_InContainer(Submix, true);
		}
		// a submix that goes to sleep when it is quiet missed sounds that begin with a moment of silence, so these stay awake
		Submix->bAutoDisable = false;
		if (!Submix->DynamicConnect(Device, &Main))
		{
			UE_LOG(LogNHGame, Warning, TEXT("NAIJA HUSTLE: audio: the %s submix did not join the main submix"), NHAudio::SubmixNames[I]);
		}
		Submixes.Add(Submix);
	}
}

void UNHAudioSubsystem::BuildClasses()
{
	for (int32 I = 0; I < static_cast<int32>(ENHSoundClass::Count); ++I)
	{
		const NHAudio::FClassDef& Def = NHAudio::ClassDefs[I];
		USoundClass* Class = NewObject<USoundClass>(this, *FString::Printf(TEXT("NH_Class_%s"), NHAudio::ClassNames[I]));
		Class->Properties.DefaultSubmix = GetSubmix(Def.Submix); // for the editor's eyes: the engine does not route by it (see Make)
		Class->Properties.bReverb = false; // the space's reverb sits on the World submix, not on the engine's reverb send
		Class->Properties.bApplyAmbientVolumes = Def.bWorld;
		Class->Properties.bIsUISound = Def.bUI;
		Class->Properties.bIsMusic = I == static_cast<int32>(ENHSoundClass::Music);
		if (Def.Parent != ENHSoundClass::Count)
		{
			USoundClass* Parent = GetClass(Def.Parent);
			Class->ParentClass = Parent;
			Parent->ChildClasses.Add(Class);
		}
		Classes.Add(Class);
	}
}

void UNHAudioSubsystem::BuildMixes()
{
	using namespace NHAudio;
	const auto Make = [this](const TCHAR* Name, float FadeIn, float FadeOut)
	{
		USoundMix* Mix = NewObject<USoundMix>(this, *FString::Printf(TEXT("NH_Mix_%s"), Name));
		Mix->InitialDelay = 0.f;
		Mix->FadeInTime = FadeIn;
		Mix->FadeOutTime = FadeOut;
		Mix->Duration = -1.f; // until popped
		return Mix;
	};
	const auto C = [this](ENHSoundClass Class) { return GetClass(Class); };

	USoundMix* Dialogue = Make(TEXT("Dialogue"), 0.25f, 0.7f);
	Dialogue->SoundClassEffects = { Adjust(C(ENHSoundClass::Music), 0.45f, true), Adjust(C(ENHSoundClass::Radio), 0.4f, true), Adjust(C(ENHSoundClass::Ambience), 0.55f, true) };

	// a call ducks everything but the Phone class itself; SFX is listed child by child because Phone is one of its children
	USoundMix* Call = Make(TEXT("PhoneCall"), 0.3f, 0.8f);
	Call->SoundClassEffects = { Adjust(C(ENHSoundClass::Music), 0.2f, true), Adjust(C(ENHSoundClass::Radio), 0.2f, true), Adjust(C(ENHSoundClass::Ambience), 0.3f, true),
		Adjust(C(ENHSoundClass::Dialogue), 0.5f, true), Adjust(C(ENHSoundClass::SFX), 0.4f, false), Adjust(C(ENHSoundClass::Vehicles), 0.4f, false),
		Adjust(C(ENHSoundClass::Weapons), 0.4f, false), Adjust(C(ENHSoundClass::Footsteps), 0.4f, false), Adjust(C(ENHSoundClass::UI), 0.6f, false),
		Adjust(C(ENHSoundClass::VoiceChat), 0.5f, true) };

	// in a closed car the street is behind glass; the car's own sounds (Vehicles) and its radio are not
	USoundMix* Cabin = Make(TEXT("Cabin"), 0.35f, 0.35f);
	Cabin->SoundClassEffects = { Adjust(C(ENHSoundClass::Ambience), 0.6f, true, 2800.f), Adjust(C(ENHSoundClass::Weapons), 0.7f, false, 3500.f),
		Adjust(C(ENHSoundClass::Footsteps), 0.5f, false, 2500.f), Adjust(C(ENHSoundClass::Barks), 0.6f, false, 2500.f) };

	USoundMix* Muffled = Make(TEXT("RadioMuffled"), 0.25f, 0.25f);
	Muffled->SoundClassEffects = { Adjust(C(ENHSoundClass::Radio), 0.45f, true, 900.f) };

	Mixes = { Dialogue, Call, Cabin, Muffled };
	UserMix = Make(TEXT("User"), 0.f, 0.f);

	// a mission line with a voice ducks the rest without anybody asking: the Dialogue class brings its mix with it
	FPassiveSoundMixModifier Passive;
	Passive.SoundMix = Dialogue;
	Passive.MinVolumeThreshold = 0.01f;
	Passive.MaxVolumeThreshold = 10.f;
	C(ENHSoundClass::Dialogue)->PassiveSoundMixModifiers.Add(Passive);
}

void UNHAudioSubsystem::BuildKinds()
{
	for (const NHAudio::FKindDef& Def : NHAudio::KindDefs)
	{
		USoundAttenuation* Att = nullptr;
		if (Def.bSpatial)
		{
			Att = NewObject<USoundAttenuation>(this, *FString::Printf(TEXT("NH_Att_%s"), Def.Name));
			FSoundAttenuationSettings& A = Att->Attenuation;
			A.bAttenuate = true;
			A.bSpatialize = true;
			A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
			A.AttenuationShape = EAttenuationShape::Sphere;
			A.AttenuationShapeExtents = FVector(Def.Inner, 0.f, 0.f);
			A.FalloffDistance = Def.Falloff;
			A.dBAttenuationAtMax = -60.f;
			// far sounds lose their top end, as through air
			A.bAttenuateWithLPF = true;
			A.LPFRadiusMin = Def.Inner + Def.Falloff * 0.25f;
			A.LPFRadiusMax = Def.Inner + Def.Falloff;
			A.LPFFrequencyAtMin = 20000.f;
			A.LPFFrequencyAtMax = 3500.f;
			A.bEnableReverbSend = false; // see BuildClasses
			// a wall between the sound and the listener: quieter and duller. One simple ray per sound, not every frame.
			A.bEnableOcclusion = Def.bOcclusion;
			A.OcclusionTraceChannel = ECC_Visibility;
			A.bUseComplexCollisionForOcclusion = false;
			A.OcclusionLowPassFilterFrequency = 1400.f;
			A.OcclusionVolumeAttenuation = 0.55f;
			A.OcclusionInterpolationTime = 0.3f;
		}
		Attenuations.Add(Att);

		USoundConcurrency* Con = NewObject<USoundConcurrency>(this, *FString::Printf(TEXT("NH_Con_%s"), Def.Name));
		Con->Concurrency.MaxCount = Def.MaxCount;
		Con->Concurrency.ResolutionRule = Def.Rule;
		Con->Concurrency.bLimitToOwner = false;
		Con->Concurrency.VoiceStealReleaseTime = 0.05f; // a stolen voice fades instead of clicking
		Concurrencies.Add(Con);
	}
}

void UNHAudioSubsystem::BuildSpaces()
{
	MonoEffect = NewObject<UNHSubmixEffectMonoPreset>(this, TEXT("NH_Mono"));
	MonoEffect->Init();
}

// ---------------------------------------------------------------------------------------------- playing
UAudioComponent* UNHAudioSubsystem::Make(USoundBase* Sound, ENHSoundKind Kind, UWorld* World, float Volume, float Pitch)
{
	if (!bBuilt || !Sound || !World)
	{
		return nullptr;
	}
	const int32 K = static_cast<int32>(Kind);
	const NHAudio::FKindDef& Def = NHAudio::KindDefs[K];
	// Unreal takes a sound's submix from the sound itself, not from its class or its component, so it is put on
	// the sound the first time it is played. A sound that already names a submix keeps it.
	if (!Sound->SoundSubmixObject)
	{
		Sound->SoundSubmixObject = GetSubmix(NHAudio::ClassDefs[static_cast<int32>(Def.Class)].Submix);
	}
	FAudioDevice::FCreateComponentParams Params(World);
	Params.bPlay = false;
	Params.bAutoDestroy = true;
	Params.AttenuationSettings = Attenuations[K];
	Params.ConcurrencySet.Add(Concurrencies[K]);
	UAudioComponent* Comp = FAudioDevice::CreateComponent(Sound, Params);
	if (!Comp)
	{
		return nullptr;
	}
	Comp->SoundClassOverride = GetClass(Def.Class);
	Comp->bAllowSpatialization = Def.bSpatial;
	Comp->bReverb = false; // the space's reverb is on the World submix; the engine's own reverb send would add a second
	Comp->bOverridePriority = true;
	Comp->Priority = Def.Priority;
	Comp->bIsUISound = NHAudio::ClassDefs[static_cast<int32>(Def.Class)].bUI;
	Comp->SetVolumeMultiplier(Volume);
	Comp->SetPitchMultiplier(Pitch);
	return Comp;
}

UAudioComponent* UNHAudioSubsystem::Play(USoundBase* Sound, ENHSoundKind Kind, float Volume, float Pitch)
{
	UAudioComponent* Comp = Make(Sound, Kind, GetTickableGameObjectWorld(), Volume, Pitch);
	if (Comp)
	{
		Comp->Play();
	}
	return Comp;
}

UAudioComponent* UNHAudioSubsystem::PlayAt(USoundBase* Sound, ENHSoundKind Kind, const FVector& Location, float Volume, float Pitch)
{
	UAudioComponent* Comp = Make(Sound, Kind, GetTickableGameObjectWorld(), Volume, Pitch);
	if (Comp)
	{
		Comp->SetWorldLocation(Location);
		Comp->Play();
	}
	return Comp;
}

UAudioComponent* UNHAudioSubsystem::PlayAttached(USoundBase* Sound, ENHSoundKind Kind, USceneComponent* To, float Volume, float Pitch)
{
	UAudioComponent* Comp = To ? Make(Sound, Kind, To->GetWorld(), Volume, Pitch) : nullptr;
	if (Comp)
	{
		Comp->AttachToComponent(To, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Comp->bStopWhenOwnerDestroyed = true;
		Comp->Play();
	}
	return Comp;
}

int32 UNHAudioSubsystem::ActiveVoices() const
{
	const UWorld* World = GetTickableGameObjectWorld();
	FAudioDeviceHandle Device = World ? World->GetAudioDevice() : FAudioDeviceHandle();
	return Device ? Device->GetNumActiveSources() : 0;
}

// ---------------------------------------------------------------------------------------------- settings
void UNHAudioSubsystem::LoadSettings()
{
	for (int32 I = 0; I < static_cast<int32>(ENHVolume::Count); ++I)
	{
		Volumes[I] = NHAudio::VolumeDefaults[I];
		GConfig->GetInt(NHAudio::Section, NHAudio::VolumeKeys[I], Volumes[I], GGameUserSettingsIni);
		Volumes[I] = FMath::Clamp(Volumes[I], 0, 100);
	}
	GConfig->GetBool(NHAudio::Section, TEXT("Subtitles"), bSubtitles, GGameUserSettingsIni);
	GConfig->GetInt(NHAudio::Section, TEXT("SubtitleSize"), SubtitleSize, GGameUserSettingsIni);
	GConfig->GetBool(NHAudio::Section, TEXT("MonoAudio"), bMono, GGameUserSettingsIni);
	SubtitleSize = FMath::Clamp(SubtitleSize, 0, 2);
}

void UNHAudioSubsystem::SaveSettings() const
{
	for (int32 I = 0; I < static_cast<int32>(ENHVolume::Count); ++I)
	{
		GConfig->SetInt(NHAudio::Section, NHAudio::VolumeKeys[I], Volumes[I], GGameUserSettingsIni);
	}
	GConfig->SetBool(NHAudio::Section, TEXT("Subtitles"), bSubtitles, GGameUserSettingsIni);
	GConfig->SetInt(NHAudio::Section, TEXT("SubtitleSize"), SubtitleSize, GGameUserSettingsIni);
	GConfig->SetBool(NHAudio::Section, TEXT("MonoAudio"), bMono, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UNHAudioSubsystem::SetVolume(ENHVolume Slider, int32 Percent)
{
	Volumes[static_cast<int32>(Slider)] = FMath::Clamp(Percent, 0, 100);
	ApplyVolumes(GetTickableGameObjectWorld());
	SaveSettings();
}

void UNHAudioSubsystem::SetSubtitles(bool bOn)
{
	bSubtitles = bOn;
	SaveSettings();
}

void UNHAudioSubsystem::SetSubtitleSize(int32 Size)
{
	SubtitleSize = FMath::Clamp(Size, 0, 2);
	SaveSettings();
}

void UNHAudioSubsystem::SetMono(bool bOn)
{
	bMono = bOn;
	ApplyMono(GetTickableGameObjectWorld());
	SaveSettings();
}

void UNHAudioSubsystem::ApplyVolumes(UWorld* World)
{
	FAudioDeviceHandle Device = World ? World->GetAudioDevice() : FAudioDeviceHandle();
	if (!Device || !bBuilt)
	{
		return;
	}
	// each top-level class gets master x its own slider, and hands it down to its children
	const float Master = Volumes[0] / 100.f;
	for (int32 I = 1; I < static_cast<int32>(ENHVolume::Count); ++I)
	{
		Device->SetSoundMixClassOverride(UserMix, GetClass(NHAudio::VolumeClass[I]), Master * Volumes[I] / 100.f, 1.f, 0.1f, true);
	}
}

void UNHAudioSubsystem::ApplyMono(UWorld* World)
{
	if (!World || !bBuilt || bMono == bMonoApplied)
	{
		return;
	}
	if (bMono)
	{
		UAudioMixerBlueprintLibrary::AddMasterSubmixEffect(World, MonoEffect);
	}
	else
	{
		UAudioMixerBlueprintLibrary::RemoveMasterSubmixEffect(World, MonoEffect);
	}
	bMonoApplied = bMono;
}

// ---------------------------------------------------------------------------------------------- watching the game
void UNHAudioSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetTickableGameObjectWorld();
	if (!World || !World->IsGameWorld() || !World->HasBegunPlay())
	{
		return;
	}
	if (!bBuilt)
	{
		Build(World);
		if (!bBuilt)
		{
			return;
		}
	}
	if (AppliedTo.Get() != World)
	{
		// a new level: the device may have dropped every mix when the old one closed, so start from nothing
		AppliedTo = World;
		FAudioDeviceHandle Device = World->GetAudioDevice();
		for (int32 I = 0; I < Mixes.Num(); ++I)
		{
			if (bPushed[I])
			{
				Device->PopSoundMixModifier(Mixes[I]);
				bPushed[I] = false;
			}
		}
		Device->PopSoundMixModifier(UserMix);
		Device->PushSoundMixModifier(UserMix);
		ApplyVolumes(World);
		UAudioMixerBlueprintLibrary::RemoveMasterSubmixEffect(World, MonoEffect);
		bMonoApplied = false;
		ApplyMono(World);
		Inside.Reset();
	}
	if (SpaceBlend < 1.f)
	{
		// gliding from the last space's sound to this one's (real time: it carries on in the pause menu)
		SpaceBlend = FMath::Min(1.f, SpaceBlend + FApp::GetDeltaTime() / NHAudio::SpaceGlide);
		const float* To = reinterpret_cast<const float*>(&NHAudio::SpaceDefs[static_cast<int32>(Space)]);
		const float K = FMath::SmoothStep(0.f, 1.f, SpaceBlend);
		for (int32 I = 0; I < NHAudio::SpaceNumbers; ++I)
		{
			SpaceNow[I] = FMath::Lerp(SpaceFrom[I], To[I], K);
		}
		PushSpace();
	}
	Since += DeltaTime;
	if (Since >= 0.25f)
	{
		Since = 0.f;
		Watch(World);
	}
}

void UNHAudioSubsystem::Watch(UWorld* World)
{
	FAudioDeviceHandle Device = World->GetAudioDevice();
	const APlayerController* PC = World->GetFirstPlayerController();
	if (!Device || !PC)
	{
		return;
	}
	const APawn* Pawn = PC->GetPawn();
	const ANHVehicle* Car = Cast<ANHVehicle>(Pawn);
	const ANHGameDirector* Director = ANHGameDirector::Get(World);
	const ANHPhone* Phone = ANHPhone::Get(World);

	bool bWant[static_cast<int32>(ENHMix::Count)];
	bWant[static_cast<int32>(ENHMix::Dialogue)] = Director && Director->Dialogue.bOpen;
	bWant[static_cast<int32>(ENHMix::PhoneCall)] = Phone && Phone->InCall();
	// an okada and a keke have no glass to shut; a broken window lets the street back in
	bWant[static_cast<int32>(ENHMix::Cabin)] = Car && !Car->GetSpec().bBike && Car->VehicleType != TEXT("keke") && !Car->bWindowBroken;
	bWant[static_cast<int32>(ENHMix::RadioMuffled)] = RadioCar.IsValid() && RadioCar.Get() != Car;
	RadioWatch(World, Car);
	for (int32 I = 0; I < static_cast<int32>(ENHMix::Count); ++I)
	{
		const bool bOn = Forced[I] >= 0 ? Forced[I] > 0 : bWant[I];
		if (bOn != bPushed[I])
		{
			bPushed[I] = bOn;
			bOn ? Device->PushSoundMixModifier(Mixes[I]) : Device->PopSoundMixModifier(Mixes[I]);
			UE_LOG(LogNHGame, Verbose, TEXT("NAIJA HUSTLE: audio mix %s %s"), NHAudio::MixNames[I], bOn ? TEXT("on") : TEXT("off"));
		}
	}

	// ---- the space: the roof wins, then the zone the listener stands in, then the open street
	ENHAudioSpace Want = ENHAudioSpace::Street;
	if (ForcedSpace != ENHAudioSpace::Count)
	{
		Want = Pending = ForcedSpace;
	}
	else
	{
		int32 Best = MIN_int32;
		for (int32 I = Inside.Num() - 1; I >= 0; --I)
		{
			const ANHAudioZone* Zone = Inside[I].Get();
			if (!Zone)
			{
				Inside.RemoveAtSwap(I);
			}
			else if (Zone->Priority > Best)
			{
				Best = Zone->Priority;
				Want = Zone->Space;
			}
		}
		if (Pawn && Want != ENHAudioSpace::Interior)
		{
			const ENHAudioSpace Roof = SpaceFromRoof(World, Pawn->GetActorLocation(), Pawn, PC->GetViewTarget());
			if (Roof != ENHAudioSpace::Street)
			{
				Want = Roof;
			}
		}
	}
	// twice running before it changes, so walking under a signboard does not flip the reverb
	if (Want == Pending && Want != Space)
	{
		ApplySpace(Want);
	}
	Pending = Want;
}

void UNHAudioSubsystem::ApplySpace(ENHAudioSpace NewSpace)
{
	const bool bFirst = Space == ENHAudioSpace::Count;
	Space = NewSpace;
	FMemory::Memcpy(SpaceFrom, SpaceNow, sizeof(SpaceFrom));
	SpaceBlend = bFirst ? 1.f : 0.f;
	if (bFirst)
	{
		FMemory::Memcpy(SpaceNow, &NHAudio::SpaceDefs[static_cast<int32>(Space)], sizeof(SpaceNow));
		PushSpace();
	}
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: audio space: %s"), SpaceName(NewSpace));
}

void UNHAudioSubsystem::PushSpace()
{
	NHAudio::FSpaceDef Def;
	FMemory::Memcpy(&Def, SpaceNow, sizeof(Def));

	// every band stays switched on (at 0 dB it does nothing), so a glide never switches one in or out with a click
	FSubmixEffectSubmixEQSettings Bands;
	const float Hz[] = { Def.LowHz, Def.MidHz, Def.HighHz }, Db[] = { Def.LowDb, Def.MidDb, Def.HighDb };
	for (int32 B = 0; B < 3; ++B)
	{
		FSubmixEffectEQBand Band;
		Band.Frequency = Hz[B];
		Band.Bandwidth = 2.f;
		Band.GainDb = Db[B];
		Band.bEnabled = true;
		Bands.EQBands.Add(Band);
	}
	WorldEQ->Settings = Bands; // the preset copies this over whatever SetSettings gave it the first time it is used, so set both
	WorldEQ->SetSettings(Bands);

	// the reverb sits on the World submix itself, so it passes the dry sound through (DryLevel 1) and adds its own on top
	FSubmixEffectReverbSettings R;
	R.DecayTime = Def.Decay;
	R.Density = Def.Density;
	R.Diffusion = Def.Diffusion;
	R.GainHF = Def.GainHF;
	R.WetLevel = Def.Wet;
	R.DryLevel = 1.f;
	R.Gain = 0.32f;            // the late reverb's own level: the engine's default for this struct is 0, which is silence
	R.ReflectionsGain = 0.5f;  // first echoes off nearby walls
	WorldReverb->Settings = R;
	WorldReverb->SetSettings(R);
}

ENHAudioSpace UNHAudioSubsystem::SpaceFromRoof(UWorld* World, const FVector& Listener, const AActor* IgnoreA, const AActor* IgnoreB) const
{
	FCollisionQueryParams Query(SCENE_QUERY_STAT(NHAudioRoof), false);
	Query.AddIgnoredActor(IgnoreA);
	Query.AddIgnoredActor(IgnoreB);
	const FVector From = Listener + FVector(0.f, 0.f, 120.f);
	// five rays up, one overhead and four 2.5 m out: a lamp post or a cable is not a roof
	static const FVector2D Offsets[] = { { 0.f, 0.f }, { 250.f, 0.f }, { -250.f, 0.f }, { 0.f, 250.f }, { 0.f, -250.f } };
	int32 Over = 0;
	float Height = 0.f;
	FHitResult Hit;
	for (const FVector2D& Offset : Offsets)
	{
		const FVector Start = From + FVector(Offset, 0.f);
		if (World->LineTraceSingleByChannel(Hit, Start, Start + FVector(0.f, 0.f, 3000.f), ECC_Visibility, Query))
		{
			++Over;
			Height += Hit.Distance;
		}
	}
	if (Over < 4)
	{
		return ENHAudioSpace::Street;
	}
	Height /= Over;
	// eight rays out at head height: how closed in it is
	bool bWall[8];
	int32 Walls = 0;
	for (int32 I = 0; I < 8; ++I)
	{
		const float Angle = I * UE_PI / 4.f;
		bWall[I] = World->LineTraceSingleByChannel(Hit, From, From + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * 1200.f, ECC_Visibility, Query);
		Walls += bWall[I] ? 1 : 0;
	}
	if (Walls >= 6 && Height < 800.f)
	{
		return ENHAudioSpace::Interior;
	}
	const bool bBothSides = (bWall[0] && bWall[4]) || (bWall[1] && bWall[5]) || (bWall[2] && bWall[6]) || (bWall[3] && bWall[7]);
	return bBothSides && Height < 900.f ? ENHAudioSpace::Tunnel : ENHAudioSpace::UnderBridge;
}

// ---------------------------------------------------------------------------------------------- the radio
void UNHAudioSubsystem::LoadStations()
{
	Stations.Reset();
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("NaijaHustleGame"));
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!Plugin || !FFileHelper::LoadFileToString(Text, *(Plugin->GetBaseDir() / TEXT("Data/radio_stations.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
	{
		UE_LOG(LogNHGame, Warning, TEXT("NAIJA HUSTLE: radio: Data/radio_stations.json could not be read"));
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (Root->TryGetArrayField(TEXT("stations"), List))
	{
		for (const TSharedPtr<FJsonValue>& Value : *List)
		{
			const TSharedPtr<FJsonObject> In = Value->AsObject();
			FRadioStation Station;
			Station.Id = FName(*In->GetStringField(TEXT("id")));
			Station.Name = In->GetStringField(TEXT("name"));
			const TArray<TSharedPtr<FJsonValue>>* Tracks = nullptr;
			if (In->TryGetArrayField(TEXT("tracks"), Tracks))
			{
				for (const TSharedPtr<FJsonValue>& T : *Tracks)
				{
					const TSharedPtr<FJsonObject> Track = T->AsObject();
					const FString File = Track->GetStringField(TEXT("file"));
					Station.Tracks.Add({ Track->GetStringField(TEXT("title")), Track->GetStringField(TEXT("artist")),
						FString::Printf(TEXT("/Game/NaijaHustle/Audio/Radio/%s/%s.%s"), *Station.Id.ToString(), *File, *File) });
				}
			}
			Stations.Add(MoveTemp(Station));
		}
	}
}

FString UNHAudioSubsystem::RadioNowPlaying() const
{
	if (!Stations.IsValidIndex(RadioStation) || !Stations[RadioStation].Tracks.IsValidIndex(RadioTrack))
	{
		return FString();
	}
	const FRadioTrack& Track = Stations[RadioStation].Tracks[RadioTrack];
	return FString::Printf(TEXT("%s: %s, %s"), *Stations[RadioStation].Name, *Track.Title, *Track.Artist);
}

void UNHAudioSubsystem::RadioAnnounce() const
{
	const FString Now = RadioNowPlaying();
	ANHHUD::Toast(GetTickableGameObjectWorld(), Now.IsEmpty() ? TEXT("Radio off") : *Now, 0);
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: radio: %s"), Now.IsEmpty() ? TEXT("off") : *Now);
}

void UNHAudioSubsystem::RadioOff()
{
	if (UAudioComponent* Voice = RadioVoice.Get())
	{
		Voice->FadeOut(0.3f, 0.f);
	}
	RadioVoice.Reset();
	RadioSound = nullptr;
	RadioStation = -1;
	RadioCar.Reset();
}

void UNHAudioSubsystem::RadioNextStation(ANHVehicle* Car)
{
	UWorld* World = GetTickableGameObjectWorld();
	if (!Car || !World || !bBuilt)
	{
		return;
	}
	// another car's radio starts from its first station; the same car's moves on, and past the last one is off
	const int32 Next = RadioCar.Get() == Car ? RadioStation + 1 : 0;
	RadioOff();
	if (Stations.IsValidIndex(Next))
	{
		RadioStation = Next;
		RadioTrack = 0;
		RadioCar = Car;
		RadioStart(World, 0.f);
	}
	RadioAnnounce();
}

void UNHAudioSubsystem::RadioNextTrack()
{
	UWorld* World = GetTickableGameObjectWorld();
	if (!World || !Stations.IsValidIndex(RadioStation) || Stations[RadioStation].Tracks.Num() == 0)
	{
		return;
	}
	RadioTrack = (RadioTrack + 1) % Stations[RadioStation].Tracks.Num();
	RadioStart(World, 0.f);
	RadioAnnounce();
}

void UNHAudioSubsystem::RadioStart(UWorld* World, float From)
{
	if (UAudioComponent* Voice = RadioVoice.Get())
	{
		Voice->Stop();
	}
	RadioVoice.Reset();
	const FRadioStation& Station = Stations[RadioStation];
	ANHVehicle* Car = RadioCar.Get();
	if (!Car || !Station.Tracks.IsValidIndex(RadioTrack))
	{
		return;
	}
	if (From <= 0.f || !RadioSound)
	{
		RadioSound = LoadObject<USoundBase>(nullptr, *Station.Tracks[RadioTrack].Asset);
		RadioLength = RadioSound ? RadioSound->GetDuration() : 0.f;
		if (!RadioSound)
		{
			UE_LOG(LogNHGame, Warning, TEXT("NAIJA HUSTLE: radio: %s is not in the project (run Scripts/import_radio.py)"), *Station.Tracks[RadioTrack].Asset);
			RadioLength = 3.f; // so the playlist moves on to a song that is there
		}
	}
	RadioStartedAt = World->GetAudioTimeSeconds() - From;
	const APlayerController* PC = World->GetFirstPlayerController();
	bRadioCabin = PC && PC->GetPawn() == Car;
	UAudioComponent* Voice = RadioSound ? Make(RadioSound, bRadioCabin ? ENHSoundKind::RadioCabin : ENHSoundKind::RadioWorld, World, 1.f, 1.f) : nullptr;
	if (Voice)
	{
		if (!bRadioCabin)
		{
			Voice->AttachToComponent(Car->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
		Voice->Play(FMath::Max(From, 0.f));
		RadioVoice = Voice;
	}
}

void UNHAudioSubsystem::RadioWatch(UWorld* World, const ANHVehicle* Driving)
{
	if (RadioStation < 0)
	{
		return;
	}
	if (!RadioCar.IsValid())
	{
		RadioOff(); // the car is gone
		return;
	}
	const float At = World->GetAudioTimeSeconds() - RadioStartedAt;
	if (At >= RadioLength - 0.1f)
	{
		// the song is over: the next one on the playlist, round and round
		RadioTrack = (RadioTrack + 1) % FMath::Max(1, Stations[RadioStation].Tracks.Num());
		RadioStart(World, 0.f);
		if (Driving == RadioCar.Get())
		{
			RadioAnnounce();
		}
	}
	else if ((Driving == RadioCar.Get()) != bRadioCabin || !RadioVoice.IsValid())
	{
		RadioStart(World, At); // got in or out: the same song from the same place, as the cabin's radio or from the car
	}
}

void UNHAudioSubsystem::SetRadioCar(ANHVehicle* Car)
{
	RadioCar = Car;
}

void UNHAudioSubsystem::ZoneChanged(ANHAudioZone* Zone, bool bInside)
{
	if (bInside)
	{
		Inside.AddUnique(Zone);
	}
	else
	{
		Inside.Remove(Zone);
	}
}

FString UNHAudioSubsystem::Describe() const
{
	if (!bBuilt)
	{
		return TEXT("audio: not built (no sound device, or the level has not started)");
	}
	FString Out = TEXT("audio\n  classes:");
	for (int32 I = 0; I < Classes.Num(); ++I)
	{
		const ENHSoundClass Parent = NHAudio::ClassDefs[I].Parent;
		Out += FString::Printf(TEXT(" %s%s%s"), NHAudio::ClassNames[I], Parent != ENHSoundClass::Count ? TEXT("<") : TEXT(""), Parent != ENHSoundClass::Count ? NHAudio::ClassNames[static_cast<int32>(Parent)] : TEXT(""));
	}
	Out += TEXT("\n  volumes:");
	for (int32 I = 0; I < static_cast<int32>(ENHVolume::Count); ++I)
	{
		Out += FString::Printf(TEXT(" %s %d"), NHAudio::VolumeNames[I], Volumes[I]);
	}
	Out += FString::Printf(TEXT("\n  subtitles %s, size %d, mono %s\n  mixes on:"), bSubtitles ? TEXT("on") : TEXT("off"), SubtitleSize, bMono ? TEXT("on") : TEXT("off"));
	for (int32 I = 0; I < static_cast<int32>(ENHMix::Count); ++I)
	{
		if (bPushed[I])
		{
			Out += FString::Printf(TEXT(" %s"), NHAudio::MixNames[I]);
		}
	}
	Out += FString::Printf(TEXT("\n  radio: %s (%d stations)"), RadioOn() ? *RadioNowPlaying() : TEXT("off"), Stations.Num());
	Out += FString::Printf(TEXT("\n  space %s%s, zones the listener is in: %d\n  voices %d of %d"), SpaceName(Space), ForcedSpace != ENHAudioSpace::Count ? TEXT(" (held)") : TEXT(""),
		Inside.Num(), ActiveVoices(), VoiceBudget);
	return Out;
}
