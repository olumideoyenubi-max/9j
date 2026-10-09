#pragma once

#include "CoreMinimal.h"
#include "NHAudioTypes.generated.h"

/**
 * The Sound Class tree: Master > Music, Radio, Dialogue (> Barks), SFX (> Vehicles, Weapons, Footsteps, UI, Phone),
 * Ambience, Voice Chat. Barks is the one class the brief does not name: passers-by shouting must follow the Dialogue
 * slider without ducking the music the way a mission line does.
 */
UENUM(BlueprintType)
enum class ENHSoundClass : uint8
{
	Master,
	Music,
	Radio,
	Dialogue,
	Barks,
	SFX,
	Vehicles,
	Weapons,
	Footsteps,
	UI,
	Phone,
	Ambience,
	VoiceChat,
	Count UMETA(Hidden)
};

/** Where a class's sound is mixed. World is the only one with a space's reverb and EQ on it. */
UENUM(BlueprintType)
enum class ENHSubmix : uint8
{
	Music,
	Radio,
	Dialogue,
	World,
	UI,
	Voice,
	Count UMETA(Hidden)
};

/** The acoustic space round the listener: one reverb and one EQ each, on the World submix */
UENUM(BlueprintType)
enum class ENHAudioSpace : uint8
{
	Street,
	Market,
	MotorPark,
	Interior,
	UnderBridge,
	Tunnel,
	Count UMETA(Hidden)
};

/** The volume sliders of the settings page */
UENUM(BlueprintType)
enum class ENHVolume : uint8
{
	Master,
	Music,
	Radio,
	Dialogue,
	SFX,
	Ambience,
	VoiceChat,
	Count UMETA(Hidden)
};

/**
 * What a sound is, for UNHAudioSubsystem::Play: it picks the class, whether it is placed in the world, how far it
 * carries, whether walls muffle it, how many of its kind may sound at once and who gives way when the Mac runs out
 * of voices.
 */
UENUM(BlueprintType)
enum class ENHSoundKind : uint8
{
	Music,
	/** The radio of the car you are sitting in, the phone's music app */
	RadioCabin,
	/** A radio heard from outside: a shop, a car going past */
	RadioWorld,
	/** A mission or cutscene line */
	Dialogue,
	Bark,
	VehicleEngine,
	Horn,
	/** Doors, locks, clunks, rattles */
	VehicleDetail,
	Weapon,
	/** The distant crack and echo of a shot */
	WeaponTail,
	Impact,
	Footstep,
	UI,
	Phone,
	/** A long stereo bed: the city, rain, night */
	AmbienceBed,
	/** One thing in the world: a generator, a hawker, a rooster */
	AmbienceSpot,
	Crowd,
	VoiceChat,
	Count UMETA(Hidden)
};

/** The Sound Mixes the game pushes and pops by what is going on */
UENUM()
enum class ENHMix : uint8
{
	/** A mission line is being spoken: music, radio and ambience drop */
	Dialogue,
	/** A phone call: everything else drops */
	PhoneCall,
	/** Sitting in a closed car: the street is quieter and duller */
	Cabin,
	/** The radio is playing in a car you are not in, or behind closed windows */
	RadioMuffled,
	Count UMETA(Hidden)
};
