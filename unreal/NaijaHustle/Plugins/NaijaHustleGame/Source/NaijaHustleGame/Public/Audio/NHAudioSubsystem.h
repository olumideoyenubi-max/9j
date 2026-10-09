#pragma once

#include "CoreMinimal.h"
#include "Audio/NHAudioTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "NHAudioSubsystem.generated.h"

class ANHAudioZone;
class ANHVehicle;
class UAudioComponent;
class USoundAttenuation;
class USoundBase;
class USoundClass;
class USoundConcurrency;
class USoundMix;
class USoundSubmix;
class USubmixEffectReverbPreset;
class USubmixEffectSubmixEQPreset;
class UNHSubmixEffectMonoPreset;

/**
 * The game's audio mix. Everything it needs is made in code the first time a level plays, so the project carries no
 * audio assets for it (Content/ is not in the repo):
 *
 *   Sound Classes   the tree in ENHSoundClass, each with its submix
 *   Submixes        Music, Radio, Dialogue, World, UI, Voice, under the engine's main submix. World carries one EQ
 *                   and one reverb, set to the space the listener is in (ENHAudioSpace) and gliding over a second
 *                   and a half when that changes; the others are dry. One reverb for the whole city, not one a
 *                   space: an 8 GB Mac pays for each.
 *   Sound Mixes     ENHMix (dialogue, phone call, car cabin, muffled radio) and the one holding the volume sliders
 *   Attenuation, concurrency and priority   one of each per ENHSoundKind
 *
 * Play sounds through Play, PlayAt and PlayAttached and they get all of that. The subsystem watches the game every
 * quarter second and pushes the mixes itself: a mission line or a call ducks the rest, a closed car dulls the street.
 *
 * The space comes from the audio zones the listener stands in (ANHAudioZone, Audio Gameplay Volumes) and, where there
 * is no zone, from what is overhead: a slab above with walls round is an interior, a low one with walls on two sides
 * a tunnel, anything else a bridge.
 *
 * Settings (pause menu, Audio): seven volumes, subtitles on or off and their size, mono. Kept in GameUserSettings.ini
 * under [NaijaHustle]. Console: NHAudio prints the state, NHAudioSpace <name|auto> holds a space, NHAudioTest runs
 * the recorded check (ANHAudioTest).
 */
UCLASS()
class NAIJAHUSTLEGAME_API UNHAudioSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	static UNHAudioSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---- FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UNHAudioSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return !IsTemplate(); }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual UWorld* GetTickableGameObjectWorld() const override;

	// ---- playing
	/** Not placed in the world: music, UI, the phone, a mission line. Null if there is no sound device. */
	UAudioComponent* Play(USoundBase* Sound, ENHSoundKind Kind, float Volume = 1.f, float Pitch = 1.f);
	UAudioComponent* PlayAt(USoundBase* Sound, ENHSoundKind Kind, const FVector& Location, float Volume = 1.f, float Pitch = 1.f);
	UAudioComponent* PlayAttached(USoundBase* Sound, ENHSoundKind Kind, USceneComponent* To, float Volume = 1.f, float Pitch = 1.f);

	USoundClass* GetClass(ENHSoundClass Class) const { return Classes[static_cast<int32>(Class)]; }
	USoundSubmix* GetSubmix(ENHSubmix Submix) const { return Submixes[static_cast<int32>(Submix)]; }
	USoundAttenuation* GetAttenuation(ENHSoundKind Kind) const { return Attenuations[static_cast<int32>(Kind)]; }
	USoundConcurrency* GetConcurrency(ENHSoundKind Kind) const { return Concurrencies[static_cast<int32>(Kind)]; }
	static ENHSoundClass ClassOf(ENHSoundKind Kind);
	/** How many of a kind may sound at once */
	static int32 MaxVoices(ENHSoundKind Kind);
	/** Every kind together: what the Mac is asked to mix at most */
	static constexpr int32 VoiceBudget = 32;

	// ---- settings
	/** 0..100 */
	int32 GetVolume(ENHVolume Slider) const { return Volumes[static_cast<int32>(Slider)]; }
	void SetVolume(ENHVolume Slider, int32 Percent);
	bool SubtitlesOn() const { return bSubtitles; }
	void SetSubtitles(bool bOn);
	/** 0 small, 1 medium, 2 large */
	int32 GetSubtitleSize() const { return SubtitleSize; }
	void SetSubtitleSize(int32 Size);
	/** What the HUD multiplies subtitle text by */
	float SubtitleScale() const { return SubtitleSize == 0 ? 0.85f : SubtitleSize == 2 ? 1.3f : 1.f; }
	/** A line with no recorded voice is always shown, or nobody could follow it; one with a voice follows the setting */
	bool ShowSubtitle(bool bHasVoice) const { return bSubtitles || !bHasVoice; }
	bool MonoOn() const { return bMono; }
	void SetMono(bool bOn);

	// ---- the radio: stations and playlists from Data/radio_stations.json, music imported by Scripts/import_radio.py
	struct FRadioTrack
	{
		FString Title, Artist, Asset;
	};
	struct FRadioStation
	{
		FName Id;
		FString Name;
		TArray<FRadioTrack> Tracks;
	};
	const TArray<FRadioStation>& GetStations() const { return Stations; }
	/** R in a car: the next station on that car's radio, and off after the last. The radio stays with the car: get out and you hear it from outside, muffled. */
	void RadioNextStation(ANHVehicle* Car);
	/** That station (an index into GetStations), on that car's radio or, with no car, on the phone: heard anywhere, by you alone */
	void RadioPlay(int32 Station, ANHVehicle* Car);
	int32 RadioStationIndex() const { return RadioStation; }
	bool RadioOnPhone() const { return RadioStation >= 0 && bRadioPhone; }
	/** T: the next song on the station */
	void RadioNextTrack();
	void RadioOff();
	bool RadioOn() const { return RadioStation >= 0; }
	/** "Ragebait FM: Bands, Tommy Ringz", or "" if it is off */
	FString RadioNowPlaying() const;

	// ---- what the game tells it
	/** The car whose radio is playing (null: none). You hear it clearly only from inside. */
	void SetRadioCar(ANHVehicle* Car);
	/** Holds a mix on whatever the game is doing, for tests: 1 on, 0 off, -1 back to the game */
	void ForceMix(ENHMix Mix, int32 State) { Forced[static_cast<int32>(Mix)] = State; }
	/** Holds a space whatever the listener is standing in; Count gives it back to the zones */
	void ForceSpace(ENHAudioSpace InSpace) { ForcedSpace = InSpace; }
	ENHAudioSpace GetSpace() const { return Space; }
	bool MixOn(ENHMix Mix) const { return bPushed[static_cast<int32>(Mix)]; }
	/** Called by ANHAudioZone as the listener crosses its edge */
	void ZoneChanged(ANHAudioZone* Zone, bool bInside);

	static const TCHAR* SpaceName(ENHAudioSpace Space);
	static const TCHAR* VolumeName(ENHVolume Slider);
	/** Sounds the device is mixing right now */
	int32 ActiveVoices() const;
	/** A few lines for the log: classes, sliders, mixes, space, voices */
	FString Describe() const;
	bool IsBuilt() const { return bBuilt; }

private:
	bool bBuilt = false;
	/** The level the mixes, the sliders and the space were last applied in */
	TWeakObjectPtr<UWorld> AppliedTo;
	void Build(UWorld* World);
	void BuildClasses();
	void BuildSubmixes(UWorld* World);
	void BuildMixes();
	void BuildKinds();
	void BuildSpaces();
	UAudioComponent* Make(USoundBase* Sound, ENHSoundKind Kind, UWorld* World, float Volume, float Pitch);

	UPROPERTY(Transient) TArray<TObjectPtr<USoundClass>> Classes;
	UPROPERTY(Transient) TArray<TObjectPtr<USoundSubmix>> Submixes;
	UPROPERTY(Transient) TArray<TObjectPtr<USoundMix>> Mixes;
	/** The volume sliders: always pushed */
	UPROPERTY(Transient) TObjectPtr<USoundMix> UserMix;
	UPROPERTY(Transient) TArray<TObjectPtr<USoundAttenuation>> Attenuations;
	UPROPERTY(Transient) TArray<TObjectPtr<USoundConcurrency>> Concurrencies;
	/** The World submix's two effects. Their settings glide to the current space's (NHAudio::SpaceDefs). */
	UPROPERTY(Transient) TObjectPtr<USubmixEffectSubmixEQPreset> WorldEQ;
	UPROPERTY(Transient) TObjectPtr<USubmixEffectReverbPreset> WorldReverb;
	UPROPERTY(Transient) TObjectPtr<UNHSubmixEffectMonoPreset> MonoEffect;
	bool bMonoApplied = false;

	// ---- settings
	int32 Volumes[static_cast<int32>(ENHVolume::Count)];
	bool bSubtitles = true;
	int32 SubtitleSize = 1;
	bool bMono = false;
	void LoadSettings();
	void SaveSettings() const;
	void ApplyVolumes(UWorld* World);
	void ApplyMono(UWorld* World);

	// ---- the mixes and the space, worked out every quarter second
	float Since = 1.f;
	bool bPushed[static_cast<int32>(ENHMix::Count)] = {};
	int32 Forced[static_cast<int32>(ENHMix::Count)];
	void Watch(UWorld* World);
	TWeakObjectPtr<ANHVehicle> RadioCar;
	TArray<FRadioStation> Stations;
	void LoadStations();
	int32 RadioStation = -1, RadioTrack = 0;
	/** When the song began, on the world's audio clock (which stops while the game is paused), and how long it is */
	float RadioStartedAt = 0.f, RadioLength = 0.f;
	/** The song is playing as the cabin's own radio (not placed in the world), as against from the car, heard outside */
	bool bRadioCabin = false;
	/** Playing from the phone, not a car */
	bool bRadioPhone = false;
	UPROPERTY(Transient) TObjectPtr<USoundBase> RadioSound;
	TWeakObjectPtr<UAudioComponent> RadioVoice;
	void RadioStart(UWorld* World, float From);
	void RadioWatch(UWorld* World, const ANHVehicle* Driving);
	void RadioAnnounce() const;
	ENHAudioSpace Space = ENHAudioSpace::Count; // nothing applied yet
	ENHAudioSpace ForcedSpace = ENHAudioSpace::Count;
	ENHAudioSpace Pending = ENHAudioSpace::Street;
	TArray<TWeakObjectPtr<ANHAudioZone>> Inside;
	void ApplySpace(ENHAudioSpace NewSpace);
	/** The space's numbers as they sound right now, part way from the last space to this one, and how far along (1 = there) */
	float SpaceNow[11] = {}, SpaceFrom[11] = {};
	float SpaceBlend = 1.f;
	void PushSpace();
	/** What is overhead and round the listener, where no zone says: Street if open sky */
	ENHAudioSpace SpaceFromRoof(UWorld* World, const FVector& Listener, const AActor* IgnoreA, const AActor* IgnoreB) const;
};
