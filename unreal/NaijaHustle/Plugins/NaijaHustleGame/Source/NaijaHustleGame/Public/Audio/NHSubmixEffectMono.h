#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundEffectSubmix.h"
#include "NHSubmixEffectMono.generated.h"

USTRUCT(BlueprintType)
struct FNHSubmixEffectMonoSettings
{
	GENERATED_BODY()
	/** 0 leaves the stereo picture alone, 1 puts the same sound in both ears */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mono", meta = (ClampMin = "0", ClampMax = "1")) float Amount = 1.f;
};

/** Folds left and right into one, for the Mono audio setting: somebody who hears with one ear misses nothing */
class FNHSubmixEffectMono : public FSoundEffectSubmix
{
public:
	virtual void Init(const FSoundEffectSubmixInitData& InData) override {}
	virtual void OnPresetChanged() override;
	virtual void OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData) override;

private:
	float Amount = 1.f;
};

UCLASS()
class NAIJAHUSTLEGAME_API UNHSubmixEffectMonoPreset : public USoundEffectSubmixPreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(NHSubmixEffectMono)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mono", meta = (ShowOnlyInnerProperties)) FNHSubmixEffectMonoSettings Settings;
};
