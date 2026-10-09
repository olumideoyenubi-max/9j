#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NHAudioTest.generated.h"

class UAudioComponent;

/**
 * The audio check. It plays placeholder test tones (UNHToneWave) through the game's mix, one situation after
 * another, and records what comes out of the speakers to a WAV file, with the time each situation started and ended
 * beside it. Scripts/audio_check.py then measures the recording: is the music quieter under a line of dialogue, is
 * the muffled radio duller, does a wall cut a sound, does the tunnel ring longer than the street.
 *
 *   Scripts/mac.sh city -NHAudioTest            runs it where the player starts (by Oshodi Motor Park) and quits
 *   ... -NHAudioTest -NHAudioAt=oshoja          goes to that stop first (any name NHGoto takes)
 *   NHAudioTest                                 the same from the console, without quitting
 *   python3 Plugins/NaijaHustleGame/Scripts/audio_check.py Saved/NHAudio
 *
 * It writes Saved/NHAudio/nh_audio_test.wav and nh_audio_test.json, and logs "[audiotest]" lines. The sliders are set
 * to full while it runs and put back after.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHAudioTest : public AActor
{
	GENERATED_BODY()

public:
	ANHAudioTest();
	virtual void Tick(float DeltaSeconds) override;
	bool bQuitWhenDone = false;

private:
	struct FPhase
	{
		FString Name;
		float Seconds = 2.5f;
		TFunction<void()> Start;
	};
	TArray<FPhase> Phases;
	TArray<TPair<FString, FVector2D>> Marks; // name, start and end in seconds of recording
	int32 Index = -1;
	float Wait = 6.f, Left = 0.f, QuitIn = -1.f;
	double RecordedFrom = 0.0;
	bool bDone = false, bWent = false;

	UPROPERTY(Transient) TMap<FName, TObjectPtr<UAudioComponent>> Playing;
	UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Flood;
	UPROPERTY(Transient) TObjectPtr<AActor> Wall;
	int32 SavedVolumes[8] = {};
	bool bSavedMono = false;
	int32 FloodAsked = 0, FloodPlaying = -1, FloodVoices = 0, VoicesPeak = 0;
	FString StartSpace, ZoneNote;
	int32 Frames = 0;
	double FrameSeconds = 0.0;

	void Begin();
	void Finish();
	void Stop(FName Id);
	/** Where the listener is, and which way is forward and right for it along the ground, and up */
	void View(FVector& At, FVector& Forward, FVector& Right, FVector& Up) const;
};
