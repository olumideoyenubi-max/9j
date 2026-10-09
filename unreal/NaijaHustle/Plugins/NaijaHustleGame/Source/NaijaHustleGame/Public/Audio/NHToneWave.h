#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "NHToneWave.generated.h"

UENUM()
enum class ENHTone : uint8
{
	/** A steady pitch */
	Sine,
	/** Hiss: every pitch at once */
	Noise,
	/** One short burst of hiss a third of a second in, then silence: for hearing a space ring */
	Burst
};

/**
 * PLACEHOLDER SOUND. A test signal worked out sample by sample, with no file behind it: a tone, hiss, or one burst.
 * ANHAudioTest plays these through the mix to check it; no game sound should be one. A wave plays in one place at a
 * time, so make a new one for every sound.
 */
UCLASS()
class NAIJAHUSTLEGAME_API UNHToneWave : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	UNHToneWave(const FObjectInitializer& ObjectInitializer);
	static UNHToneWave* Make(UObject* Outer, ENHTone Shape, float Hz, float Gain = 0.25f);

	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;
	virtual Audio::EAudioMixerStreamDataFormat::Type GetGeneratedPCMDataFormat() const override { return Audio::EAudioMixerStreamDataFormat::Int16; }

private:
	static constexpr int32 Rate = 48000;
	// set before it plays and only read after
	ENHTone Shape = ENHTone::Sine;
	float Hz = 440.f;
	float Gain = 0.25f;
	// the audio thread's own
	double Phase = 0.0;
	int64 Done = 0;
	uint32 Seed = 22222;
};
