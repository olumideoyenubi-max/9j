#include "Player/NHPlayerController.h"

#include "Audio/NHAudioSubsystem.h"
#include "Audio/NHAudioTest.h"
#include "AudioMixerBlueprintLibrary.h"
#include "EngineUtils.h"
#include "Input/NHInputSet.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Core/NHHustleSubsystem.h"
#include "Debug/NHDebugPlay.h"
#include "Components/CapsuleComponent.h"
#include "Gameplay/NHGameDirector.h"
#include "Lighting/NHLightingRig.h"
#include "Player/NHCharacter.h"
#include "Characters/NHOutfitComponent.h"
#include "Core/NHGameData.h"
#include "Gameplay/NHPerson.h"
#include "Gameplay/NHResponse.h"
#include "Debug/NHBridgeTest.h"
#include "Phone/NHPhone.h"
#include "UI/NHHUD.h"
#include "Vehicles/NHCarTheft.h"
#include "Vehicles/NHTraffic.h"
#include "Vehicles/NHVehicle.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "NaijaHustleGame.h"

UNHInputSet* ANHPlayerController::GetInputSet()
{
	if (!InputSet)
	{
		InputSet = NewObject<UNHInputSet>(this, TEXT("NHInputSet"));
		InputSet->Build();
	}
	return InputSet;
}

void ANHPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetNHInputMode(ENHInputMode::OnFoot);

#if !UE_BUILD_SHIPPING
	// a scripted run: one carried over a level restart, or -NHRun=autoplay|selftest on the command line (quits when done)
	FString Run = UNHDebugPlay::PendingRun;
	bool bQuitAfter = UNHDebugPlay::bPendingQuit;
	UNHDebugPlay::PendingRun.Reset();
	if (Run.IsEmpty() && FParse::Value(FCommandLine::Get(), TEXT("NHRun="), Run))
	{
		bQuitAfter = true;
	}
	if (!Run.IsEmpty())
	{
		FTimerHandle Start; // once the pawn is in and the level has settled
		GetWorldTimerManager().SetTimer(Start, FTimerDelegate::CreateWeakLambda(this, [this, Run, bQuitAfter]()
		{
			if (UNHDebugPlay* Play = DebugPlay(); Play && !Play->IsRunning())
			{
				Run == TEXT("selftest") ? Play->SelfTest(bQuitAfter) : Play->Autoplay(bQuitAfter);
			}
		}), 4.f, false);
	}
#endif

	if (FParse::Param(FCommandLine::Get(), TEXT("NHResponseTest")))
	{
		FTimerHandle Start;
		GetWorldTimerManager().SetTimer(Start, FTimerDelegate::CreateWeakLambda(this, [this] { ResponseTestStep(0); }), 8.f, false);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("NHDamageTest")))
	{
		FTimerHandle Start;
		GetWorldTimerManager().SetTimer(Start, FTimerDelegate::CreateWeakLambda(this, [this] { DamageTestStep(0); }), 8.f, false);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("NHWeaponTest")))
	{
		FTimerHandle Start;
		GetWorldTimerManager().SetTimer(Start, FTimerDelegate::CreateWeakLambda(this, [this] { WeaponTestStep(0); }), 8.f, false);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("NHRadioTest")))
	{
		FTimerHandle Start;
		GetWorldTimerManager().SetTimer(Start, FTimerDelegate::CreateWeakLambda(this, [this] { RadioTestStep(0); }), 8.f, false);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("NHBridgeTest")))
	{
		GetWorld()->SpawnActor<ANHBridgeTest>(ANHBridgeTest::StaticClass(), FTransform::Identity); // drives each vehicle over a bridge and back, logs, quits
	}
	FString Spot;
	if (FParse::Value(FCommandLine::Get(), TEXT("NHLookShots="), Spot, false)) // false: keep the commas
	{
		TArray<FString> Parts;
		Spot.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() >= 3)
		{
			FString Folder;
			FParse::Value(FCommandLine::Get(), TEXT("NHLookDir="), Folder);
			FParse::Value(FCommandLine::Get(), TEXT("NHLookHour="), LookShotHour);
			bLookShotQuit = true;
			const FVector Where(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
			FTimerHandle Start; // once the pawn is in and the level has settled
			GetWorldTimerManager().SetTimer(Start, FTimerDelegate::CreateWeakLambda(this, [this, Where, Folder]() { NHLookShots(Where.X, Where.Y, Where.Z, Folder); }), 4.f, false);
		}
	}
}

void ANHPlayerController::SetNHInputMode(ENHInputMode NewMode)
{
	InputMode = NewMode;

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return; // not a local player (e.g. a server-side controller)
	}

	UNHInputSet* Set = GetInputSet();
	Subsystem->ClearAllMappings();
	Subsystem->AddMappingContext(Set->Global, 0);
	switch (InputMode)
	{
	case ENHInputMode::OnFoot:  Subsystem->AddMappingContext(Set->OnFoot, 1); break;
	case ENHInputMode::Vehicle: Subsystem->AddMappingContext(Set->Vehicle, 1); break;
	case ENHInputMode::Menu:    Subsystem->AddMappingContext(Set->Menu, 2); break; // higher priority: Esc means "back", not "pause"
	}
	UE_LOG(LogNHGame, Verbose, TEXT("Input mode -> %d"), static_cast<int32>(InputMode));
}

void ANHPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		UNHInputSet* Set = GetInputSet();
		Input->BindAction(Set->CycleLighting, ETriggerEvent::Started, this, &ANHPlayerController::OnCycleLighting);
		Input->BindAction(Set->LightingMenu, ETriggerEvent::Started, this, &ANHPlayerController::NHLightMenu);
		Input->BindAction(Set->Interact, ETriggerEvent::Started, this, &ANHPlayerController::OnInteract);
		Input->BindAction(Set->ExitVehicle, ETriggerEvent::Started, this, &ANHPlayerController::OnInteract);
		Input->BindAction(Set->Action, ETriggerEvent::Started, this, &ANHPlayerController::OnAction);
		Input->BindAction(Set->Choice1, ETriggerEvent::Started, this, &ANHPlayerController::OnChoice1);
		Input->BindAction(Set->Choice2, ETriggerEvent::Started, this, &ANHPlayerController::OnChoice2);
		Input->BindAction(Set->Choice3, ETriggerEvent::Started, this, &ANHPlayerController::OnChoice3);
		Input->BindAction(Set->Choice4, ETriggerEvent::Started, this, &ANHPlayerController::OnChoice4);
	}
	// Plain key bindings for the HUD's screens: they must work while the game is paused, and need no input assets
	bShouldPerformFullTickWhenPaused = true;
	const auto Key = [this](const FKey& K, void (ANHPlayerController::*Fn)(), EInputEvent Event = IE_Pressed)
	{
		FInputKeyBinding& Binding = InputComponent->BindKey(K, Event, this, Fn);
		Binding.bExecuteWhenPaused = true;
		Binding.bConsumeInput = false;
	};
	Key(EKeys::M, &ANHPlayerController::UiMap);
	Key(EKeys::Escape, &ANHPlayerController::UiMenu);
	Key(EKeys::P, &ANHPlayerController::UiPhone);
	Key(EKeys::BackSpace, &ANHPlayerController::UiBack);
	// on-foot moves: R keeps you running without holding Shift, Left Ctrl or C rolls; Space climbs when there is a ledge (ANHCharacter::Jump)
	Key(EKeys::R, &ANHPlayerController::OnRunToggle); // in a car: hold for the radio wheel
	Key(EKeys::R, &ANHPlayerController::UiRadioClose, IE_Released);
	Key(EKeys::F2, &ANHPlayerController::OnStreamingOverlay);
	Key(EKeys::LeftControl, &ANHPlayerController::OnRoll);
	Key(EKeys::C, &ANHPlayerController::OnRoll);
	Key(EKeys::Tab, &ANHPlayerController::UiWheelOpen);
	Key(EKeys::Tab, &ANHPlayerController::UiWheelClose, IE_Released);
	Key(EKeys::Up, &ANHPlayerController::UiUp);
	Key(EKeys::Down, &ANHPlayerController::UiDown);
	Key(EKeys::Left, &ANHPlayerController::UiLeft);
	Key(EKeys::Right, &ANHPlayerController::UiRight);
	Key(EKeys::Enter, &ANHPlayerController::UiAccept);
	Key(EKeys::LeftMouseButton, &ANHPlayerController::UiClick);
	Key(EKeys::LeftMouseButton, &ANHPlayerController::UiClickEnd, IE_Released);
	Key(EKeys::RightMouseButton, &ANHPlayerController::UiRightClick);
	Key(EKeys::MouseScrollUp, &ANHPlayerController::UiZoomIn);
	Key(EKeys::MouseScrollDown, &ANHPlayerController::UiZoomOut);
}

void ANHPlayerController::UiMap() { if (ANHHUD* H = ANHHUD::Get(this)) { H->ToggleMap(); } }
void ANHPlayerController::OnStreamingOverlay()
{
	bStreamingOverlay = !bStreamingOverlay;
	ConsoleCommand(TEXT("wp.Runtime.ToggleDrawRuntimeHash2D"));
}

void ANHPlayerController::UpdateStreaming()
{
	// On foot, 450 m of city around you. Driving, 600 m plus 40 m for every m/s: at a danfo's 100 km/h that is
	// about 1.7 km, a minute of road ahead, so what you are driving towards is there before you are.
	ANHVehicle* Car = Cast<ANHVehicle>(GetPawn());
	const float Want = Car ? FMath::Min(60000.f + FMath::Abs(Car->Speed) * 40.f, 200000.f) : 45000.f;
	StreamingRadius = FMath::FInterpTo(StreamingRadius, Want, GetWorld()->GetDeltaSeconds(), Want > StreamingRadius ? 4.f : 0.5f); // widens at once, narrows slowly
	FStreamingSourceShape Shape;
	Shape.bUseGridLoadingRange = false;
	Shape.Radius = StreamingRadius;
	StreamingSourceShapes.SetNum(1);
	StreamingSourceShapes[0] = Shape;
	if (StreamingCar.IsValid() && StreamingCar.Get() != Car)
	{
		StreamingCar->SetStreamingRadius(false, 0.f);
	}
	if (Car)
	{
		Car->SetStreamingRadius(true, StreamingRadius);
	}
	StreamingCar = Car;
}

void ANHPlayerController::OnRunToggle()
{
	if (ANHCharacter* C = Cast<ANHCharacter>(GetPawn()))
	{
		C->ToggleRun();
	}
	else if (ANHHUD* H = ANHHUD::Get(this))
	{
		H->SetRadioWheel(true);
	}
}

void ANHPlayerController::UiRadioClose()
{
	if (ANHHUD* H = ANHHUD::Get(this))
	{
		H->SetRadioWheel(false);
	}
}

void ANHPlayerController::OnRoll()
{
	if (ANHCharacter* C = Cast<ANHCharacter>(GetPawn()))
	{
		C->Roll();
	}
}

void ANHPlayerController::UiPhone() { if (ANHHUD* H = ANHHUD::Get(this)) { H->TogglePhone(); } }
void ANHPlayerController::UiBack() { if (ANHHUD* H = ANHHUD::Get(this)) { H->Back(); } }
void ANHPlayerController::UiMenu() { if (ANHHUD* H = ANHHUD::Get(this)) { H->ToggleMenu(); } }
void ANHPlayerController::UiWheelOpen() { if (ANHHUD* H = ANHHUD::Get(this)) { H->SetWheel(true); } }
void ANHPlayerController::UiWheelClose() { if (ANHHUD* H = ANHHUD::Get(this)) { H->SetWheel(false); } }
void ANHPlayerController::UiUp() { if (ANHHUD* H = ANHHUD::Get(this)) { H->Nav(0, -1); } }
void ANHPlayerController::UiDown() { if (ANHHUD* H = ANHHUD::Get(this)) { H->Nav(0, 1); } }
void ANHPlayerController::UiLeft() { if (ANHHUD* H = ANHHUD::Get(this)) { H->Nav(-1, 0); } }
void ANHPlayerController::UiRight() { if (ANHHUD* H = ANHHUD::Get(this)) { H->Nav(1, 0); } }
void ANHPlayerController::UiAccept() { if (ANHHUD* H = ANHHUD::Get(this)) { H->Accept(); } }
void ANHPlayerController::UiClick()
{
	ANHHUD* H = ANHHUD::Get(this);
	if (H)
	{
		H->Click(false);
	}
	// with no screen open, on foot, the left button is the attack: fire, or swing
	if (ANHCharacter* C = Cast<ANHCharacter>(GetPawn()); C && (!H || H->GetScreen() == ANHHUD::EScreen::None) && !IsPaused())
	{
		C->SetTrigger(true);
	}
}

void ANHPlayerController::UiClickEnd()
{
	if (ANHCharacter* C = Cast<ANHCharacter>(GetPawn()))
	{
		C->SetTrigger(false);
	}
}
void ANHPlayerController::UiRightClick() { if (ANHHUD* H = ANHHUD::Get(this)) { H->Click(true); } }
void ANHPlayerController::UiZoomIn() { if (ANHHUD* H = ANHHUD::Get(this)) { H->Zoom(1); } }
void ANHPlayerController::UiZoomOut() { if (ANHHUD* H = ANHHUD::Get(this)) { H->Zoom(-1); } }

void ANHPlayerController::NHLighting(const FString& PresetName)
{
	ANHLightingRig* Rig = ANHLightingRig::Find(this);
	if (!Rig)
	{
		UE_LOG(LogNHGame, Warning, TEXT("NHLighting: no ANHLightingRig in this level"));
		return;
	}
	if (PresetName.IsEmpty())
	{
		Rig->CyclePreset();
	}
	else
	{
		const UEnum* Enum = StaticEnum<ENHLightingPreset>();
		const int64 Value = Enum->GetValueByNameString(PresetName);
		if (Value == INDEX_NONE)
		{
			UE_LOG(LogNHGame, Warning, TEXT("NHLighting: unknown preset '%s' (Day, DustyNoon, Sunset, NightRain, HarshMorning, GoldenEvening)"), *PresetName);
			return;
		}
		Rig->ApplyPreset(static_cast<ENHLightingPreset>(Value));
	}
	UE_LOG(LogNHGame, Log, TEXT("Lighting preset: %s"), *StaticEnum<ENHLightingPreset>()->GetNameStringByValue(static_cast<int64>(Rig->Preset)));
}

void ANHPlayerController::NHLightMenu()
{
	if (ANHGameDirector* Dir = ANHGameDirector::Get(this))
	{
		Dir->OpenLightingMenu();
	}
}

void ANHPlayerController::OnCycleLighting()
{
	NHLighting(FString());
	if (ANHGameDirector* Dir = ANHGameDirector::Get(this))
	{
		Dir->SetManualLighting();
	}
}

void ANHPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (ANHCharacter* C = Cast<ANHCharacter>(InPawn))
	{
		OnFootCharacter = C;
	}
	SetNHInputMode(Cast<ANHVehicle>(InPawn) ? ENHInputMode::Vehicle : ENHInputMode::OnFoot);
}

ANHVehicle* ANHPlayerController::NearbyVehicle() const
{
	const APawn* P = GetPawn();
	if (!P || Cast<ANHVehicle>(P))
	{
		return nullptr;
	}
	// Parked side by side, two vehicles can be equally near. Of those in reach, take the one whose body is closest,
	// leaning towards the one you are looking at and the one the job is asking for.
	const ANHGameDirector* Dir = ANHGameDirector::Get(this);
	const ANHVehicle* Wanted = Dir ? Dir->WantedVehicle() : nullptr;
	const FVector Look = FRotator(0.f, GetControlRotation().Yaw, 0.f).Vector();
	ANHVehicle* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (TActorIterator<ANHVehicle> It(GetWorld()); It; ++It)
	{
		const FVector To = (It->GetActorLocation() - P->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
		if (To.Size() >= It->EnterRadius() || It->IsWrecked() || It->GetController())
		{
			continue;
		}
		const FVector Local = It->GetActorTransform().InverseTransformPositionNoScale(P->GetActorLocation());
		const float Gap = static_cast<float>(FVector2D(FMath::Max(0.f, FMath::Abs(Local.X) - It->GetSpec().Length * 0.5f), FMath::Max(0.f, FMath::Abs(Local.Y) - It->GetSpec().Width * 0.5f)).Size());
		const float Score = Gap + (1.f - static_cast<float>(Look | To.GetSafeNormal())) * 150.f - (*It == Wanted ? 200.f : 0.f);
		if (Score < BestScore)
		{
			Best = *It;
			BestScore = Score;
		}
	}
	return Best;
}

bool ANHPlayerController::EnterVehicle(ANHVehicle* Vehicle)
{
	ANHCharacter* C = Cast<ANHCharacter>(GetPawn());
	if (!Vehicle || !C || Vehicle->GetController())
	{
		return false;
	}
	OnFootCharacter = C;
	C->SetActorHiddenInGame(true);
	C->SetActorEnableCollision(false);
	C->AttachToActor(Vehicle, FAttachmentTransformRules::KeepWorldTransform);
	Possess(Vehicle);
	if (LastVehicle.IsValid())
	{
		LastVehicle->bPlayerOwned = false;
	}
	LastVehicle = Vehicle;
	Vehicle->bPlayerOwned = true; // kept where it is left, for the car keys to find
	Vehicle->SetOccupied(true);
	SetControlRotation(Vehicle->GetActorRotation());
	// how the lights work, said once a session, and again whenever you set off in the dark without them
	if (ANHTraffic::IsDark(this) && !Vehicle->HeadlightsOn())
	{
		ANHHUD::Toast(this, TEXT("It is dark: press K for headlights"), 0);
	}
	else if (!bToldLights)
	{
		ANHHUD::Toast(this, TEXT("K: headlights on / off     V: cabin view     H: horn"), 0);
	}
	bToldLights = true;
	return true;
}

bool ANHPlayerController::LeaveVehicle(bool bForce)
{
	ANHVehicle* V = Cast<ANHVehicle>(GetPawn());
	if (!V || !OnFootCharacter)
	{
		return false;
	}
	if (!bForce && FMath::Abs(V->Speed) > 350.f)
	{
		ANHHUD::Toast(this, TEXT("Slow down before you jump out!"), 2);
		return false;
	}
	const FVector Out = V->ExitPoint();
	ANHCharacter* C = OnFootCharacter;
	C->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	C->SetActorLocationAndRotation(Out, FRotator(0.f, V->GetActorRotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	C->SetActorHiddenInGame(false);
	C->SetActorEnableCollision(true);
	V->SetOccupied(false);
	Possess(C);
	SetControlRotation(FRotator(-10.f, V->GetActorRotation().Yaw, 0.f));
	return true;
}

void ANHPlayerController::OnInteract()
{
	if (ANHGameDirector* Dir = ANHGameDirector::Get(this); Dir && Dir->IsBusy())
	{
		return;
	}
	if (ANHPhone* Phone = ANHPhone::Get(this); Phone && Phone->Interact())
	{
		return; // got into, or out of, a hailed ride
	}
	if (Cast<ANHVehicle>(GetPawn()))
	{
		LeaveVehicle();
	}
	else if (ANHVehicle* V = NearbyVehicle())
	{
		ANHCarTheft* Theft = ANHCarTheft::Get(this);
		if (Theft && Theft->Guards(V))
		{
			Theft->Approach(V); // not yours: the handle, the window, the wires, or the driver
		}
		else
		{
			V->Lock = ENHLock::Open; // your own, locked with the keys: they open it
			EnterVehicle(V);
		}
	}
}

void ANHPlayerController::OnAction()
{
	if (ANHCarTheft* Theft = ANHCarTheft::Get(this); Theft && Theft->Action())
	{
		return;
	}
	if (ANHGameDirector* Dir = ANHGameDirector::Get(this))
	{
		Dir->OnAction(GetPawn());
	}
}

void ANHPlayerController::Choose(int32 Index)
{
	if (ANHGameDirector* Dir = ANHGameDirector::Get(this))
	{
		Dir->OnChoice(Index);
	}
}

FString ANHPlayerController::Prompt() const
{
	const ANHGameDirector* Dir = ANHGameDirector::Get(this);
	if (Dir && Dir->IsBusy())
	{
		return FString();
	}
	const FString E = Dir ? Dir->ActionPrompt(GetPawn()) : FString();
	FString F;
	const ANHPhone* Phone = ANHPhone::Get(this);
	if (const FString Ride = Phone ? Phone->InteractPrompt() : FString(); !Ride.IsEmpty())
	{
		return Ride; // a hailed ride waiting, or the trip itself
	}
	const ANHCarTheft* Theft = ANHCarTheft::Get(this);
	if (const FString Wire = Theft ? Theft->ActionPrompt() : FString(); !Wire.IsEmpty())
	{
		return Wire; // the hotwire, or a place to do business at
	}
	if (const ANHVehicle* V = NearbyVehicle())
	{
		F = Theft && Theft->Guards(V) ? Theft->Prompt(V) : FString::Printf(TEXT("F  Get in %s"), *V->DisplayName());
	}
	else if (const ANHVehicle* In = Cast<ANHVehicle>(GetPawn()))
	{
		F = FMath::Abs(In->Speed) < 350.f ? TEXT("F  Get out") : FString();
	}
	if (E.IsEmpty() || F.IsEmpty())
	{
		return E + F;
	}
	return F + TEXT("      ") + E;
}

void ANHPlayerController::NHReset()
{
	if (UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this))
	{
		Hustle->ResetProgress();
		ANHHUD::Toast(this, TEXT("Progress reset. Reload the level to start the story again."), 2);
	}
}

void ANHPlayerController::NHAudio()
{
	const UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this);
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: %s"), Audio ? *Audio->Describe() : TEXT("audio: no subsystem"));
}

void ANHPlayerController::NHAudioSpace(const FString& Name)
{
	UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this);
	if (!Audio)
	{
		return;
	}
	ENHAudioSpace Held = ENHAudioSpace::Count; // "auto", or anything unknown
	for (int32 I = 0; I < static_cast<int32>(ENHAudioSpace::Count); ++I)
	{
		if (Name.Equals(UNHAudioSubsystem::SpaceName(static_cast<ENHAudioSpace>(I)), ESearchCase::IgnoreCase))
		{
			Held = static_cast<ENHAudioSpace>(I);
		}
	}
	Audio->ForceSpace(Held);
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: audio space %s"), Held == ENHAudioSpace::Count ? TEXT("follows where you stand") : *FString::Printf(TEXT("held at %s"), UNHAudioSubsystem::SpaceName(Held)));
}

void ANHPlayerController::NHRadio(const FString& What)
{
	UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this);
	if (!Audio)
	{
		return;
	}
	if (What.Equals(TEXT("off"), ESearchCase::IgnoreCase))
	{
		Audio->RadioOff();
	}
	else if (What.Equals(TEXT("track"), ESearchCase::IgnoreCase))
	{
		Audio->RadioNextTrack();
	}
	else
	{
		ANHVehicle* Car = Cast<ANHVehicle>(GetPawn());
		const FVector Here = GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
		for (TActorIterator<ANHVehicle> It(GetWorld()); !Cast<ANHVehicle>(GetPawn()) && It; ++It)
		{
			if (!Car || FVector::DistSquared(It->GetActorLocation(), Here) < FVector::DistSquared(Car->GetActorLocation(), Here))
			{
				Car = *It;
			}
		}
		Audio->RadioNextStation(Car);
	}
}

void ANHPlayerController::NHResponse()
{
	const ANHResponse* Response = ANHResponse::Get(this);
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: %s"), Response ? *Response->Describe() : TEXT("response: none in this level"));
}

void ANHPlayerController::ResponseTestStep(int32 Step)
{
	// -NHResponseTest: three stars where the player stands, then half a minute of whoever comes; logs it every five seconds
	ANHCharacter* C = Cast<ANHCharacter>(GetPawn());
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const ANHResponse* Response = ANHResponse::Get(this);
	if (!C || !Hustle || !Response || Step > 7)
	{
		ConsoleCommand(TEXT("quit"));
		return;
	}
	if (Step == 0)
	{
		UE_LOG(LogNHGame, Log, TEXT("[responsetest] %s"), *Response->AreaReport());
		if (const UNHGameData* Data = UNHGameData::Get(this); Data && Data->bRealCity)
		{
			// what three other parts of town would be
			static const TCHAR* Kinds[] = { TEXT("ordinary"), TEXT("area boys'"), TEXT("high-class") };
			for (const TCHAR* Name : { TEXT("Ikoyi"), TEXT("Yaba"), TEXT("Mushin"), TEXT("Ketu") })
			{
				FVector2D At;
				FString Station;
				float Distance = 0.f;
				if (Data->DistrictCentre(Name, At) && Response->NearestStation(FVector(At, 0.f), Station, Distance))
				{
					UE_LOG(LogNHGame, Log, TEXT("[responsetest] %s: %s streets, nearest station %s %.1f km"), Name, Kinds[static_cast<int32>(Response->AreaAt(FVector(At, 0.f)))], *Station, Distance / 100000.f);
				}
			}
		}
		Hustle->ClearHeat();
		Hustle->AddHeat(2.6f);
	}
	UE_LOG(LogNHGame, Log, TEXT("[responsetest] %2d s: %s; health %.0f, cash %d"), Step * 5, *Response->Describe().Replace(TEXT("\n"), TEXT(" | ")), C->Health, Hustle->Cash);
	FTimerHandle Next;
	GetWorldTimerManager().SetTimer(Next, FTimerDelegate::CreateWeakLambda(this, [this, Step] { ResponseTestStep(Step + 1); }), 5.f, false);
}

void ANHPlayerController::DamageTestStep(int32 Step)
{
	// -NHDamageTest: a dozen passers-by stood in front of the player, two seconds of AK-47 into them, then the machete on whoever is nearest
	ANHCharacter* C = Cast<ANHCharacter>(GetPawn());
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	if (!C || !Hustle)
	{
		ConsoleCommand(TEXT("quit"));
		return;
	}
	const auto Count = [this](int32& Standing, int32& Down, int32& Fleeing)
	{
		Standing = Down = Fleeing = 0;
		for (TActorIterator<ANHPerson> It(GetWorld()); It; ++It)
		{
			Down += It->IsDown() ? 1 : 0;
			Fleeing += It->IsFleeing() ? 1 : 0;
			Standing += !It->IsDown() ? 1 : 0;
		}
	};
	int32 Standing = 0, Down = 0, Fleeing = 0;
	float Wait = 1.f;
	switch (Step)
	{
	case 0:
		Hustle->ClearHeat();
		NHPeople(12, 500.f);
		SetControlRotation(FRotator(0.f, C->GetActorRotation().Yaw, 0.f));
		C->Equip(TEXT("ak47"));
		Wait = 1.5f;
		break;
	case 1:
		Count(Standing, Down, Fleeing);
		UE_LOG(LogNHGame, Log, TEXT("[damagetest] before: %d people standing, %d running, %d stars"), Standing, Fleeing, Hustle->Stars());
		C->SetTrigger(true);
		Wait = 2.f;
		break;
	case 2:
		C->SetTrigger(false);
		Count(Standing, Down, Fleeing);
		UE_LOG(LogNHGame, Log, TEXT("[damagetest] after 2 s of AK-47 (%d shots): %d hits on people, %d down, %d standing of whom %d running; %d hits on vehicles; %d stars"),
			C->Attacks, C->PeopleHit, Down, Standing, Fleeing, C->VehiclesHit, Hustle->Stars());
		break;
	case 3:
	{
		// somebody new right in front, and the machete
		C->Equip(TEXT("machete"));
		if (ANHPerson* Person = GetWorld()->SpawnActor<ANHPerson>(ANHPerson::StaticClass(), C->GetActorLocation() + C->GetActorForwardVector() * 110.f, FRotator::ZeroRotator))
		{
			Person->Init(977, FLinearColor(0.8f, 0.2f, 0.2f));
			Person->LifeLeft = 60.f;
		}
		C->PeopleHit = C->PeopleDown = 0;
		Wait = 0.8f;
		break;
	}
	case 4: case 6: C->SetTrigger(true); Wait = 0.1f; break;
	case 5: C->SetTrigger(false); Wait = 0.7f; break;
	case 7:
		C->SetTrigger(false);
		Wait = 0.8f;
		break;
	case 8:
		UE_LOG(LogNHGame, Log, TEXT("[damagetest] machete, two swings at somebody 1.1 m away: %d hits, %d down; %d stars"), C->PeopleHit, C->PeopleDown, Hustle->Stars());
		Hustle->ClearHeat();
		C->Equip(NAME_None);
		break;
	default:
		ConsoleCommand(TEXT("quit"));
		return;
	}
	FTimerHandle Next;
	GetWorldTimerManager().SetTimer(Next, FTimerDelegate::CreateWeakLambda(this, [this, Step] { DamageTestStep(Step + 1); }), Wait, false);
}

void ANHPlayerController::WeaponTestStep(int32 Step)
{
	// -NHWeaponTest: three pistol shots, a second of AK-47, two machete swings at a wall, recorded to Saved/NHAudio/nh_weapon_test.wav
	ANHCharacter* C = Cast<ANHCharacter>(GetPawn());
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("NHAudio"));
	float Wait = 0.5f;
	if (!C)
	{
		ConsoleCommand(TEXT("quit"));
		return;
	}
	switch (Step)
	{
	case 0: UAudioMixerBlueprintLibrary::StartRecordingOutput(this, 60.f); C->Equip(TEXT("pistol")); Wait = 1.f; break;
	case 1: case 3: case 5: C->SetTrigger(true); Wait = 0.1f; break;
	case 2: case 4: case 6: C->SetTrigger(false); Wait = Step == 6 ? 1.6f : 0.6f; break;
	case 7: UE_LOG(LogNHGame, Log, TEXT("[weapontest] pistol: %d shots"), C->Attacks); C->Attacks = 0; C->Equip(TEXT("ak47")); Wait = 1.f; break;
	case 8: C->SetTrigger(true); Wait = 1.f; break;
	case 9: C->SetTrigger(false); Wait = 2.2f; break;
	case 10: UE_LOG(LogNHGame, Log, TEXT("[weapontest] AK-47 held for a second: %d shots"), C->Attacks); C->Attacks = 0; C->Equip(TEXT("machete")); Wait = 1.f; break;
	case 11: case 13: C->SetTrigger(true); Wait = 0.1f; break;
	case 12: case 14: C->SetTrigger(false); Wait = 1.f; break;
	case 15:
		UE_LOG(LogNHGame, Log, TEXT("[weapontest] machete: %d swings"), C->Attacks);
		C->Equip(NAME_None);
		UAudioMixerBlueprintLibrary::StopRecordingOutput(this, EAudioRecordingExportType::WavFile, TEXT("nh_weapon_test"), Folder);
		Wait = 4.f;
		break;
	default:
		ConsoleCommand(TEXT("quit"));
		return;
	}
	FTimerHandle Next;
	GetWorldTimerManager().SetTimer(Next, FTimerDelegate::CreateWeakLambda(this, [this, Step] { WeaponTestStep(Step + 1); }), Wait, false);
}

void ANHPlayerController::RadioTestStep(int32 Step)
{
	UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this);
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("NHAudio"));
	static const float Waits[] = { 10.f, 10.f, 10.f, 4.f };
	switch (Step)
	{
	case 0: // into the nearest free car, radio on
	{
		ANHVehicle* Car = nullptr;
		const FVector Here = GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
		for (TActorIterator<ANHVehicle> It(GetWorld()); It; ++It)
		{
			if (!It->GetController() && !It->GetSpec().bBike && (!Car || FVector::DistSquared(It->GetActorLocation(), Here) < FVector::DistSquared(Car->GetActorLocation(), Here)))
			{
				Car = *It;
			}
		}
		UAudioMixerBlueprintLibrary::StartRecordingOutput(this, 60.f);
		const bool bIn = EnterVehicle(Car);
		UE_LOG(LogNHGame, Log, TEXT("[radiotest] 0 s: %s the %s; %d stations"), bIn ? TEXT("in") : TEXT("COULD NOT GET INTO"), Car ? *Car->DisplayName() : TEXT("(no car)"), Audio ? Audio->GetStations().Num() : 0);
		if (Audio && Car)
		{
			Audio->RadioNextStation(Car);
		}
		break;
	}
	case 1: // out: the same song, from the car
		UE_LOG(LogNHGame, Log, TEXT("[radiotest] 10 s: getting out: %s; muffled mix %d"), LeaveVehicle(true) ? TEXT("out") : TEXT("STILL IN"), Audio && Audio->MixOn(ENHMix::RadioMuffled) ? 1 : 0);
		break;
	case 2:
		if (Audio)
		{
			UE_LOG(LogNHGame, Log, TEXT("[radiotest] 20 s: muffled mix %d; next song"), Audio->MixOn(ENHMix::RadioMuffled) ? 1 : 0);
			Audio->RadioNextTrack();
		}
		break;
	case 3:
		UAudioMixerBlueprintLibrary::StopRecordingOutput(this, EAudioRecordingExportType::WavFile, TEXT("nh_radio_test"), Folder);
		UE_LOG(LogNHGame, Log, TEXT("[radiotest] 30 s: recorded %s"), *(Folder / TEXT("nh_radio_test.wav")));
		break;
	default:
		ConsoleCommand(TEXT("quit"));
		return;
	}
	FTimerHandle Next;
	GetWorldTimerManager().SetTimer(Next, FTimerDelegate::CreateWeakLambda(this, [this, Step] { RadioTestStep(Step + 1); }), Waits[Step], false);
}

void ANHPlayerController::NHAudioTest()
{
	GetWorld()->SpawnActor<ANHAudioTest>(ANHAudioTest::StaticClass(), FTransform::Identity);
}

void ANHPlayerController::NHTime(float Hour)
{
	if (UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this))
	{
		Hustle->Minutes = FMath::FloorToFloat(Hustle->Minutes / 1440.f) * 1440.f + FMath::Clamp(Hour, 0.f, 23.99f) * 60.f;
		UE_LOG(LogNHGame, Log, TEXT("Clock: %s"), *Hustle->ClockText());
	}
}

void ANHPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateStreaming();
	if (Debug)
	{
		Debug->Tick(DeltaTime);
	}
	if (bLookShotActive)
	{
		SetControlRotation(FRotator(-3.f, LookShotYaw, 0.f));
	}
	if (bLookFps)
	{
		++LookFpsFrames;
		LookFpsSeconds += FApp::GetDeltaTime();
	}
	if (bLookShotSprint)
	{
		if (APawn* P = GetPawn())
		{
			P->AddMovementInput(FRotator(0.f, GetControlRotation().Yaw, 0.f).Vector(), 1.f);
			const ACharacter* Body = Cast<ACharacter>(P);
			if (Body && Body->GetMesh() && Body->GetMesh()->DoesSocketExist(TEXT("foot_l")) && Body->GetMesh()->DoesSocketExist(TEXT("foot_r")))
			{
				const float Ahead = FVector::DotProduct(Body->GetMesh()->GetSocketLocation(TEXT("foot_l")) - Body->GetMesh()->GetSocketLocation(TEXT("foot_r")), P->GetActorForwardVector());
				LookStrideMin = FMath::Min(LookStrideMin, Ahead);
				LookStrideMax = FMath::Max(LookStrideMax, Ahead);
			}
		}
	}
}

void ANHPlayerController::NHLookShots(float X, float Y, float Yaw, const FString& Folder)
{
	ANHCharacter* Char = Cast<ANHCharacter>(GetPawn());
	if (!Char)
	{
		UE_LOG(LogNHGame, Warning, TEXT("NHLookShots: get out of the vehicle first"));
		return;
	}
	LookShotFolder = Folder.IsEmpty() ? FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots/NaijaLook")) : Folder;
	NHTime(LookShotHour - 0.05f); // the clock runs on while the scene settles; each shot sets the hour again
	if (AHUD* Hud = GetHUD())
	{
		Hud->bShowHUD = false;
	}
	Char->TeleportTo(FVector(X, Y, 130.f), FRotator(0.f, Yaw, 0.f));
	LookShotYaw = Yaw;
	bLookShotActive = true;
	SetControlRotation(FRotator(-3.f, Yaw, 0.f));
	// let the lighting, exposure and cables settle, then: the street, a sprint, the sprint shot
	GetWorldTimerManager().SetTimer(LookShotTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bLookFps = true; // standing still, looking down the street
		LookFpsFrames = 0;
		LookFpsSeconds = 0.0;
		GetWorldTimerManager().SetTimer(LookShotTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			bLookFps = false;
			const FIntPoint Size = GEngine && GEngine->GameViewport ? GEngine->GameViewport->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
			UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: frame rate standing still: %.1f fps average, %.1f ms a frame (%d frames in %.1f s at %dx%d)"),
				LookFpsFrames / FMath::Max(LookFpsSeconds, 0.001), 1000.0 * LookFpsSeconds / FMath::Max(LookFpsFrames, 1), LookFpsFrames, LookFpsSeconds, Size.X, Size.Y);
			LookShot(TEXT("street"));
			LookShotsSprint();
		}), 15.f, false);
	}), 8.f, false);
}

void ANHPlayerController::LookShot(const TCHAR* Name)
{
	const FString File = LookShotFolder / FString(Name) + TEXT(".png");
	NHTime(LookShotHour);
	ConsoleCommand(FString::Printf(TEXT("HighResShot 2560x1440 filename=\"%s\""), *File));
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: look shot %s"), *File);
}

void ANHPlayerController::LookShotsSprint()
{
	// give the first shot a moment to be taken before anything moves
	GetWorldTimerManager().SetTimer(LookShotTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (ANHCharacter* Char = Cast<ANHCharacter>(GetPawn()))
		{
			Char->SetSprinting(true);
		}
		bLookShotSprint = true;
		LookStrideMin = LookStrideMax = 0.f;
		GetWorldTimerManager().SetTimer(LookShotTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			LookShot(TEXT("sprint"));
			GetWorldTimerManager().SetTimer(LookShotTimer, this, &ANHPlayerController::LookShotsDone, 1.5f, false);
		}), 2.5f, false);
	}), 2.f, false);
}

void ANHPlayerController::LookShotsDone()
{
	bLookShotSprint = false;
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: running stride: the left foot got %.0f cm ahead of the right and %.0f cm behind it"), LookStrideMax, -LookStrideMin);
	bLookShotActive = false;
	if (AHUD* Hud = GetHUD())
	{
		Hud->bShowHUD = true;
	}
	if (ANHCharacter* Char = Cast<ANHCharacter>(GetPawn()))
	{
		Char->SetSprinting(false);
	}
	if (bLookShotQuit)
	{
		ConsoleCommand(TEXT("quit"));
	}
}

UNHDebugPlay* ANHPlayerController::DebugPlay()
{
#if !UE_BUILD_SHIPPING
	if (!Debug)
	{
		Debug = NewObject<UNHDebugPlay>(this);
		Debug->Init(this);
	}
#endif
	return Debug;
}

void ANHPlayerController::NHGoto(const FString& Where) { if (UNHDebugPlay* P = DebugPlay()) { P->Goto(Where); } }
void ANHPlayerController::NHBoard() { if (UNHDebugPlay* P = DebugPlay()) { P->Board(); } }
void ANHPlayerController::NHAgbero(const FString& What) { if (UNHDebugPlay* P = DebugPlay()) { P->Agbero(What); } }
void ANHPlayerController::NHFinish() { if (UNHDebugPlay* P = DebugPlay()) { P->Finish(); } }
void ANHPlayerController::NHAutoplay() { if (UNHDebugPlay* P = DebugPlay(); P && !P->IsRunning()) { P->Autoplay(false); } }
void ANHPlayerController::NHDriveShots(const FString& Type, const FString& Folder) { if (UNHDebugPlay* P = DebugPlay()) { P->DriveShots(FName(*Type), Folder); } }

void ANHPlayerController::NHSkinShots(const FString& Folder) { if (UNHDebugPlay* P = DebugPlay()) { P->SkinShots(Folder); } }

void ANHPlayerController::NHHeadlights()
{
	if (ANHVehicle* V = Cast<ANHVehicle>(GetPawn()))
	{
		V->SetHeadlights(!V->HeadlightsOn());
	}
}

void ANHPlayerController::NHWear(const FString& What, int32 Steps)
{
	ANHCharacter* Char = Cast<ANHCharacter>(GetPawn());
	const UNHOutfitComponent* Outfit = Char ? Char->GetOutfit() : nullptr;
	if (!Outfit || !Outfit->HasWardrobe())
	{
		UE_LOG(LogNHGame, Warning, TEXT("NHWear: this character has no wardrobe (pick one of the people with NHSkin, on foot)"));
		return;
	}
	const bool bColour = What.EndsWith(TEXT("colour"));
	const ENHOutfitSlot Slot = What.StartsWith(TEXT("hair")) ? ENHOutfitSlot::Hair : What.StartsWith(TEXT("top")) ? ENHOutfitSlot::Top : What.StartsWith(TEXT("bottom")) ? ENHOutfitSlot::Bottom : ENHOutfitSlot::Shoes;
	for (int32 I = 0; I < FMath::Abs(Steps); ++I)
	{
		Char->ChangeOutfit(Slot, Steps < 0 ? -1 : 1, bColour);
	}
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: wearing %s"), *Outfit->Describe());
}

void ANHPlayerController::NHPeople(int32 Count, float Distance)
{
	const APawn* Me = GetPawn();
	if (!Me)
	{
		return;
	}
	const FVector Ahead = Me->GetActorForwardVector(), Side = Me->GetActorRightVector();
	for (int32 I = 0; I < FMath::Clamp(Count, 1, 40); ++I)
	{
		const FVector At = Me->GetActorLocation() + Ahead * (Distance + FMath::Sign(Distance) * 140.f * (I / 8)) + Side * ((I % 8) - 3.5f) * 110.f;
		if (ANHPerson* Person = GetWorld()->SpawnActor<ANHPerson>(ANHPerson::StaticClass(), At, FRotator::ZeroRotator))
		{
			Person->Init(101 + I * 37, FLinearColor::MakeFromHSV8(static_cast<uint8>(I * 53), 170, 200), I % 3 == 1 ? ENHCast::Woman : ENHCast::Anyone);
			Person->FaceTowards(Me->GetActorLocation());
			Person->LifeLeft = 120.f;
		}
	}
}

void ANHPlayerController::NHSkin(const FString& Id)
{
	ANHCharacter* Char = Cast<ANHCharacter>(GetPawn());
	if (!Char)
	{
		UE_LOG(LogNHGame, Warning, TEXT("NHSkin: get out of the vehicle first"));
		return;
	}
	if (Id.IsEmpty())
	{
		const FString Name = Char->WearNextSkin();
		ANHHUD::Toast(this, Name.IsEmpty() ? FString(TEXT("No skins in this project")) : FString::Printf(TEXT("Skin: %s"), *Name), 1);
	}
	else if (!Char->WearSkin(FName(*Id), true))
	{
		FString Known;
		for (const FNHPlayerSkin& S : Char->Skins)
		{
			Known += (Known.IsEmpty() ? TEXT("") : TEXT(", ")) + S.Id.ToString();
		}
		UE_LOG(LogNHGame, Warning, TEXT("NHSkin: no skin '%s' in this project. Skins: %s"), *Id, *Known);
	}
}

void ANHPlayerController::NHCarShow(float X, float Y, const FString& Folder) { if (UNHDebugPlay* P = DebugPlay()) { P->CarShow(FVector(X, Y, 0.f), Folder); } }
void ANHPlayerController::NHPaintDemo(float X, float Y) { if (UNHDebugPlay* P = DebugPlay()) { P->PaintDemo(FVector(X, Y, 0.f)); } }
void ANHPlayerController::NHSelfTest() { if (UNHDebugPlay* P = DebugPlay(); P && !P->IsRunning()) { P->SelfTest(false); } }

void ANHPlayerController::NHCash(int32 Amount)
{
	if (UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this))
	{
		Hustle->Earn(Amount, TEXT("Console"));
	}
}
