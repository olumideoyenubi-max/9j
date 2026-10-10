#include "Audio/NHEngineWave.h"

#include "Core/NHGameData.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "NaijaHustleGame.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// ---------------------------------------------------------------------------------------------------- engines
UNHEngineWave::UNHEngineWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	SetSampleRate(Rate);
	Duration = INDEFINITELY_LOOPING_DURATION;
	bLooping = true;
	SoundGroup = SOUNDGROUP_Default;
}

ENHEngine UNHEngineWave::KindFor(FName VehicleType, bool bBike, bool bBoat)
{
	return bBoat ? ENHEngine::Outboard : VehicleType == TEXT("keke") ? ENHEngine::Keke : bBike ? ENHEngine::Bike
		: VehicleType == TEXT("danfo") || VehicleType == TEXT("truck") || VehicleType.ToString().Contains(TEXT("truck")) ? ENHEngine::Diesel : ENHEngine::Car;
}

UNHEngineWave* UNHEngineWave::Make(UObject* Outer, ENHEngine Kind)
{
	UNHEngineWave* Wave = NewObject<UNHEngineWave>(Outer);
	switch (Kind)
	{
	case ENHEngine::Diesel:   Wave->IdleHz = 22.f; Wave->TopHz = 82.f;  Wave->Rough = 0.34f; Wave->Bright = 0.3f; break;
	case ENHEngine::Bike:     Wave->IdleHz = 36.f; Wave->TopHz = 175.f; Wave->Rough = 0.22f; Wave->Bright = 0.85f; break;
	case ENHEngine::Keke:     Wave->IdleHz = 24.f; Wave->TopHz = 92.f;  Wave->Rough = 0.4f;  Wave->Bright = 0.7f; break;
	case ENHEngine::Outboard: Wave->IdleHz = 30.f; Wave->TopHz = 130.f; Wave->Rough = 0.3f;  Wave->Bright = 0.6f; break;
	default:                  Wave->IdleHz = 28.f; Wave->TopHz = 118.f; Wave->Rough = 0.14f; Wave->Bright = 0.5f; break;
	}
	return Wave;
}

int32 UNHEngineWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	OutAudio.SetNumUninitialized(NumSamples * sizeof(int16));
	int16* Out = reinterpret_cast<int16*>(OutAudio.GetData());
	const float ToRevs = WantRevs.load(), ToLoad = WantLoad.load();
	for (int32 I = 0; I < NumSamples; ++I)
	{
		// the revs and the load ease to what the game last said, so a gear change is a slide and not a click
		Revs += (ToRevs - Revs) * 0.0004f;
		Load += (ToLoad - Load) * 0.0008f;
		const double Hz = FMath::Lerp(IdleHz, TopHz, Revs);
		Phase += Hz / Rate;
		if (Phase > 4096.0)
		{
			Phase -= 4096.0;
		}
		const double T = 2.0 * UE_DOUBLE_PI * Phase;
		// the firing note and what rides on it; the higher ones come up with the revs and the load
		const float Up = 0.35f + 0.65f * FMath::Max(Revs, Load);
		// (a laptop's speakers give nothing back below 100 Hz or so: the engine is heard by what rides on its note, so those are strong)
		float S = static_cast<float>(FMath::Sin(T)) * 0.5f
			+ static_cast<float>(FMath::Sin(2.0 * T + 0.4)) * 0.45f
			+ static_cast<float>(FMath::Sin(3.0 * T + 1.1)) * 0.42f * Up
			+ static_cast<float>(FMath::Sin(4.0 * T + 0.2)) * 0.4f * Up
			+ static_cast<float>(FMath::Sin(6.0 * T + 2.0)) * 0.32f * Up * (0.5f + Bright)
			+ static_cast<float>(FMath::Sin(8.0 * T + 0.7)) * 0.26f * Up * (0.4f + Bright)
			+ static_cast<float>(FMath::Sin(12.0 * T + 1.6)) * 0.16f * Up * Bright;
		// every other firing a little weaker: the lope of an idle, gone by the time it is pulling
		S *= 1.f - (1.f - Revs) * 0.22f * (0.5f + 0.5f * static_cast<float>(FMath::Sin(0.5 * T)));
		// the rattle: hiss, low-passed, pulsed with the firing
		Seed = Seed * 1664525u + 1013904223u;
		const float Hiss = (static_cast<float>(Seed >> 8) / 8388608.f) - 1.f;
		Low += (Hiss - Low) * (0.08f + 0.25f * Bright);
		S += Low * Rough * (0.4f + 0.6f * Load) * (0.6f + 0.4f * static_cast<float>(FMath::Sin(T)));
		const float Gain = 0.3f + 0.25f * Load + 0.12f * Revs;
		Out[I] = static_cast<int16>(FMath::Clamp(S * Gain, -1.f, 1.f) * 32000.f);
	}
	return NumSamples;
}

// ---------------------------------------------------------------------------------------------------- voices
namespace
{
	struct FSound
	{
		/** A vowel's two resonances, Hz; 0 for a consonant */
		float F1 = 0.f, F2 = 0.f;
		/** -1 low, 0 mid, 1 high */
		int32 Tone = 0;
		/** 'v' vowel, 'm' murmur (b d g l m n r w y j), 's' hiss, 'k' stop, ' ' gap, '.' long gap */
		TCHAR Kind = TEXT(' ');
		bool bSharp = false, bRise = false, bLoud = false;
	};

	bool Vowel(TCHAR C, float& F1, float& F2, int32& Tone, bool& bOpen)
	{
		Tone = 0;
		bOpen = false;
		switch (C)
		{
		case 0x00E1: Tone = 1; C = TEXT('a'); break;  case 0x00E0: Tone = -1; C = TEXT('a'); break;
		case 0x00E9: Tone = 1; C = TEXT('e'); break;  case 0x00E8: Tone = -1; C = TEXT('e'); break;
		case 0x00ED: Tone = 1; C = TEXT('i'); break;  case 0x00EC: Tone = -1; C = TEXT('i'); break;
		case 0x00F3: Tone = 1; C = TEXT('o'); break;  case 0x00F2: Tone = -1; C = TEXT('o'); break;
		case 0x00FA: Tone = 1; C = TEXT('u'); break;  case 0x00F9: Tone = -1; C = TEXT('u'); break;
		case 0x1EB9: bOpen = true; C = TEXT('e'); break; // e with a dot below
		case 0x1ECD: bOpen = true; C = TEXT('o'); break; // o with a dot below
		default: break;
		}
		switch (C)
		{
		case TEXT('a'): F1 = 760.f; F2 = 1320.f; return true;
		case TEXT('e'): F1 = 440.f; F2 = 2050.f; return true;
		case TEXT('i'): F1 = 300.f; F2 = 2300.f; return true;
		case TEXT('o'): F1 = 450.f; F2 = 860.f; return true;
		case TEXT('u'): F1 = 320.f; F2 = 800.f; return true;
		default: return false;
		}
	}

	TCHAR Lower(TCHAR C)
	{
		switch (C)
		{
		case 0x00C1: return 0x00E1; case 0x00C0: return 0x00E0; case 0x00C9: return 0x00E9; case 0x00C8: return 0x00E8;
		case 0x00CD: return 0x00ED; case 0x00CC: return 0x00EC; case 0x00D3: return 0x00F3; case 0x00D2: return 0x00F2;
		case 0x00DA: return 0x00FA; case 0x00D9: return 0x00F9; case 0x1EB8: return 0x1EB9; case 0x1ECC: return 0x1ECD;
		case 0x1E62: return 0x1E63; case 0x0143: return 0x0144; case 0x01F8: return 0x01F9;
		default: return FChar::ToLower(C);
		}
	}

	/** The line as sounds: vowels with their tones, consonants, gaps */
	TArray<FSound> Sounds(const FString& Line)
	{
		TArray<FSound> Out;
		for (int32 I = 0; I < Line.Len(); ++I)
		{
			const TCHAR C = Lower(Line[I]);
			FSound S;
			int32 Tone = 0;
			bool bOpen = false;
			if (C == 0x0301 || C == 0x0300 || C == 0x0323 || C == 0x0304)
			{
				// a mark on the letter before: acute high, grave low, a dot below opens e and o and turns s to sh
				if (Out.Num() > 0)
				{
					FSound& Last = Out.Last();
					if (C == 0x0301) { Last.Tone = 1; }
					else if (C == 0x0300) { Last.Tone = -1; }
					else if (C == 0x0323 && Last.Kind == TEXT('v')) { Last.F1 += 150.f; Last.F2 += Last.F2 < 1200.f ? 90.f : -200.f; }
					else if (C == 0x0323 && Last.Kind == TEXT('s')) { Last.bSharp = false; }
				}
				continue;
			}
			if (Vowel(C, S.F1, S.F2, Tone, bOpen))
			{
				S.Kind = TEXT('v');
				S.Tone = Tone;
				if (bOpen)
				{
					S.F1 += 150.f;
					S.F2 += S.F2 < 1200.f ? 90.f : -200.f;
				}
			}
			else if (C == 0x0144 || C == 0x01F9) // a syllabic n with its own tone
			{
				S.Kind = TEXT('v');
				S.F1 = 260.f;
				S.F2 = 1250.f;
				S.Tone = C == 0x0144 ? 1 : -1;
			}
			else if (FCString::Strchr(TEXT("bdgjlmnrwyvz"), C))
			{
				S.Kind = TEXT('m');
			}
			else if (C == TEXT('s') || C == TEXT('f') || C == TEXT('h') || C == 0x1E63 || C == TEXT('c') || C == TEXT('x'))
			{
				S.Kind = TEXT('s');
				S.bSharp = C == TEXT('s');
			}
			else if (FCString::Strchr(TEXT("kptq"), C))
			{
				S.Kind = TEXT('k');
			}
			else if (C == TEXT('.') || C == TEXT('!') || C == TEXT('?') || C == TEXT(',') || C == TEXT(';') || C == TEXT(':'))
			{
				S.Kind = TEXT('.');
				// what came before it is said as a question rises, or as a shout is loud
				for (int32 K = Out.Num() - 1, Back = 0; K >= 0 && Back < 3; --K)
				{
					if (Out[K].Kind == TEXT('v'))
					{
						Out[K].bRise |= C == TEXT('?');
						Out[K].bLoud |= C == TEXT('!');
						++Back;
					}
				}
			}
			else if (C == TEXT(' ') || C == TEXT('-') || C == TEXT('\''))
			{
				S.Kind = TEXT(' ');
			}
			else
			{
				continue; // digits, brackets, signs: not said
			}
			Out.Add(S);
		}
		return Out;
	}

	/** One resonance: a two-pole filter ringing at a pitch */
	struct FRing
	{
		float A1 = 0.f, A2 = 0.f, G = 0.f, Y1 = 0.f, Y2 = 0.f;
		void Tune(float Hz, float Width)
		{
			const float R = FMath::Exp(-UE_PI * Width / NHVoice::Rate);
			A1 = 2.f * R * FMath::Cos(2.f * UE_PI * Hz / NHVoice::Rate);
			A2 = -R * R;
			G = 1.f - R;
		}
		float Step(float X)
		{
			const float Y = G * X + A1 * Y1 + A2 * Y2;
			Y2 = Y1;
			Y1 = Y;
			return Y;
		}
	};
}

TArray<int16> NHVoice::Make(const FString& Line, const FNHVoice& Voice)
{
	const TArray<FSound> List = Sounds(Line);
	TArray<int16> Out;
	Out.Reserve(List.Num() * Rate / 8);
	FRing R1, R2, R3;
	R3.Tune(2700.f * Voice.Formant, 260.f);
	double Phase = 0.0;
	float Hz = Voice.Hz;
	uint32 Seed = 777u + static_cast<uint32>(Line.Len()) * 131u;
	int32 Vowels = 0, Said = 0;
	for (const FSound& S : List)
	{
		Vowels += S.Kind == TEXT('v') ? 1 : 0;
	}
	// a line with no tone marks (English, Pidgin) is given a little tune of its own, or it is one flat note
	bool bMarked = false;
	for (const FSound& S : List)
	{
		bMarked |= S.Tone != 0;
	}
	for (int32 Index = 0; Index < List.Num(); ++Index)
	{
		const FSound& S = List[Index];
		const float Seconds = (S.Kind == TEXT('v') ? 0.115f : S.Kind == TEXT('m') ? 0.05f : S.Kind == TEXT('s') ? 0.07f : S.Kind == TEXT('k') ? 0.045f : S.Kind == TEXT('.') ? 0.22f : 0.06f) / Voice.Speed;
		const int32 N = FMath::Max(1, FMath::RoundToInt(Seconds * Rate));
		float Want = Voice.Hz;
		if (S.Kind == TEXT('v'))
		{
			// high, mid and low about three semitones apart; the whole line drifts down as breath runs out
			const float Fall = 1.f - 0.1f * (Vowels > 1 ? static_cast<float>(Said) / (Vowels - 1) : 0.f);
			const int32 Tune = bMarked ? S.Tone : static_cast<int32>((Line.Len() * 7 + Said * 5) % 3) - 1;
			Want = Voice.Hz * Fall * (Tune > 0 ? 1.19f : Tune < 0 ? 0.84f : 1.f) * (S.bRise ? 1.18f : 1.f) * (S.bLoud ? 1.08f : 1.f);
			R1.Tune(S.F1 * Voice.Formant, 90.f);
			R2.Tune(S.F2 * Voice.Formant, 120.f);
			++Said;
		}
		for (int32 I = 0; I < N; ++I)
		{
			const float K = static_cast<float>(I) / N;
			// in and out of each sound without a click
			const float Shape = FMath::Min(1.f, FMath::Min(K * 8.f, (1.f - K) * 6.f));
			Seed = Seed * 1664525u + 1013904223u;
			const float Hiss = (static_cast<float>(Seed >> 8) / 8388608.f) - 1.f;
			float X = 0.f;
			if (S.Kind == TEXT('v') || S.Kind == TEXT('m'))
			{
				Hz += (Want - Hz) * 0.0025f; // the pitch slides from one syllable's to the next
				Phase += Hz / Rate;
				Phase -= FMath::FloorToDouble(Phase);
				// the buzz of the voice: a falling ramp each cycle, with a breath of hiss in it
				const float Buzz = static_cast<float>(1.0 - 2.0 * Phase) + Hiss * 0.05f;
				if (S.Kind == TEXT('v'))
				{
					X = (R1.Step(Buzz) * 1.0f + R2.Step(Buzz) * 0.7f + R3.Step(Buzz) * 0.25f) * 9.f * (S.bLoud ? 1.25f : 1.f);
				}
				else
				{
					X = R1.Step(Buzz) * 3.2f; // lips or tongue closed: only the low hum gets out
					R2.Step(Buzz * 0.2f);
				}
			}
			else if (S.Kind == TEXT('s'))
			{
				X = (S.bSharp ? Hiss - R1.Step(Hiss) : R2.Step(Hiss) * 3.f) * 0.28f;
			}
			else if (S.Kind == TEXT('k'))
			{
				X = K > 0.7f ? Hiss * 0.35f * (1.f - K) * 3.f : 0.f; // closed, then a small burst
			}
			Out.Add(static_cast<int16>(FMath::Clamp(X * Shape * 0.5f, -1.f, 1.f) * 30000.f));
		}
	}
	return Out;
}

FNHVoice NHVoice::For(const FString& Who, bool bWoman)
{
	// the cast's own voices, by the name a line gives (first names too)
	static const struct { const TCHAR* Name; float Hz, Speed, Formant; } Known[] = {
		{ TEXT("Tunde"), 128.f, 1.05f, 1.f }, { TEXT("Amaka"), 215.f, 1.1f, 1.17f }, { TEXT("Iya Tobi"), 190.f, 0.9f, 1.12f }, { TEXT("Baba"), 96.f, 0.84f, 0.94f },
		{ TEXT("Chidi"), 150.f, 1.15f, 1.04f }, { TEXT("Zainab"), 205.f, 1.f, 1.15f }, { TEXT("Kemi"), 225.f, 1.05f, 1.18f }, { TEXT("Shina"), 100.f, 0.95f, 0.92f },
		{ TEXT("Sir Jaguar"), 108.f, 0.9f, 0.95f }, { TEXT("Big Bar"), 92.f, 0.82f, 0.9f }, { TEXT("Treasurer"), 118.f, 1.1f, 0.98f }, { TEXT("Neighbour"), 200.f, 1.15f, 1.14f },
		{ TEXT("Trader"), 210.f, 1.2f, 1.15f } };
	for (const auto& K : Known)
	{
		if (Who.Contains(K.Name))
		{
			return { K.Hz, K.Speed, K.Formant };
		}
	}
	// anybody else: a voice from their name, so the same person always sounds the same
	const uint32 H = GetTypeHash(Who);
	const float A = static_cast<float>(H % 1000) / 1000.f, B = static_cast<float>((H / 1000) % 1000) / 1000.f;
	return bWoman ? FNHVoice{ 185.f + 50.f * A, 0.95f + 0.25f * B, 1.12f + 0.08f * A } : FNHVoice{ 98.f + 45.f * A, 0.9f + 0.25f * B, 0.93f + 0.1f * A };
}

TArray<int16> NHVoice::Horn(bool bBig)
{
	TArray<int16> Out;
	const float A = bBig ? 310.f : 420.f, B = bBig ? 370.f : 500.f;
	const int32 N = Rate * 45 / 100;
	Out.Reserve(N);
	for (int32 I = 0; I < N; ++I)
	{
		const float T = static_cast<float>(I) / Rate, K = static_cast<float>(I) / N;
		// two reedy notes a third apart
		const auto Reed = [T](float Hz) { const float P = FMath::Frac(T * Hz); return (P < 0.42f ? 1.f : -1.f) * 0.6f + FMath::Sin(2.f * UE_PI * Hz * T) * 0.4f; };
		const float Shape = FMath::Min(1.f, FMath::Min(K * 40.f, (1.f - K) * 12.f));
		Out.Add(static_cast<int16>((Reed(A) + Reed(B)) * 0.22f * Shape * 32000.f));
	}
	return Out;
}

// ---------------------------------------------------------------------------------------------------- what people say
namespace
{
	TMap<FName, TArray<FString>>& BarkLines()
	{
		static TMap<FName, TArray<FString>> Lines;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			FString Text;
			TSharedPtr<FJsonObject> Root;
			if (FFileHelper::LoadFileToString(Text, *(UNHGameData::DataDir() / TEXT("barks.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root)
			{
				const TSharedPtr<FJsonObject>* Kinds = nullptr;
				if (Root->TryGetObjectField(TEXT("lines"), Kinds))
				{
					for (const TPair<FString, TSharedPtr<FJsonValue>>& Kind : (*Kinds)->Values)
					{
						const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
						if (!Kind.Value->TryGetArray(List))
						{
							continue;
						}
						TArray<FString>& Into = Lines.FindOrAdd(FName(*Kind.Key));
						for (const TSharedPtr<FJsonValue>& V : *List)
						{
							const TSharedPtr<FJsonObject>* J = nullptr;
							FString Plain;
							if (V->TryGetObject(J))
							{
								// Yoruba with its English after it, or a Pidgin line on its own
								FString Yo, En, Pcm;
								(*J)->TryGetStringField(TEXT("yo"), Yo);
								(*J)->TryGetStringField(TEXT("en"), En);
								(*J)->TryGetStringField(TEXT("pcm"), Pcm);
								Into.Add(!Yo.IsEmpty() ? (En.IsEmpty() ? Yo : FString::Printf(TEXT("%s  (%s)"), *Yo, *En)) : Pcm);
							}
							else if (V->TryGetString(Plain))
							{
								Into.Add(Plain);
							}
						}
					}
				}
			}
			int32 Total = 0;
			for (const TPair<FName, TArray<FString>>& Kind : Lines)
			{
				Total += Kind.Value.Num();
			}
			UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: barks: %d lines of %d kinds from Data/barks.json"), Total, Lines.Num());
		}
		return Lines;
	}
}

FString NHBarks::Pick(FName Kind)
{
	const TArray<FString>* Lines = BarkLines().Find(Kind);
	return Lines && Lines->Num() > 0 ? (*Lines)[FMath::RandRange(0, Lines->Num() - 1)] : FString();
}

int32 NHBarks::Count(FName Kind)
{
	const TArray<FString>* Lines = BarkLines().Find(Kind);
	return Lines ? Lines->Num() : 0;
}

FString NHBarks::Spoken(const FString& Shown)
{
	// "Ẹ ṣé o!  (Thank you)" is said as "Ẹ ṣé o!"; a name before a colon is who says it, not part of it
	FString Out = Shown;
	int32 Bracket = INDEX_NONE;
	if (Out.FindChar(TEXT('('), Bracket) && Bracket > 2)
	{
		Out.LeftInline(Bracket);
	}
	return Out.TrimStartAndEnd().Replace(TEXT("\""), TEXT(""));
}
