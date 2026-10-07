#include "Player/NHPlayerController.h"

#include "Input/NHInputSet.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Core/NHHustleSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Gameplay/NHGameDirector.h"
#include "Lighting/NHLightingRig.h"
#include "Player/NHCharacter.h"
#include "UI/NHHUD.h"
#include "Vehicles/NHVehicle.h"
#include "EngineUtils.h"
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
			UE_LOG(LogNHGame, Warning, TEXT("NHLighting: unknown preset '%s' (Day, DustyNoon, Sunset, NightRain)"), *PresetName);
			return;
		}
		Rig->ApplyPreset(static_cast<ENHLightingPreset>(Value));
	}
	UE_LOG(LogNHGame, Log, TEXT("Lighting preset: %s"), *StaticEnum<ENHLightingPreset>()->GetNameStringByValue(static_cast<int64>(Rig->Preset)));
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
	ANHVehicle* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	for (TActorIterator<ANHVehicle> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist2D(It->GetActorLocation(), P->GetActorLocation());
		if (D < It->EnterRadius() && D < BestD && !It->IsWrecked() && !It->GetController())
		{
			Best = *It;
			BestD = D;
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

void ANHPlayerController::NHCash(int32 Amount)
{
	if (UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this))
	{
		Hustle->Earn(Amount, TEXT("Console"));
	}
}
