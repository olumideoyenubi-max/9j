#include "Audio/NHSubmixEffectMono.h"

void FNHSubmixEffectMono::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(NHSubmixEffectMono);
	Amount = FMath::Clamp(Settings.Amount, 0.f, 1.f);
}

void FNHSubmixEffectMono::OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData)
{
	const int32 Channels = InData.NumChannels, Samples = InData.NumFrames * Channels;
	const float* In = InData.AudioBuffer->GetData();
	float* Out = OutData.AudioBuffer->GetData();
	FMemory::Memcpy(Out, In, Samples * sizeof(float));
	if (Channels < 2 || Amount <= 0.f)
	{
		return;
	}
	// front left and front right are the first two channels of every layout; anything else (centre, surrounds) is left alone
	for (int32 I = 0; I < Samples; I += Channels)
	{
		const float Mid = 0.5f * (In[I] + In[I + 1]);
		Out[I] = FMath::Lerp(In[I], Mid, Amount);
		Out[I + 1] = FMath::Lerp(In[I + 1], Mid, Amount);
	}
}
