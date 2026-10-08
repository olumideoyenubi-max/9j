#include "Player/NHPlayerController.h"

#include "Input/NHInputSet.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Core/NHHustleSubsystem.h"
#include "Debug/NHDebugPlay.h"
#include "Components/CapsuleComponent.h"
#include "Gameplay/NHGameDirector.h"
#include "Lighting/NHLightingRig.h"
#include "Player/NHCharacter.h"
#include "UI/NHHUD.h"
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
}

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
	Vehicle->SetOccupied(true);
	SetControlRotation(Vehicle->GetActorRotation());
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
	if (Cast<ANHVehicle>(GetPawn()))
	{
		LeaveVehicle();
	}
	else if (ANHVehicle* V = NearbyVehicle())
	{
		EnterVehicle(V);
	}
}

void ANHPlayerController::OnAction()
{
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
	if (const ANHVehicle* V = NearbyVehicle())
	{
		F = FString::Printf(TEXT("F  Get in %s"), *V->DisplayName());
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

void ANHPlayerController::NHHeadlights()
{
	if (ANHVehicle* V = Cast<ANHVehicle>(GetPawn()))
	{
		V->SetHeadlights(!V->HeadlightsOn());
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
	else if (!Char->WearSkin(FName(*Id)))
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
