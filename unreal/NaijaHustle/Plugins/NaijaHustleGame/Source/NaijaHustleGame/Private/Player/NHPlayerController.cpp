#include "Player/NHPlayerController.h"
#include "Containers/Ticker.h"
#include "Gameplay/NHEstate.h"
#include "Kismet/GameplayStatics.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/NHInventory.h"
#include "Gameplay/NHLeads.h"
#include "Gameplay/NHMissions.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
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
#include "Gameplay/NHLaw.h"
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
				Run == TEXT("selftest") ? Play->SelfTest(bQuitAfter) : Run == TEXT("leads") ? Play->Leads(bQuitAfter) : Run == TEXT("systems") ? Play->Systems(bQuitAfter) : Run == TEXT("act1") || Run == TEXT("story1") ? Play->Story(bQuitAfter, 1) : Run == TEXT("story2") ? Play->Story(bQuitAfter, 2) : Run == TEXT("story3") ? Play->Story(bQuitAfter, 3) : Play->Autoplay(bQuitAfter);
			}
		}), 4.f, false);
	}
#endif

	if (FParse::Param(FCommandLine::Get(), TEXT("NHResponseTest")))
	{
		FTimerHandle Start;
		GetWorldTimerManager().SetTimer(Start, FTimerDelegate::CreateWeakLambda(this, [this] { ResponseTestStep(0); }), 8.f, false);
	}
	if (const bool bTrees = FParse::Param(FCommandLine::Get(), TEXT("NHTreeTest")); bTrees || FParse::Param(FCommandLine::Get(), TEXT("NHRoomTest")))
	{
		// -NHTreeTest -NHNoSave: three places with trees about, a picture and the frame rate at each: Saved/NHTrees/
		// -NHRoomTest -NHNoSave: into the chief's house and the madam's flat, two pictures of each: Saved/NHRooms/
		struct FStep { float At; const TCHAR* Do; };
		static const FStep Steps[] = { { 10.f, TEXT("NHPlayAs chief") }, { 15.f, TEXT("NHPlaceUse 0") }, { 19.f, TEXT("shot house_1") }, { 20.f, TEXT("turn 120") }, { 22.f, TEXT("shot house_2") }, { 23.f, TEXT("turn 240") }, { 25.f, TEXT("shot house_3") },
			{ 26.f, TEXT("NHPlayAs madam") }, { 31.f, TEXT("NHPlaceUse 0") }, { 35.f, TEXT("shot flat_1") }, { 36.f, TEXT("turn 120") }, { 38.f, TEXT("shot flat_2") }, { 39.f, TEXT("turn 240") }, { 41.f, TEXT("shot flat_3") }, { 43.f, TEXT("quit") } };
		static const FStep TreeSteps[] = { { 10.f, TEXT("NHPlace land_ikoyi") }, { 11.f, TEXT("turn 200") }, { 19.f, TEXT("fps Ikoyi") }, { 19.5f, TEXT("shot 1_ikoyi") }, { 20.f, TEXT("turn 20") }, { 23.f, TEXT("shot 2_ikoyi") },
			{ 24.f, TEXT("NHPlace house_vi") }, { 25.f, TEXT("turn 90") }, { 33.f, TEXT("fps Victoria Island") }, { 33.5f, TEXT("shot 3_vi") },
			{ 34.f, TEXT("NHPlace house_yaba") }, { 35.f, TEXT("turn 300") }, { 43.f, TEXT("fps Yaba") }, { 43.5f, TEXT("shot 4_yaba") },
			{ 45.f, TEXT("NHPlace atlantic_penthouse") }, { 46.f, TEXT("turn 180") }, { 52.f, TEXT("shot 5_atlantic_a") }, { 53.f, TEXT("turn 90") }, { 56.f, TEXT("shot 6_atlantic_b") }, { 57.f, TEXT("turn 270") }, { 60.f, TEXT("shot 7_atlantic_c") },
			{ 61.f, TEXT("NHPlace land_lekki") }, { 62.f, TEXT("turn 0") }, { 68.f, TEXT("shot 8_lekki_a") }, { 69.f, TEXT("turn 180") }, { 72.f, TEXT("shot 9_lekki_b") }, { 74.f, TEXT("quit") } };
		// -NHTreeTest -NHGround: the grass, the forest floor, the beach and the water
		static const FStep GroundSteps[] = { { 10.f, TEXT("NHAt 282051 349417") }, { 11.f, TEXT("turn 0") }, { 18.f, TEXT("shot g1_grass") }, { 19.f, TEXT("turn 180") }, { 21.f, TEXT("shot g2_grass") },
			{ 22.f, TEXT("NHAt 237660 144533") }, { 23.f, TEXT("turn 90") }, { 30.f, TEXT("shot g3_forest") },
			{ 31.f, TEXT("NHAt -42328 750439") }, { 32.f, TEXT("turn 90") }, { 39.f, TEXT("shot g4_beach_south") }, { 40.f, TEXT("turn 0") }, { 42.f, TEXT("shot g5_beach_east") }, { 43.f, TEXT("turn 200") }, { 45.f, TEXT("shot g6_beach_west") }, { 47.f, TEXT("quit") } };
		// -NHRoomTest -NHLiving: the things done at home, a picture of each
		static const FStep LivingSteps[] = { { 10.f, TEXT("NHPlayAs chief") }, { 15.f, TEXT("NHPlaceUse 0") }, { 18.f, TEXT("turn 200") }, { 19.f, TEXT("NHPlaceUse 2") }, { 21.f, TEXT("shot live_1_sit") },
			{ 22.f, TEXT("NHPlaceUse 3") }, { 24.f, TEXT("shot live_2_lie_down") }, { 25.f, TEXT("NHPlaceUse 4") }, { 27.f, TEXT("shot live_3_eat") }, { 34.f, TEXT("NHPlaceUse 5") }, { 36.f, TEXT("shot live_4_drink") },
			{ 41.f, TEXT("NHPlaceUse 6") }, { 44.f, TEXT("shot live_5_shower") }, { 51.f, TEXT("NHPlaceUse 7") }, { 55.f, TEXT("shot live_6_swim") }, { 66.f, TEXT("shot live_7_back_in_the_room") }, { 68.f, TEXT("quit") } };
		const bool bLiving = FParse::Param(FCommandLine::Get(), TEXT("NHLiving"));
		for (const FStep& Step : bTrees && FParse::Param(FCommandLine::Get(), TEXT("NHGround")) ? MakeArrayView(GroundSteps) : bTrees ? MakeArrayView(TreeSteps) : bLiving ? MakeArrayView(LivingSteps) : MakeArrayView(Steps))
		{
			const FString Do = Step.Do;
			FTimerHandle Handle;
			GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Do]
			{
				if (Do.StartsWith(TEXT("shot ")))
				{
					FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / (FParse::Param(FCommandLine::Get(), TEXT("NHTreeTest")) ? TEXT("NHTrees") : TEXT("NHRooms")) / (Do.RightChop(5) + TEXT(".png")), false, false);
				}
				else if (Do.StartsWith(TEXT("fps ")))
				{
					extern ENGINE_API float GAverageFPS;
					UE_LOG(LogNHGame, Log, TEXT("[trees] %s: %.0f frames a second"), *Do.RightChop(4), GAverageFPS);
				}
				else if (Do.StartsWith(TEXT("turn ")))
				{
					SetControlRotation(FRotator(-8.f, FCString::Atof(*Do.RightChop(5)), 0.f));
				}
				else
				{
					ConsoleCommand(Do);
				}
			}), Step.At, false);
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("NHPhoneTest")))
	{
		// -NHPhoneTest: the phone's home screen, the browser and three of its sites, a picture of each in Saved/NHPhone/
		static const TCHAR* Shots[] = { TEXT(""), TEXT("start"), TEXT("ekohomes"), TEXT("coastbank"), TEXT("motorhaus"), TEXT("headlines") };
		for (int32 I = 0; I <= UE_ARRAY_COUNT(Shots); ++I)
		{
			FTimerHandle Step;
			GetWorldTimerManager().SetTimer(Step, FTimerDelegate::CreateWeakLambda(this, [this, I]
			{
				ANHPhone* Phone = ANHPhone::Get(this);
				if (!Phone || I >= UE_ARRAY_COUNT(Shots))
				{
					ConsoleCommand(TEXT("quit"));
					return;
				}
				Phone->DebugSite(Shots[I]);
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("NHPhone") / FString::Printf(TEXT("%d_%s.png"), I, I == 0 ? TEXT("home") : Shots[I]), true, false);
			}), 10.f + 2.f * I, false);
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("NHEstateTest")))
	{
		FTimerHandle Start;
		GetWorldTimerManager().SetTimer(Start, FTimerDelegate::CreateWeakLambda(this, [this] { EstateTestStep(0); }), 9.f, false);
	}
	if (FString Ride; FParse::Value(FCommandLine::Get(), TEXT("NHRideTest="), Ride))
	{
		FTimerHandle Start;
		GetWorldTimerManager().SetTimer(Start, FTimerDelegate::CreateWeakLambda(this, [this] { RideTestStep(0); }), 9.f, false);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("NHBagTest")))
	{
		FTimerHandle Start;
		GetWorldTimerManager().SetTimer(Start, FTimerDelegate::CreateWeakLambda(this, [this] { BagTestStep(0); }), 8.f, false);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("NHActionTest")))
	{
		FTimerHandle Start;
		GetWorldTimerManager().SetTimer(Start, FTimerDelegate::CreateWeakLambda(this, [this] { ActionTestStep(0); }), 8.f, false);
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
	Key(EKeys::X, &ANHPlayerController::OnCrouch); // down into a crouch and up again
	Key(EKeys::G, &ANHPlayerController::OnClimb);
	Key(EKeys::B, &ANHPlayerController::OnBag);
	Key(EKeys::N, &ANHPlayerController::NHWho); // who to be
	Key(EKeys::T, &ANHPlayerController::OnFire);
	Key(EKeys::T, &ANHPlayerController::OnFireEnd, IE_Released);
	Key(EKeys::Tab, &ANHPlayerController::OnTabDown);
	Key(EKeys::Tab, &ANHPlayerController::OnTabUp, IE_Released);
	Key(EKeys::Z, &ANHPlayerController::OnAbility);
	Key(EKeys::Up, &ANHPlayerController::UiUp);
	Key(EKeys::Down, &ANHPlayerController::UiDown);
	Key(EKeys::Left, &ANHPlayerController::UiLeft);
	Key(EKeys::Right, &ANHPlayerController::UiRight);
	Key(EKeys::Enter, &ANHPlayerController::UiAccept);
	Key(EKeys::LeftMouseButton, &ANHPlayerController::UiClick);
	Key(EKeys::LeftMouseButton, &ANHPlayerController::UiClickEnd, IE_Released);
	Key(EKeys::RightMouseButton, &ANHPlayerController::UiRightClick);
	Key(EKeys::RightMouseButton, &ANHPlayerController::OnAimEnd, IE_Released);
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

void ANHPlayerController::OnBag()
{
	if (ANHHUD* H = ANHHUD::Get(this))
	{
		H->ToggleBag();
	}
}

void ANHPlayerController::NHBag()
{
	const ANHCharacter* C = GetOnFootCharacter();
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: bag: %s"), C && C->GetInventory() ? *C->GetInventory()->Describe() : TEXT("nobody to carry one"));
}

void ANHPlayerController::NHGive(const FString& Item, int32 HowMany)
{
	const ANHCharacter* C = GetOnFootCharacter();
	if (!HasAuthority() || !C || !C->GetInventory())
	{
		return;
	}
	const bool bGiven = C->GetInventory()->Add(FName(*Item), FMath::Max(HowMany, 1), TEXT("console"));
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: bag: %s %d %s; %s"), bGiven ? TEXT("given") : TEXT("could not give"), FMath::Max(HowMany, 1), *Item, *C->GetInventory()->Describe());
}

void ANHPlayerController::BagTestStep(int32 Step)
{
	ANHCharacter* C = Cast<ANHCharacter>(GetPawn());
	UNHInventoryComponent* Bag = C ? C->GetInventory() : nullptr;
	if (!Bag || Step > 9)
	{
		ConsoleCommand(TEXT("quit"));
		return;
	}
	float Wait = 0.6f;
	const auto Say = [C, Bag](const TCHAR* What)
	{
		UE_LOG(LogNHGame, Log, TEXT("[bagtest] %s: health %.0f, holding %s, shots %d; %s"), What, C->Health, *C->Equipped().ToString(), C->Attacks, *Bag->Describe());
	};
	switch (Step)
	{
	case 0: Say(TEXT("at the start")); C->Hurt(60.f); break;
	case 1: Bag->ServerUse(TEXT("meat_pie")); Say(TEXT("hurt for 60, then a meat pie")); break;
	case 2: Say(TEXT("too heavy, and unknown, are refused"));
		UE_LOG(LogNHGame, Log, TEXT("[bagtest] 40 jerry cans: %s; a second pistol: %s; a thing that is not an item: %s"), Bag->Add(TEXT("jerry_can"), 40, TEXT("test")) ? TEXT("TAKEN (wrong)") : TEXT("refused"),
			Bag->Add(TEXT("pistol"), 1, TEXT("test")) ? TEXT("TAKEN (wrong)") : TEXT("refused"), Bag->Add(TEXT("gold_bar"), 1, TEXT("test")) ? TEXT("TAKEN (wrong)") : TEXT("refused"));
		break;
	case 3: Bag->Remove(TEXT("pistol_ammo"), Bag->Count(TEXT("pistol_ammo")) - 2, TEXT("test")); C->Equip(TEXT("pistol")); Say(TEXT("pistol out, two rounds left")); break;
	case 4: case 5: case 6: C->SetTrigger(true); Wait = 0.3f; break; // three pulls on two rounds
	case 7: C->SetTrigger(false); Say(TEXT("after three pulls of the trigger")); Bag->ServerDrop(TEXT("pistol"), 1); break;
	case 8: Say(TEXT("pistol dropped")); C->Equip(TEXT("pistol")); OnBag(); Wait = 1.5f; break;
	case 9: Say(TEXT("asked for the pistol again; the bag is open"));
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("NHBag") / TEXT("bag.png"), true, false);
		Wait = 1.f;
		break;
	}
	if (Step >= 4 && Step <= 6)
	{
		FTimerHandle Up;
		GetWorldTimerManager().SetTimer(Up, FTimerDelegate::CreateWeakLambda(this, [C] { C->SetTrigger(false); }), 0.1f, false);
	}
	FTimerHandle Next;
	GetWorldTimerManager().SetTimer(Next, FTimerDelegate::CreateWeakLambda(this, [this, Step] { BagTestStep(Step + 1); }), Wait, false);
}

void ANHPlayerController::OnClimb()
{
	if (ANHCharacter* C = Cast<ANHCharacter>(GetPawn()))
	{
		C->Climb();
	}
}

void ANHPlayerController::OnCrouch()
{
	if (ANHCharacter* C = Cast<ANHCharacter>(GetPawn()))
	{
		C->ToggleCrouch();
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
}

void ANHPlayerController::UiClickEnd()
{
}

void ANHPlayerController::ActionTestStep(int32 Step)
{
	// what is done at each step, and the name of the picture taken at the end of it
	struct FDo { const TCHAR* Weapon; bool bCrouch; bool bFire; float Wait; const TCHAR* Picture; bool bAim = true; bool bWalk = false; float Hurt = 0.f; };
	static const FDo Steps[] = {
		{ TEXT("machete"), false, false, 1.2f, TEXT("1_machete_guard") }, { nullptr, false, true, 0.24f, TEXT("2_machete_slash") }, { nullptr, false, false, 0.7f, nullptr },
		{ nullptr, false, true, 0.24f, TEXT("3_machete_backslash") }, { nullptr, false, false, 0.7f, nullptr },
		{ TEXT("pistol"), false, false, 1.2f, TEXT("4_pistol_aim") }, { nullptr, false, true, 0.07f, TEXT("5_pistol_fire") },
		{ TEXT("ak47"), false, false, 1.2f, TEXT("6_rifle_aim") }, { nullptr, true, false, 1.2f, TEXT("7_crouch_rifle_aim") },
		{ TEXT("ak47"), true, false, 1.2f, TEXT("8_crouch") }, { nullptr, false, false, 1.0f, TEXT("9_standing") },
		// carried, not raised, stood and walking; then the weapon used on the move
		{ TEXT("machete"), false, false, 1.4f, TEXT("10_machete_carried"), false }, { nullptr, false, false, 1.6f, TEXT("11_machete_walking"), false, true },
		{ nullptr, false, true, 0.24f, TEXT("12_machete_walking_cut"), false, true }, { nullptr, false, false, 0.7f, nullptr, false, true },
		{ TEXT("pistol"), false, false, 1.4f, TEXT("13_pistol_carried"), false }, { nullptr, false, false, 1.6f, TEXT("14_pistol_walking"), false, true },
		{ nullptr, false, false, 1.2f, TEXT("15_pistol_walking_aimed"), true, true },
		{ TEXT("ak47"), false, false, 1.4f, TEXT("16_rifle_carried"), false }, { nullptr, false, false, 1.6f, TEXT("17_rifle_walking"), false, true },
		{ nullptr, false, true, 0.5f, TEXT("18_rifle_walking_firing"), false, true }, { nullptr, false, false, 1.0f, TEXT("19_rifle_after"), false },
		// bare hands: the rifle put away, two punches and a kick, a blow taken, and down
		{ TEXT("ak47"), false, false, 0.8f, nullptr, false }, { nullptr, false, true, 0.2f, TEXT("20_punch"), false }, { nullptr, false, false, 0.3f, nullptr, false },
		{ nullptr, false, true, 0.2f, TEXT("21_punch_again"), false }, { nullptr, false, false, 0.3f, nullptr, false }, { nullptr, false, true, 0.28f, TEXT("22_kick"), false },
		{ nullptr, false, false, 0.9f, TEXT("23_fists_up"), false }, { nullptr, false, false, 0.15f, TEXT("24_hit"), false, false, 12.f }, { nullptr, false, false, 2.0f, nullptr, false },
		{ nullptr, false, false, 1.2f, TEXT("25_knocked_out"), false, false, 500.f },
	};
	ANHCharacter* C = Cast<ANHCharacter>(GetPawn());
	if (!C || Step >= UE_ARRAY_COUNT(Steps))
	{
		ActionWalk = FVector::ZeroVector;
		ConsoleCommand(TEXT("quit"));
		return;
	}
	if (!ActionLens)
	{
		const FVector Eye = C->GetActorLocation() + C->GetActorForwardVector() * 330.f + C->GetActorRightVector() * -210.f + FVector(0.f, 0.f, 40.f);
		ActionLens = GetWorld()->SpawnActor<ACameraActor>(Eye, (C->GetActorLocation() + FVector(0.f, 0.f, 10.f) - Eye).Rotation());
		ActionLens->GetCameraComponent()->SetFieldOfView(50.f);
		ActionLens->GetCameraComponent()->SetConstraintAspectRatio(false);
		ActionLens->AttachToActor(C, FAttachmentTransformRules::KeepWorldTransform); // it goes with the body when the body walks
		SetViewTarget(ActionLens);
		ActionAhead = C->GetActorForwardVector();
		SetControlRotation(C->GetActorRotation());
	}
	const FDo& Do = Steps[Step];
	// walking steps go out and back along one line, so the test stays where it started
	if (Do.bWalk && ActionWalk.IsZero())
	{
		ActionAhead = -ActionAhead;
	}
	ActionWalk = Do.bWalk ? ActionAhead : FVector::ZeroVector;
	if (Do.bWalk)
	{
		SetControlRotation(ActionAhead.Rotation());
	}
	if (Step == 0)
	{
		const USkeletalMesh* Mesh = C->GetMesh()->GetSkeletalMeshAsset();
		const UAnimInstance* Anim = C->GetMesh()->GetAnimInstance();
		UE_LOG(LogNHGame, Log, TEXT("[actiontest] body %s, animated by %s, crouch clip %s"), *GetPathNameSafe(Mesh), Anim ? *Anim->GetClass()->GetName() : TEXT("nothing"),
			*GetPathNameSafe(ANHCharacter::ActionClip(Mesh, TEXT("Crouch_Idle"))));
	}
	if (Do.Weapon)
	{
		C->Equip(Do.Weapon); // the one already held is put away
	}
	if (Do.bCrouch != C->bIsCrouched)
	{
		C->ToggleCrouch();
	}
	C->SetTrigger(Do.bFire);
	C->SetAiming(Do.bAim);
	if (Do.Hurt > 0.f)
	{
		C->Hurt(Do.Hurt);
	}
	FTimerHandle Next;
	GetWorldTimerManager().SetTimer(Next, FTimerDelegate::CreateWeakLambda(this, [this, Step, C, Do]
	{
		UE_LOG(LogNHGame, Log, TEXT("[actiontest] %s: holding %s, showing %s%s"), Do.Picture ? Do.Picture : TEXT("-"), *C->Equipped().ToString(), *C->ActionShown().ToString(), C->bIsCrouched ? TEXT(", crouched") : TEXT(""));
		if (Do.Picture)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("NHActions") / (FString(Do.Picture) + TEXT(".png")), false, false);
		}
		C->SetTrigger(false);
		FTimerHandle After;
		GetWorldTimerManager().SetTimer(After, FTimerDelegate::CreateWeakLambda(this, [this, Step] { ActionTestStep(Step + 1); }), 0.25f, false);
	}), Do.Wait, false);
}

void ANHPlayerController::OnFire()
{
	// T, with no screen open, on foot, is the attack: fire, or swing. (In a car T is the radio's next song.)
	const ANHHUD* H = ANHHUD::Get(this);
	if (ANHCharacter* C = Cast<ANHCharacter>(GetPawn()); C && (!H || H->GetScreen() == ANHHUD::EScreen::None) && !IsPaused())
	{
		C->SetTrigger(true);
	}
}

void ANHPlayerController::OnFireEnd()
{
	if (ANHCharacter* C = Cast<ANHCharacter>(GetPawn()))
	{
		C->SetTrigger(false);
	}
}
void ANHPlayerController::UiRightClick()
{
	ANHHUD* H = ANHHUD::Get(this);
	if (H)
	{
		H->Click(true);
	}
	// with no screen open, on foot, the right button aims the gun in the hand
	if (ANHCharacter* C = Cast<ANHCharacter>(GetPawn()); C && (!H || H->GetScreen() == ANHHUD::EScreen::None) && !IsPaused())
	{
		C->SetAiming(true);
	}
}

void ANHPlayerController::OnAimEnd()
{
	if (ANHCharacter* C = Cast<ANHCharacter>(GetPawn()))
	{
		C->SetAiming(false);
	}
}
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
	// F in a story mission's talk: the rest of the scene is skipped
	if (ANHGameDirector* Talk = ANHGameDirector::Get(this); Talk && Talk->Dialogue.bOpen && ANHMissions::Get(this) && ANHMissions::Get(this)->IsActive())
	{
		for (int32 Guard = 0; Guard < 16 && Talk->Dialogue.bOpen; ++Guard)
		{
			Talk->OnAction(GetPawn());
		}
		return;
	}
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

void ANHPlayerController::EstateTestStep(int32 Step)
{
	ANHEstate* Estate = ANHEstate::Get(this);
	if (!Estate || Step > 12)
	{
		ConsoleCommand(TEXT("quit"));
		return;
	}
	const auto Shot = [](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("NHEstate") / (FString(Name) + TEXT(".png")), true, false); };
	float Wait = 3.f;
	switch (Step)
	{
	case 0: NHPlayAs(TEXT("chief")); Wait = 5.f; break;
	case 1:
		Shot(TEXT("1_chief_at_home"));
		UE_LOG(LogNHGame, Log, TEXT("[estate] chief: %s"), *Estate->Describe());
		if (FParse::Param(FCommandLine::Get(), TEXT("NHHomeTest")))
		{
			// -NHEstateTest -NHHomeTest: into the home, save and sleep, travel to the other home, and the map
			FTimerHandle A, B, C, D, E;
			GetWorldTimerManager().SetTimer(A, FTimerDelegate::CreateWeakLambda(this, [this] { NHPlaceUse(0); }), 1.f, false);                               // go in
			GetWorldTimerManager().SetTimer(B, FTimerDelegate::CreateWeakLambda(this, [this, Shot] { Shot(TEXT("h1_inside_home")); NHPlaceUse(0); }), 4.f, false); // save
			GetWorldTimerManager().SetTimer(C, FTimerDelegate::CreateWeakLambda(this, [this] { NHPlaceUse(1); NHPlace(TEXT("apt_vi")); }), 6.f, false);        // sleep; then to a flat for sale
			GetWorldTimerManager().SetTimer(D, FTimerDelegate::CreateWeakLambda(this, [this] { NHPlaceUse(0); if (ANHPhone* Phone = ANHPhone::Get(this)) { Phone->DebugKeys(); } }), 9.f, false); // buy it; the Keys app
			GetWorldTimerManager().SetTimer(E, FTimerDelegate::CreateWeakLambda(this, [this, Shot]
			{
				Shot(TEXT("h2_keys"));
				ANHEstate* Now = ANHEstate::Get(this);
				UE_LOG(LogNHGame, Log, TEXT("[estate] travel home: %s; %s"), Now && Now->Travel(TEXT("lekki_mansion")) ? TEXT("there") : TEXT("REFUSED"), Now ? *Now->Describe() : TEXT(""));
				if (ANHPhone* Phone = ANHPhone::Get(this)) { Phone->Close(); }
			}), 11.f, false);
			FTimerHandle F;
			GetWorldTimerManager().SetTimer(F, FTimerDelegate::CreateWeakLambda(this, [this, Shot]
			{
				// the map stops the game, and the game's timers with it: the picture is asked for now and the quit left to a real-time ticker
				if (ANHHUD* H = Cast<ANHHUD>(GetHUD())) { H->ToggleMap(); }
				Shot(TEXT("h3_map"));
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float) { ConsoleCommand(TEXT("quit")); return false; }), 3.f);
			}), 14.f, false);
			return;
		}
		Wait = 1.f;
		break;
	case 2: NHPlace(TEXT("club_lekki")); break;
	case 3: Shot(TEXT("2_club_door")); NHPlaceUse(0); break;                 // go in
	case 4: Shot(TEXT("3_club_inside")); NHPlaceUse(3); Wait = 1.5f; break; // champagne for the table
	case 5: NHPlaceUse(5); NHPlace(TEXT("strip_vi")); break;                 // out, and across town
	case 6: NHPlaceUse(0); break;
	case 7: Shot(TEXT("4_strip_club_inside")); Wait = 1.f; break;
	case 8: NHPlaceUse(5); NHPlace(TEXT("land_ikoyi")); break;
	case 9: Shot(TEXT("5_land_for_sale")); NHPlaceUse(0); Wait = 2.f; break; // buy it
	case 10: Shot(TEXT("6_land_bought")); NHPlace(TEXT("petrol_lekki")); break;
	case 11: Shot(TEXT("7_petrol")); NHPlayAs(TEXT("madam")); Wait = 5.f; break;
	default: Shot(TEXT("8_madam_at_home")); UE_LOG(LogNHGame, Log, TEXT("[estate] madam: %s"), *Estate->Describe()); Wait = 1.f; break;
	}
	FTimerHandle Next;
	GetWorldTimerManager().SetTimer(Next, FTimerDelegate::CreateWeakLambda(this, [this, Step] { EstateTestStep(Step + 1); }), Wait, false);
}

void ANHPlayerController::RideTestStep(int32 Step)
{
	FString Type;
	FParse::Value(FCommandLine::Get(), TEXT("NHRideTest="), Type);
	const auto Shot = [&Type](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("NHRide") / (Type + TEXT("_") + Name + TEXT(".png")), true, false); };
	const auto Say = [this](const TCHAR* What) { UE_LOG(LogNHGame, Log, TEXT("[ridetest] %s: %s"), What, RideTestCar ? *RideTestCar->DescribeMotion() : TEXT("no vehicle")); };
	float Wait = 2.f;
	if (Step == 0 && GetPawn())
	{
		// on the nearest road, in its right-hand lane and facing along it, where the map has roads; else just ahead of the player
		FTransform Where(GetPawn()->GetActorRotation(), GetPawn()->GetActorLocation() + GetPawn()->GetActorForwardVector() * 500.f + FVector(0.f, 0.f, 80.f));
		const UNHGameData* Data = UNHGameData::Get(this);
		FNHRoadSeg Seg;
		FVector2D OnRoad;
		if (Data && Data->bRealCity && Data->NearestRoad(FVector2D(GetPawn()->GetActorLocation()), Seg, OnRoad))
		{
			const FNHRoadWay& Way = Data->RoadWays[Seg.Way];
			const FVector2D Along = (Data->RoadNodes[Way.Nodes[Seg.Index + 1]] - Data->RoadNodes[Way.Nodes[Seg.Index]]).GetSafeNormal();
			Where = FTransform(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), 0.f), FVector(OnRoad + FVector2D(-Along.Y, Along.X) * Data->LaneOffset(Way, 1.f), GetPawn()->GetActorLocation().Z + 60.f));
		}
		RideTestCar = GetWorld()->SpawnActorDeferred<ANHVehicle>(ANHVehicle::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (RideTestCar)
		{
			RideTestCar->VehicleType = FName(*Type);
			UGameplayStatics::FinishSpawningActor(RideTestCar, Where);
			RideTestCar->Fuel = 0.6f;
		}
	}
	if (!RideTestCar || Step > 6)
	{
		ConsoleCommand(TEXT("quit"));
		return;
	}
	switch (Step)
	{
	case 0: break;
	case 1: UE_LOG(LogNHGame, Log, TEXT("[ridetest] got on: %s"), EnterVehicle(RideTestCar) ? TEXT("yes") : TEXT("NO")); Say(TEXT("standing")); Shot(TEXT("1_standing")); Wait = 1.f; break;
	case 2: RideTestCar->SetDriveInput(1.f, 0.f, 0.f); Wait = 4.f; break;
	case 3: Say(TEXT("after 4 s of throttle")); Shot(TEXT("2_riding")); RideTestCar->SetDriveInput(1.f, 0.f, 1.f); Wait = 1.5f; break;
	case 4: Say(TEXT("turning right")); Shot(TEXT("3_turning")); RideTestCar->SetDriveInput(1.f, 0.f, -1.f); Wait = 1.5f; break;
	case 5: Say(TEXT("turning left")); RideTestCar->SetDriveInput(0.f, 1.f, 0.f); Wait = 2.5f; break;
	default: Say(TEXT("after braking")); Shot(TEXT("4_stopped")); RideTestCar->SetDriveInput(0.f, 0.f, 0.f); Wait = 1.f; break;
	}
	FTimerHandle Next;
	GetWorldTimerManager().SetTimer(Next, FTimerDelegate::CreateWeakLambda(this, [this, Step] { RideTestStep(Step + 1); }), Wait, false);
}

void ANHPlayerController::NHPlayAs(const FString& Who)
{
	if (ANHEstate* Estate = ANHEstate::Get(this))
	{
		Estate->PlayAs(FName(*Who));
	}
}

void ANHPlayerController::OnTabDown()
{
	bTabWheel = false;
	GetWorldTimerManager().SetTimer(TabHold, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bTabWheel = true;
		UiWheelOpen();
	}), 0.25f, false);
}

void ANHPlayerController::OnTabUp()
{
	GetWorldTimerManager().ClearTimer(TabHold);
	if (bTabWheel)
	{
		UiWheelClose();
	}
	else if (const ANHHUD* H = ANHHUD::Get(this); H && H->GetScreen() == ANHHUD::EScreen::None && !IsPaused())
	{
		NHSwitch(FString());
	}
	bTabWheel = false;
}

void ANHPlayerController::OnAbility()
{
	if (const ANHHUD* H = ANHHUD::Get(this); H && H->GetScreen() == ANHHUD::EScreen::None && !IsPaused())
	{
		NHAbility();
	}
}

void ANHPlayerController::TravelTo(const FVector& At, float Yaw, const FString& Arrived)
{
	if (Cast<ANHVehicle>(GetPawn()))
	{
		LeaveVehicle(true);
	}
	ANHCharacter* Me = Cast<ANHCharacter>(GetPawn());
	if (!Me)
	{
		return;
	}
	bTravelling = true;
	TravelAt = At;
	TravelYaw = Yaw;
	TravelT = 0.f;
	TravelLine = Arrived;
	SetIgnoreMoveInput(true);
	Me->GetCharacterMovement()->StopMovementImmediately();
	Me->GetCharacterMovement()->SetMovementMode(MOVE_None);
	Me->SetActorLocation(At + FVector(0.f, 0.f, 400.f), false, nullptr, ETeleportType::TeleportPhysics);
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(1.f, 1.f, 0.1f, FLinearColor::Black, false, true);
	}
}

void ANHPlayerController::NHMission(const FString& Id)
{
	if (ANHMissions* Story = ANHMissions::Get(this))
	{
		if (Id.IsEmpty())
		{
			FString Known;
			for (const FName& Have : Story->Known())
			{
				Known += Have.ToString() + TEXT(" ");
			}
			UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: missions: %s"), *Known);
		}
		else
		{
			Story->Start(FName(*Id));
		}
	}
}

void ANHPlayerController::NHObjective()
{
	if (ANHMissions* Story = ANHMissions::Get(this))
	{
		Story->SkipObjective();
	}
}

void ANHPlayerController::NHMissionAbort()
{
	if (ANHMissions* Story = ANHMissions::Get(this))
	{
		Story->Abort(TEXT("console"));
	}
}

void ANHPlayerController::NHCheckpoint()
{
	if (ANHMissions* Story = ANHMissions::Get(this))
	{
		Story->RestartFromCheckpoint();
	}
}

void ANHPlayerController::NHSwitch(const FString& Who)
{
	if (ANHLeads* Leads = ANHLeads::Get(this))
	{
		Leads->Switch(Who.IsEmpty() ? NAME_None : FName(*Who));
	}
}

void ANHPlayerController::NHAbility()
{
	if (ANHLeads* Leads = ANHLeads::Get(this))
	{
		Leads->UseAbility();
	}
}

void ANHPlayerController::NHMeter(float Value)
{
	if (ANHLeads* Leads = ANHLeads::Get(this))
	{
		Leads->SetMeter(Leads->Current(), Value);
	}
}

void ANHPlayerController::NHFlag(const FString& Name, int32 Value)
{
	if (UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this); Hustle && !Name.IsEmpty())
	{
		Hustle->SetFlag(FName(*Name), Value);
		UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: flag %s = %d"), *Name, Value);
	}
}

void ANHPlayerController::NHWho()
{
	if (ANHEstate* Estate = ANHEstate::Get(this))
	{
		Estate->OpenPeople();
	}
}

void ANHPlayerController::NHPlace(const FString& Id)
{
	ANHEstate* Estate = ANHEstate::Get(this);
	UE_LOG(LogNHGame, Log, TEXT("[estate] to %s: %s"), *Id, Estate && Estate->GoTo(FName(*Id)) ? TEXT("there") : TEXT("no such place, or nowhere to stand it"));
}

void ANHPlayerController::NHAt(float X, float Y)
{
	FHitResult Hit;
	const bool bGround = GetWorld()->LineTraceSingleByObjectType(Hit, FVector(X, Y, 30000.f), FVector(X, Y, -3000.f), FCollisionObjectQueryParams(ECC_WorldStatic));
	if (GetPawn())
	{
		GetPawn()->SetActorLocation(FVector(X, Y, (bGround ? Hit.ImpactPoint.Z : 0.f) + 110.f), false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void ANHPlayerController::NHPlaceUse(int32 Line)
{
	if (ANHEstate* Estate = ANHEstate::Get(this))
	{
		if (!Estate->MenuOpen())
		{
			Estate->Interact(GetPawn());
		}
		FString Title, Heading;
		TArray<FNHMenuLine> Lines;
		Estate->Menu(Title, Heading, Lines);
		UE_LOG(LogNHGame, Log, TEXT("[estate] %s / %s: choosing %d of %d: %s"), *Title, *Heading, Line, Lines.Num(), Lines.IsValidIndex(Line) ? *(Lines[Line].Label + TEXT("  ") + Lines[Line].Value) : TEXT("nothing"));
		Estate->Choose(Line);
		UE_LOG(LogNHGame, Log, TEXT("[estate] now: %s"), *Estate->Describe());
	}
}

void ANHPlayerController::OnAction()
{
	if (ANHMissions* Story = ANHMissions::Get(this); Story && Story->OnAction(GetPawn()))
	{
		return; // a guard taken down from behind
	}
	if (ANHEstate* Estate = ANHEstate::Get(this); Estate && Estate->Interact(GetPawn()))
	{
		return; // a place's board, door, counter or pumps
	}
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
	if (const ANHMissions* Story = ANHMissions::Get(this); Story && !Story->ActionPrompt(GetPawn()).IsEmpty())
	{
		return Story->ActionPrompt(GetPawn());
	}
	const ANHEstate* Estate = ANHEstate::Get(this);
	const FString AtPlace = Estate ? Estate->Prompt(GetPawn()) : FString();
	const FString E = !AtPlace.IsEmpty() ? AtPlace : Dir ? Dir->ActionPrompt(GetPawn()) : FString();
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
	// -NHResponseTest: three stars where the player stands (or -NHResponseStars=N), then half a minute of whoever comes; logs it every five seconds
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
		int32 Stars = 3; // -NHResponseStars=5 for the army
		FParse::Value(FCommandLine::Get(), TEXT("NHResponseStars="), Stars);
		Hustle->AddHeat(FMath::Clamp(Stars, 1, 5) - 0.4f);
	}
	if (FVector Ride; Step == 3 && Response->RideAt(Ride))
	{
		// a picture of what they came in, and of them, from where the player stands: Saved/NHResponse/response_<stars>.png
		const FVector Eye = C->GetActorLocation() + (Ride - C->GetActorLocation()).GetSafeNormal2D() * 120.f + FVector(0.f, 0.f, 110.f);
		ACameraActor* Lens = GetWorld()->SpawnActor<ACameraActor>(Eye, (Ride + FVector(0.f, 0.f, 60.f) - Eye).Rotation());
		if (Lens)
		{
			Lens->GetCameraComponent()->SetFieldOfView(48.f);
			Lens->GetCameraComponent()->SetConstraintAspectRatio(false);
		}
		const FString Shot = FPaths::ProjectSavedDir() / TEXT("NHResponse") / FString::Printf(TEXT("response_%d.png"), Hustle->Stars());
		if (Lens)
		{
			SetViewTarget(Lens);
			FTimerHandle Later, Back;
			GetWorldTimerManager().SetTimer(Later, FTimerDelegate::CreateWeakLambda(this, [Shot] { FScreenshotRequest::RequestScreenshot(Shot, false, false); }), 0.8f, false);
			GetWorldTimerManager().SetTimer(Back, FTimerDelegate::CreateWeakLambda(this, [this, Lens] { SetViewTarget(GetPawn()); Lens->Destroy(); }), 1.6f, false);
			UE_LOG(LogNHGame, Log, TEXT("[responsetest] picture: %s"), *Shot);
		}
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
		if (const ANHLaw* Law = ANHLaw::Get(this))
		{
			UE_LOG(LogNHGame, Log, TEXT("[damagetest] %s"), *Law->Describe());
		}
		C->Equip(NAME_None);
		Wait = 8.f; // the reports are phone calls: give them time to be made
		break;
	case 9:
		if (const ANHLaw* Law = ANHLaw::Get(this))
		{
			UE_LOG(LogNHGame, Log, TEXT("[damagetest] 8 s later, %d stars: %s"), Hustle->Stars(), *Law->Describe());
		}
		Hustle->ClearHeat();
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
	if (!ActionWalk.IsZero() && GetPawn())
	{
		GetPawn()->AddMovementInput(ActionWalk); // -NHActionTest walking
	}
	UpdateStreaming();
	if (bTravelling)
	{
		TravelT += DeltaTime;
		ANHCharacter* Me = Cast<ANHCharacter>(GetPawn());
		FHitResult Floor;
		const bool bFloor = GetWorld()->LineTraceSingleByObjectType(Floor, FVector(TravelAt.X, TravelAt.Y, 30000.f), FVector(TravelAt.X, TravelAt.Y, -3000.f), FCollisionObjectQueryParams(ECC_WorldStatic));
		if (Me && TravelT > 0.5f && (bFloor || TravelT > 10.f))
		{
			Me->SetActorLocationAndRotation(FVector(TravelAt.X, TravelAt.Y, (bFloor ? Floor.ImpactPoint.Z : TravelAt.Z) + 100.f), FRotator(0.f, TravelYaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
			Me->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			SetControlRotation(FRotator(-10.f, TravelYaw, 0.f));
			SetIgnoreMoveInput(false);
			if (PlayerCameraManager)
			{
				PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.8f, FLinearColor::Black, false, false);
			}
			if (!TravelLine.IsEmpty())
			{
				ANHHUD::Toast(this, TravelLine, 0);
			}
			bTravelling = false;
		}
	}
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
