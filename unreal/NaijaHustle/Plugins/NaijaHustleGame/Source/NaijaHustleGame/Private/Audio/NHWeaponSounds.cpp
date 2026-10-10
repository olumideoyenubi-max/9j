#include "Audio/NHWeaponSounds.h"

namespace NHWeaponSounds
{
	/** The building blocks: hiss, a one-pole low-pass, a two-pole band-pass, and a soft limit */
	struct FVoice
	{
		uint32 Seed;
		float Low = 0.f, BandLow = 0.f, Band = 0.f;
		explicit FVoice(uint32 InSeed) : Seed(InSeed) {}
		float Noise()
		{
			Seed = Seed * 1664525u + 1013904223u;
			return static_cast<float>(Seed >> 8) / 8388608.f - 1.f;
		}
		/** 0..1 */
		float Chance() { return Noise() * 0.5f + 0.5f; }
		float LowPass(float In, float Hz)
		{
			Low += (1.f - FMath::Exp(-2.f * UE_PI * Hz / Rate)) * (In - Low);
			return Low;
		}
		float BandPass(float In, float Hz, float Q)
		{
			const float F = 2.f * FMath::Sin(UE_PI * FMath::Min(Hz, 10000.f) / Rate);
			BandLow += F * Band;
			const float High = In - BandLow - Band / Q;
			Band += F * High;
			return Band;
		}
	};

	float Decay(float T, float Tau) { return FMath::Exp(-T / Tau); }

	/** A gun going off: the crack, the body of the bang, the thump in the chest, and the action working */
	void Gun(TArray<float>& Out, FVoice& V, float Seconds, float ThumpFrom, float ThumpTo, float ThumpTau, float BangTau, float BangHz, float ActionAt, float ActionHz)
	{
		const int32 N = FMath::RoundToInt(Seconds * Rate);
		Out.SetNumZeroed(N);
		FVoice Body(V.Seed ^ 0x51ed27u), Action(V.Seed ^ 0x9e3779u);
		double Phase = 0.0;
		for (int32 I = 0; I < N; ++I)
		{
			const float T = static_cast<float>(I) / Rate;
			const float Crack = V.Noise() * Decay(T, 0.0025f);                                    // the first three thousandths of a second
			const float Bang = Body.LowPass(Body.Noise(), BangHz) * Decay(T, BangTau) * 2.2f;     // the bang itself
			Phase += 2.0 * UE_DOUBLE_PI * FMath::Lerp(ThumpTo, ThumpFrom, Decay(T, ThumpTau * 0.5f)) / Rate;
			const float Thump = FMath::Sin(Phase) * Decay(T, ThumpTau) * 0.9f;                    // a falling low note
			const float TA = T - ActionAt;
			const float Works = TA > 0.f ? Action.BandPass(Action.Noise(), ActionHz, 6.f) * Decay(TA, 0.012f) * 0.5f : 0.f; // the slide or bolt
			Out[I] = Crack * 0.9f + Bang + Thump + Works;
		}
	}

	/** The shot coming back off the street: dull hiss dying slowly, with a few separate returns */
	void Tail(TArray<float>& Out, FVoice& V, float Seconds, float Hz, float Tau)
	{
		const int32 N = FMath::RoundToInt(Seconds * Rate);
		Out.SetNumZeroed(N);
		const float Returns[] = { 0.07f + 0.03f * V.Chance(), 0.16f + 0.04f * V.Chance(), 0.27f + 0.06f * V.Chance(), 0.45f + 0.1f * V.Chance() };
		for (int32 I = 0; I < N; ++I)
		{
			const float T = static_cast<float>(I) / Rate;
			float Level = Decay(T, Tau) * FMath::Min(1.f, T / 0.03f) * 0.5f;
			for (int32 R = 0; R < 4; ++R)
			{
				const float TR = T - Returns[R];
				Level += TR > 0.f ? Decay(TR, 0.05f) * (0.8f - 0.15f * R) : 0.f;
			}
			Out[I] = V.LowPass(V.Noise(), Hz) * Level * 1.6f;
		}
	}

	TArray<int16> Make(ENHShot Shot, int32 Take)
	{
		FVoice V(7919u * (static_cast<uint32>(Shot) + 1u) + 104729u * static_cast<uint32>(Take + 1));
		const float Vary = 1.f + 0.06f * (Take - 1); // each take a little higher or lower
		TArray<float> Out;
		float Gain = 0.8f;
		switch (Shot)
		{
		case ENHShot::Pistol:
			Gun(Out, V, 0.32f, 240.f * Vary, 70.f, 0.03f, 0.022f, 3800.f, 0.045f, 3200.f * Vary);
			break;
		case ENHShot::Rifle: // heavier, lower, longer, and the bolt slams home
			Gun(Out, V, 0.42f, 190.f * Vary, 48.f, 0.055f, 0.034f, 2600.f, 0.06f, 2100.f * Vary);
			Gain = 0.9f;
			break;
		case ENHShot::PistolTail:
			Tail(Out, V, 1.1f, 1800.f * Vary, 0.22f);
			Gain = 0.45f;
			break;
		case ENHShot::RifleTail:
			Tail(Out, V, 1.6f, 1300.f * Vary, 0.36f);
			Gain = 0.55f;
			break;
		case ENHShot::BulletHit:
		{
			const int32 N = Rate * 18 / 100;
			Out.SetNumZeroed(N);
			double Phase = 0.0;
			for (int32 I = 0; I < N; ++I)
			{
				const float T = static_cast<float>(I) / Rate;
				Phase += 2.0 * UE_DOUBLE_PI * (160.f * Vary) / Rate;
				Out[I] = FMath::Sin(Phase) * Decay(T, 0.02f) + V.BandPass(V.Noise(), 2400.f * Vary, 2.f) * Decay(T, 0.035f) * 0.8f; // a knock, and grit flying
			}
			Gain = 0.6f;
			break;
		}
		case ENHShot::BodyHit:
		{
			const int32 N = Rate * 16 / 100;
			Out.SetNumZeroed(N);
			double Phase = 0.0;
			for (int32 I = 0; I < N; ++I)
			{
				const float T = static_cast<float>(I) / Rate;
				Phase += 2.0 * UE_DOUBLE_PI * FMath::Lerp(55.f, 110.f * Vary, Decay(T, 0.02f)) / Rate;
				Out[I] = FMath::Sin(Phase) * Decay(T, 0.035f) * 1.2f + V.LowPass(V.Noise(), 700.f) * Decay(T, 0.02f) * 1.5f;
			}
			Gain = 0.6f;
			break;
		}
		case ENHShot::MacheteSwing:
		{
			const int32 N = Rate * 28 / 100;
			Out.SetNumZeroed(N);
			for (int32 I = 0; I < N; ++I)
			{
				const float K = static_cast<float>(I) / N;
				const float Speed = FMath::Square(FMath::Sin(UE_PI * K)); // slow, fast, slow
				Out[I] = V.BandPass(V.Noise(), (500.f + 2600.f * Speed) * Vary, 3.f) * Speed * 0.9f; // air round the blade: higher the faster it moves
			}
			Gain = 0.5f;
			break;
		}
		case ENHShot::MacheteHit:
		case ENHShot::DrawBlade:
		{
			// steel ringing: a handful of notes that do not belong to one chord. Drawn, it is a scrape first and rings less.
			const bool bHit = Shot == ENHShot::MacheteHit;
			const int32 N = Rate * (bHit ? 45 : 30) / 100;
			Out.SetNumZeroed(N);
			const float Notes[] = { 1180.f, 2730.f, 4310.f, 6150.f };
			for (int32 I = 0; I < N; ++I)
			{
				const float T = static_cast<float>(I) / Rate;
				float Ring = 0.f;
				for (int32 P = 0; P < 4; ++P)
				{
					Ring += FMath::Sin(2.f * UE_PI * Notes[P] * Vary * T) * Decay(T, 0.11f - 0.02f * P) / (P + 1);
				}
				const float Knock = V.LowPass(V.Noise(), 900.f) * Decay(T, 0.012f) * 2.f;
				const float Scrape = V.BandPass(V.Noise(), 3000.f + 2500.f * T / 0.3f, 4.f) * FMath::Square(FMath::Sin(UE_PI * FMath::Min(1.f, T / 0.22f)));
				Out[I] = bHit ? Ring * 0.7f + Knock : Scrape * 0.7f + Ring * 0.15f * FMath::Min(1.f, T / 0.15f);
			}
			Gain = bHit ? 0.6f : 0.35f;
			break;
		}
		default: // Draw: cloth, then the click of a hand closing on it
		{
			const int32 N = Rate * 22 / 100;
			Out.SetNumZeroed(N);
			FVoice Click(V.Seed ^ 0x2545f49u);
			for (int32 I = 0; I < N; ++I)
			{
				const float T = static_cast<float>(I) / Rate;
				const float Cloth = V.LowPass(V.Noise(), 1500.f) * FMath::Square(FMath::Sin(UE_PI * FMath::Min(1.f, T / 0.16f))) * 1.2f;
				const float TC = T - 0.15f;
				Out[I] = Cloth + (TC > 0.f ? Click.BandPass(Click.Noise(), 2800.f * Vary, 5.f) * Decay(TC, 0.008f) : 0.f);
			}
			Gain = 0.3f;
			break;
		}
		}
		TArray<int16> Samples;
		Samples.SetNumUninitialized(Out.Num());
		const int32 FadeOut = Rate / 200; // 5 ms, so the end does not click
		for (int32 I = 0; I < Out.Num(); ++I)
		{
			const float Edge = FMath::Min(1.f, static_cast<float>(Out.Num() - I) / FadeOut);
			Samples[I] = static_cast<int16>(FMath::Tanh(Out[I] * 1.4f) * Gain * Edge * 32767.f); // tanh: loud without clipping hard
		}
		return Samples;
	}
}

UNHShotWave::UNHShotWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	SetSampleRate(NHWeaponSounds::Rate);
	bLooping = false;
	SoundGroup = SOUNDGROUP_Default;
}

UNHShotWave* UNHShotWave::MakeAt(UObject* Outer, const TArray<int16>& Samples, int32 SampleRate)
{
	UNHShotWave* Wave = NewObject<UNHShotWave>(Outer);
	Wave->SetSampleRate(SampleRate);
	Wave->Length = static_cast<float>(Samples.Num()) / SampleRate;
	Wave->Duration = Wave->Length;
	Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
	return Wave;
}

UNHShotWave* UNHShotWave::Make(UObject* Outer, const TArray<int16>& Samples)
{
	UNHShotWave* Wave = NewObject<UNHShotWave>(Outer);
	Wave->Length = static_cast<float>(Samples.Num()) / NHWeaponSounds::Rate;
	Wave->Duration = Wave->Length;
	Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
	return Wave;
}
