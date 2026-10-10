#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "NHWeaponSounds.generated.h"

/** The weapon sounds the game makes for itself */
UENUM()
enum class ENHShot : uint8
{
	Pistol,
	/** What a pistol shot leaves in the street after it: the crack coming back off the buildings */
	PistolTail,
	Rifle,
	RifleTail,
	/** A bullet arriving: a dull knock and grit */
	BulletHit,
	/** A bullet or a blade arriving in somebody: dull, no ring */
	BodyHit,
	MacheteSwing,
	/** The blade on something hard */
	MacheteHit,
	/** Taking a gun out or putting it away: cloth and a click */
	Draw,
	/** The same for the machete: a short scrape of steel */
	DrawBlade,
	Count UMETA(Hidden)
};

/**
 * ORIGINAL SOUNDS, MADE BY ARITHMETIC. No recording, no file, nothing from anybody else's game: each sound is worked
 * out sample by sample from noise, a few sine waves and filters (NHWeaponSounds::Make), three takes of each so the
 * same shot is not heard twice running. They stand in for recorded and designed sounds, which can replace them one
 * for one later.
 */
namespace NHWeaponSounds
{
	constexpr int32 Rate = 48000;
	constexpr int32 Takes = 3;
	/** One take of a sound, mono, 16-bit */
	NAIJAHUSTLEGAME_API TArray<int16> Make(ENHShot Shot, int32 Take);
}

/** One playing of one take: a wave that hands the engine a ready-made buffer once and then is silent. Make a new one for every sound. */
UCLASS()
class NAIJAHUSTLEGAME_API UNHShotWave : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	UNHShotWave(const FObjectInitializer& ObjectInitializer);
	static UNHShotWave* Make(UObject* Outer, const TArray<int16>& Samples);
	/** The same for samples at another rate (the voices and the horn are worked out at 24 kHz) */
	static UNHShotWave* MakeAt(UObject* Outer, const TArray<int16>& Samples, int32 SampleRate);
	/** How long the sound is, seconds */
	float Length = 0.f;
};
