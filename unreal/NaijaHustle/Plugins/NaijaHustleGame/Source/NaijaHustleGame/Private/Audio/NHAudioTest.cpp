#include "Audio/NHAudioTest.h"

#include "Audio/NHAudioSubsystem.h"
#include "Audio/NHToneWave.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMemory.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NaijaHustleGame.h"

ANHAudioTest::ANHAudioTest()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ANHAudioTest::View(FVector& At, FVector& Forward, FVector& Right, FVector& Up) const
{
	FRotator Rot = FRotator::ZeroRotator;
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->GetPlayerViewPoint(At, Rot);
	}
	// level with the ground whichever way the camera tilts: a sound put "ahead" of a camera looking down would be under the road
	const FRotationMatrix Axes(FRotator(0.f, Rot.Yaw, 0.f));
	Forward = Axes.GetUnitAxis(EAxis::X);
	Right = Axes.GetUnitAxis(EAxis::Y);
	Up = Axes.GetUnitAxis(EAxis::Z);
}

void ANHAudioTest::Stop(FName Id)
{
	if (UAudioComponent* Comp = Playing.FindRef(Id))
	{
		Comp->Stop();
	}
	Playing.Remove(Id);
}

void ANHAudioTest::Begin()
{
	UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this);
	if (!Audio || !Audio->IsBuilt())
	{
		UE_LOG(LogNHGame, Error, TEXT("[audiotest] RESULT: FAIL, the audio mix was not built (no sound device?)"));
		bDone = true;
		QuitIn = bQuitWhenDone ? 1.f : -1.f;
		return;
	}
	// full sliders, stereo, and none of the game's own ducking, so every level in the recording is the test's doing
	for (int32 I = 0; I < static_cast<int32>(ENHVolume::Count); ++I)
	{
		SavedVolumes[I] = Audio->GetVolume(static_cast<ENHVolume>(I));
		Audio->SetVolume(static_cast<ENHVolume>(I), 100);
	}
	bSavedMono = Audio->MonoOn();
	Audio->SetMono(false);
	for (int32 I = 0; I < static_cast<int32>(ENHMix::Count); ++I)
	{
		Audio->ForceMix(static_cast<ENHMix>(I), 0);
	}
	StartSpace = UNHAudioSubsystem::SpaceName(Audio->GetSpace());
	ZoneNote = Audio->Describe();
	Audio->ForceSpace(ENHAudioSpace::Street);

	const auto Tone = [this, Audio](FName Id, ENHTone Shape, float Hz, ENHSoundKind Kind, float Gain = 0.25f)
	{
		Playing.Add(Id, Audio->Play(UNHToneWave::Make(this, Shape, Hz, Gain), Kind));
	};
	const auto ToneAt = [this, Audio](FName Id, ENHTone Shape, float Hz, ENHSoundKind Kind, const FVector& Where, float Gain = 0.25f)
	{
		Playing.Add(Id, Audio->PlayAt(UNHToneWave::Make(this, Shape, Hz, Gain), Kind, Where));
	};

	Phases.Add({ TEXT("silence"), 1.f, [] {} });
	Phases.Add({ TEXT("music"), 2.5f, [Tone] { Tone(TEXT("music"), ENHTone::Sine, 220.f, ENHSoundKind::Music); } });
	// a line in the Dialogue class: nothing is pushed by hand, the class brings its own mix
	Phases.Add({ TEXT("music_dialogue"), 2.5f, [Tone] { Tone(TEXT("line"), ENHTone::Sine, 660.f, ENHSoundKind::Dialogue); } });
	Phases.Add({ TEXT("music_after_dialogue"), 2.5f, [this] { Stop(TEXT("line")); } });
	Phases.Add({ TEXT("music_call"), 2.5f, [Tone, Audio] { Audio->ForceMix(ENHMix::PhoneCall, 1); Tone(TEXT("call"), ENHTone::Sine, 1320.f, ENHSoundKind::Phone); } });
	Phases.Add({ TEXT("music_after_call"), 2.5f, [this, Audio] { Audio->ForceMix(ENHMix::PhoneCall, 0); Stop(TEXT("call")); } });
	Phases.Add({ TEXT("music_slider_50"), 2.f, [Audio] { Audio->SetVolume(ENHVolume::Music, 50); } });
	Phases.Add({ TEXT("music_master_50"), 2.f, [Audio] { Audio->SetVolume(ENHVolume::Master, 50); } });
	Phases.Add({ TEXT("radio"), 2.5f, [this, Tone, Audio]
	{
		Stop(TEXT("music"));
		Audio->SetVolume(ENHVolume::Music, 100);
		Audio->SetVolume(ENHVolume::Master, 100);
		Tone(TEXT("radio"), ENHTone::Noise, 0.f, ENHSoundKind::RadioCabin, 0.2f);
	} });
	Phases.Add({ TEXT("radio_muffled"), 2.5f, [Audio] { Audio->ForceMix(ENHMix::RadioMuffled, 1); } });
	Phases.Add({ TEXT("side"), 2.5f, [this, ToneAt, Audio]
	{
		Audio->ForceMix(ENHMix::RadioMuffled, 0);
		Stop(TEXT("radio"));
		FVector At, F, R, U;
		View(At, F, R, U);
		ToneAt(TEXT("side"), ENHTone::Sine, 500.f, ENHSoundKind::AmbienceSpot, At + R * 400.f);
	} });
	Phases.Add({ TEXT("side_mono"), 2.5f, [Audio] { Audio->SetMono(true); } });
	Phases.Add({ TEXT("front"), 2.5f, [this, ToneAt, Audio]
	{
		Audio->SetMono(false);
		Stop(TEXT("side"));
		FVector At, F, R, U;
		View(At, F, R, U);
		ToneAt(TEXT("front"), ENHTone::Sine, 3000.f, ENHSoundKind::Horn, At + F * 800.f + R * 300.f + U * 100.f);
	} });
	Phases.Add({ TEXT("front_wall"), 3.f, [this]
	{
		// a slab half way between the listener and the sound, face on to the line between them
		FVector At, F, R, U;
		View(At, F, R, U);
		const FVector Source = At + F * 800.f + R * 300.f + U * 100.f;
		AStaticMeshActor* Slab = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform((Source - At).Rotation(), (At + Source) * 0.5f));
		if (Slab)
		{
			UStaticMeshComponent* Mesh = Slab->GetStaticMeshComponent();
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			// it stops the rays that look for walls and nothing else: a slab that pushed the player aside would move the listener
			Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
			Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			Slab->SetActorScale3D(FVector(0.5f, 12.f, 12.f));
		}
		Wall = Slab;
	} });
	Phases.Add({ TEXT("burst_street"), 3.f, [this, ToneAt]
	{
		Stop(TEXT("front"));
		if (Wall)
		{
			Wall->Destroy();
			Wall = nullptr;
		}
		FVector At, F, R, U;
		View(At, F, R, U);
		ToneAt(TEXT("burst"), ENHTone::Burst, 0.f, ENHSoundKind::WeaponTail, At + F * 300.f + R * 200.f + U * 100.f, 0.5f); // a kind no wall muffles
	} });
	Phases.Add({ TEXT("to_tunnel"), 3.f, [this, Audio] { Stop(TEXT("burst")); Audio->ForceSpace(ENHAudioSpace::Tunnel); } });
	Phases.Add({ TEXT("burst_tunnel"), 4.5f, [this, ToneAt]
	{
		FVector At, F, R, U;
		View(At, F, R, U);
		ToneAt(TEXT("burst"), ENHTone::Burst, 0.f, ENHSoundKind::WeaponTail, At + F * 300.f + R * 200.f + U * 100.f, 0.5f); // a kind no wall muffles
	} });
	// forty horns at once, in a ring 10 m out: only as many as the Horn kind allows may sound
	Phases.Add({ TEXT("flood"), 2.f, [this, Audio]
	{
		Stop(TEXT("burst"));
		Audio->ForceSpace(ENHAudioSpace::Street);
		FVector At, F, R, U;
		View(At, F, R, U);
		FloodAsked = 40;
		for (int32 I = 0; I < FloodAsked; ++I)
		{
			const float Angle = I * 2.f * UE_PI / FloodAsked;
			Flood.Add(Audio->PlayAt(UNHToneWave::Make(this, ENHTone::Sine, 300.f + 20.f * I, 0.03f), ENHSoundKind::Horn, At + (F * FMath::Cos(Angle) + R * FMath::Sin(Angle)) * (800.f + 10.f * I)));
		}
	} });

	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("NHAudio"));
	IFileManager::Get().MakeDirectory(*Folder, true);
	IFileManager::Get().Delete(*(Folder / TEXT("nh_audio_test.wav")));
	GEngine->Exec(GetWorld(), TEXT("CsvCategory Audio Enable"));
	GEngine->Exec(GetWorld(), TEXT("CsvProfile Start"));
	UAudioMixerBlueprintLibrary::StartRecordingOutput(this, 60.f);
	RecordedFrom = FPlatformTime::Seconds();
	UE_LOG(LogNHGame, Log, TEXT("[audiotest] started in the %s space; %d situations, recording to %s"), *StartSpace, Phases.Num(), *Folder);
}

void ANHAudioTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (QuitIn > 0.f)
	{
		QuitIn -= DeltaSeconds;
		if (QuitIn <= 0.f)
		{
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				PC->ConsoleCommand(TEXT("quit"));
			}
		}
		return;
	}
	if (bDone)
	{
		return;
	}
	if (Index < 0)
	{
		Wait -= DeltaSeconds; // let the level settle, and the audio subsystem find the listener's space
		if (Wait > 0.f)
		{
			return;
		}
		// -NHAudioAt=oshoja: run it at that stop. Only now, with the streets round the player loaded: going there the
		// moment the level opens puts the player on ground that is not there yet.
		FString Where;
		if (!bWent && FParse::Value(FCommandLine::Get(), TEXT("NHAudioAt="), Where))
		{
			bWent = true;
			Wait = 4.f;
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				PC->ConsoleCommand(FString::Printf(TEXT("NHGoto %s"), *Where));
			}
			return;
		}
		Begin();
		if (bDone)
		{
			return;
		}
	}
	++Frames;
	FrameSeconds += FApp::GetDeltaTime();
	const UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this);
	VoicesPeak = FMath::Max(VoicesPeak, Audio->ActiveVoices());
	Left -= DeltaSeconds;
	const double Now = FPlatformTime::Seconds() - RecordedFrom;
	if (Phases.IsValidIndex(Index) && Phases[Index].Name == TEXT("flood") && Left < 0.8f && FloodPlaying < 0)
	{
		// a component whose sound was turned away still says it is playing, so count what the device is mixing:
		// nothing else sounds during the flood
		FloodVoices = Audio->ActiveVoices();
		FloodPlaying = FloodVoices;
	}
	if (Left <= 0.f)
	{
		if (Marks.IsValidIndex(Index))
		{
			Marks[Index].Value.Y = Now;
		}
		if (++Index >= Phases.Num())
		{
			Finish();
			return;
		}
		Left = Phases[Index].Seconds;
		Marks.Add({ Phases[Index].Name, FVector2D(Now, Now) });
		Phases[Index].Start();
		FVector At, F, R, U;
		View(At, F, R, U);
		UE_LOG(LogNHGame, Log, TEXT("[audiotest] %5.1f s  %-22s listener at %.0f, %.0f, %.0f"), Now, *Phases[Index].Name, At.X, At.Y, At.Z);
	}
}

void ANHAudioTest::Finish()
{
	bDone = true;
	UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this);
	for (UAudioComponent* Comp : Flood)
	{
		if (Comp)
		{
			Comp->Stop();
		}
	}
	Flood.Reset();
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("NHAudio"));
	UAudioMixerBlueprintLibrary::StopRecordingOutput(this, EAudioRecordingExportType::WavFile, TEXT("nh_audio_test"), Folder);
	GEngine->Exec(GetWorld(), TEXT("CsvProfile Stop"));

	for (int32 I = 0; I < static_cast<int32>(ENHVolume::Count); ++I)
	{
		Audio->SetVolume(static_cast<ENHVolume>(I), SavedVolumes[I]);
	}
	Audio->SetMono(bSavedMono);
	for (int32 I = 0; I < static_cast<int32>(ENHMix::Count); ++I)
	{
		Audio->ForceMix(static_cast<ENHMix>(I), -1);
	}
	Audio->ForceSpace(ENHAudioSpace::Count);

	const int32 Limit = UNHAudioSubsystem::MaxVoices(ENHSoundKind::Horn);
	const FPlatformMemoryStats Memory = FPlatformMemory::GetStats();
	const double FrameMs = Frames > 0 ? 1000.0 * FrameSeconds / Frames : 0.0;
	FString Json = TEXT("{\n  \"phases\": [\n");
	for (int32 I = 0; I < Marks.Num(); ++I)
	{
		Json += FString::Printf(TEXT("    { \"name\": \"%s\", \"start\": %.3f, \"end\": %.3f }%s\n"), *Marks[I].Key, Marks[I].Value.X, Marks[I].Value.Y, I + 1 < Marks.Num() ? TEXT(",") : TEXT(""));
	}
	Json += FString::Printf(TEXT("  ],\n  \"startSpace\": \"%s\",\n  \"flood\": { \"asked\": %d, \"playing\": %d, \"limit\": %d, \"voices\": %d },\n  \"voicesPeak\": %d,\n  \"voiceBudget\": %d,\n  \"frameMs\": %.2f,\n  \"usedPhysicalMB\": %.0f\n}\n"),
		*StartSpace, FloodAsked, FloodPlaying, Limit, FloodVoices, VoicesPeak, UNHAudioSubsystem::VoiceBudget, FrameMs, Memory.UsedPhysical / 1048576.0);
	FFileHelper::SaveStringToFile(Json, *(Folder / TEXT("nh_audio_test.json")));

	UE_LOG(LogNHGame, Log, TEXT("[audiotest] before the test: %s"), *ZoneNote.Replace(TEXT("\n"), TEXT(" | ")));
	UE_LOG(LogNHGame, Log, TEXT("[audiotest] flood: asked for %d horns, %d sounding (the kind allows %d); most voices at once %d of %d"),
		FloodAsked, FloodPlaying, Limit, VoicesPeak, UNHAudioSubsystem::VoiceBudget);
	UE_LOG(LogNHGame, Log, TEXT("[audiotest] frame %.1f ms on average; process memory %.0f MB"), FrameMs, Memory.UsedPhysical / 1048576.0);
	UE_LOG(LogNHGame, Log, TEXT("[audiotest] RESULT: recorded %s; measure it with Scripts/audio_check.py"), *(Folder / TEXT("nh_audio_test.wav")));
	QuitIn = bQuitWhenDone ? 4.f : -1.f; // the WAV is written in the background
}
