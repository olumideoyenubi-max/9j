#include "Audio/NHToneWave.h"

UNHToneWave::UNHToneWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	SetSampleRate(Rate);
	Duration = INDEFINITELY_LOOPING_DURATION;
	bLooping = true;
	SoundGroup = SOUNDGROUP_Default;
}

UNHToneWave* UNHToneWave::Make(UObject* Outer, ENHTone InShape, float InHz, float InGain)
{
	UNHToneWave* Wave = NewObject<UNHToneWave>(Outer);
	Wave->Shape = InShape;
	Wave->Hz = InHz;
	Wave->Gain = InGain;
	return Wave;
}

int32 UNHToneWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	OutAudio.Reset();
	OutAudio.AddZeroed(NumSamples * sizeof(int16));
	int16* Out = reinterpret_cast<int16*>(OutAudio.GetData());
	const double Step = 2.0 * UE_DOUBLE_PI * Hz / Rate;
	const int64 BurstFrom = Rate / 3, BurstTo = BurstFrom + Rate * 6 / 100; // 60 ms
	for (int32 I = 0; I < NumSamples; ++I, ++Done)
	{
		float V = 0.f;
		if (Shape == ENHTone::Sine)
		{
			V = FMath::Sin(Phase);
			Phase += Step;
			if (Phase > 2.0 * UE_DOUBLE_PI)
			{
				Phase -= 2.0 * UE_DOUBLE_PI;
			}
		}
		else if (Shape == ENHTone::Noise || (Done >= BurstFrom && Done < BurstTo))
		{
			Seed = Seed * 1664525u + 1013904223u;
			V = static_cast<float>(Seed >> 8) / 8388608.f - 1.f;
		}
		const float Fade = FMath::Min(1.f, Done / (Rate * 0.02f)); // 20 ms in, so it does not click
		Out[I] = static_cast<int16>(FMath::Clamp(V * Gain * Fade, -1.f, 1.f) * 32767.f);
	}
	return NumSamples;
}
