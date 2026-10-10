#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include <atomic>
#include "NHEngineWave.generated.h"

/** What kind of engine it is: how many times a turn it fires, how rough it runs, how deep it sounds */
UENUM()
enum class ENHEngine : uint8
{
	/** A saloon or an SUV: four cylinders, smooth */
	Car,
	/** A danfo or a truck: a diesel, low and rattling */
	Diesel,
	/** An okada: one cylinder, buzzing */
	Bike,
	/** A keke: a small two-stroke, putting */
	Keke,
	/** A speedboat's outboard */
	Outboard
};

/**
 * A vehicle's engine, worked out sample by sample for as long as it runs: there is no recording behind it. The firing
 * note and its harmonics rise with the revs; under load it is louder and rougher; at idle it lopes. ANHVehicle makes
 * one while it is near enough to hear and tells it the revs and the throttle every frame.
 */
UCLASS()
class NAIJAHUSTLEGAME_API UNHEngineWave : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	UNHEngineWave(const FObjectInitializer& ObjectInitializer);
	static UNHEngineWave* Make(UObject* Outer, ENHEngine Kind);
	static ENHEngine KindFor(FName VehicleType, bool bBike, bool bBoat);
	/** Revs 0 (idle) .. 1 (flat out) and load 0 (coasting) .. 1 (foot down); read by the audio thread */
	void Set(float NewRevs, float NewLoad) { WantRevs.store(FMath::Clamp(NewRevs, 0.f, 1.f)); WantLoad.store(FMath::Clamp(NewLoad, 0.f, 1.f)); }
	/** The firing note at those revs, Hz: for tests */
	float NoteAt(float AtRevs) const { return FMath::Lerp(IdleHz, TopHz, AtRevs); }

	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;
	virtual Audio::EAudioMixerStreamDataFormat::Type GetGeneratedPCMDataFormat() const override { return Audio::EAudioMixerStreamDataFormat::Int16; }

private:
	static constexpr int32 Rate = 24000;
	// set before it plays and only read after
	float IdleHz = 28.f, TopHz = 110.f, Rough = 0.15f, Bright = 0.5f;
	std::atomic<float> WantRevs{ 0.f }, WantLoad{ 0.f };
	// the audio thread's own
	double Phase = 0.0;
	float Revs = 0.f, Load = 0.f, Low = 0.f;
	uint32 Seed = 9001;
};

/** One of the cast's or a passer-by's voice: how high, how fast, how big the mouth */
struct FNHVoice
{
	float Hz = 120.f, Speed = 1.f, Formant = 1.f;
};

/**
 * Speech without recordings: a line is turned into syllables and sung through two resonances, a vowel at a time, with
 * consonants as murmurs, hisses and stops. It cannot be understood (the words are on the screen) but it sounds like
 * somebody talking, and each person has their own voice. Yoruba's tone marks are followed: an acute accent is said
 * higher, a grave one lower, so "Ẹ káàárọ̀" rises and falls the way it is written.
 */
namespace NHVoice
{
	constexpr int32 Rate = 24000;
	/** The samples of a line in that voice */
	NAIJAHUSTLEGAME_API TArray<int16> Make(const FString& Line, const FNHVoice& Voice);
	/** A voice for a name: the cast's own (Data/characters.json "voice", by first name too), otherwise one made from the name, a woman's or a man's by bWoman */
	NAIJAHUSTLEGAME_API FNHVoice For(const FString& Who, bool bWoman = false);
	/** A horn: two notes together, short. Danfos and trucks are lower. */
	NAIJAHUSTLEGAME_API TArray<int16> Horn(bool bBig);
}

/**
 * What people in the street say (Data/barks.json): each kind of line has Yoruba ones, with their English beside them,
 * and Pidgin ones. Pick gives one as it is shown: "Ẹ káàárọ̀ o!  (Good morning)".
 */
namespace NHBarks
{
	NAIJAHUSTLEGAME_API FString Pick(FName Kind);
	/** The part that is said aloud: without the English in brackets */
	NAIJAHUSTLEGAME_API FString Spoken(const FString& Shown);
	NAIJAHUSTLEGAME_API int32 Count(FName Kind);
}
