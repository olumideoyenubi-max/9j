#include "Player/NHPlayerController.h"

#include "Input/NHInputSet.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Core/NHGameData.h"
#include "Core/NHGameState.h"
#include "Core/NHHustleSubsystem.h"
#include "Core/NHPlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Gameplay/NHGameDirector.h"
#include "Lighting/NHLightingRig.h"
#include "Player/NHCharacter.h"
#include "UI/NHHUD.h"
#include "Vehicles/NHVehicle.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
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
		Input->BindAction(Set->SwitchLead, ETriggerEvent::Started, this, &ANHPlayerController::OnSwitchLead);
		Input->BindAction(Set->Ability, ETriggerEvent::Started, this, &ANHPlayerController::OnAbility);
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
	if (ANHGameState* GS = ANHGameState::Get(this))
	{
		GS->SetManualLighting();
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
	if (!HasAuthority())
	{
		ANHHUD::Toast(this, TEXT("Only the host can reset the story."), 2);
		return;
	}
	if (UNHHustleSubsystem* Saves = UNHHustleSubsystem::Get(this))
	{
		Saves->ResetProgress(this);
		ANHHUD::Toast(this, TEXT("Progress reset. Reload the level to start the story again."), 2);
	}
}

void ANHPlayerController::NHCash(int32 Amount)
{
	ServerCheatCash(Amount);
}

void ANHPlayerController::ServerCheatCash_Implementation(int32 Amount)
{
#if !UE_BUILD_SHIPPING
	if (ANHPlayerState* Stats = GetPlayerState<ANHPlayerState>())
	{
		Stats->Earn(Amount, TEXT("Console"));
	}
#endif
}

// ---------------------------------------------------------------------------------------------------- leads (Tunde, Amaka, ...)
void ANHPlayerController::ClientToast_Implementation(const FString& Text, int32 Kind)
{
	ANHHUD::Toast(this, Text, Kind);
}

ANHCharacter* ANHPlayerController::CurrentLead() const
{
	if (ANHCharacter* C = Cast<ANHCharacter>(GetPawn()))
	{
		return C;
	}
	// at the wheel the character rides along, attached to the vehicle
	if (const APawn* P = GetPawn())
	{
		TArray<AActor*> Riders;
		P->GetAttachedActors(Riders);
		for (AActor* A : Riders)
		{
			if (ANHCharacter* C = Cast<ANHCharacter>(A))
			{
				return C;
			}
		}
	}
	return OnFootCharacter;
}

ANHCharacter* ANHPlayerController::NextLead() const
{
	const UNHGameData* Data = UNHGameData::Get(this);
	ANHCharacter* Current = CurrentLead();
	if (!Data || !Current || Current->GetLeadId().IsNone())
	{
		return nullptr;
	}
	// this player's leads, in characters.json order
	TArray<ANHCharacter*> Mine;
	for (TActorIterator<ANHCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->GetLeadId().IsNone() && (*It == Current || It->GetOwner() == this))
		{
			Mine.Add(*It);
		}
	}
	auto Order = [Data](const ANHCharacter* C)
	{
		return Data->Characters.IndexOfByPredicate([C](const FNHCharacterInfo& I) { return I.Id == C->GetLeadId(); });
	};
	Mine.Sort([&Order](const ANHCharacter& A, const ANHCharacter& B) { return Order(&A) < Order(&B); });
	if (Mine.Num() < 2)
	{
		return nullptr;
	}
	const int32 I = Mine.IndexOfByKey(Current);
	return Mine[(I + 1) % Mine.Num()];
}

FString ANHPlayerController::SwitchBlocker() const
{
	if (!Cast<ANHCharacter>(GetPawn()))
	{
		return TEXT("Get out of the vehicle to switch.");
	}
	if (const ANHGameDirector* Dir = ANHGameDirector::Get(this); Dir && !Dir->CanSwitchLead())
	{
		return TEXT("You can't switch in the middle of a job.");
	}
	if (!NextLead())
	{
		return TEXT("Nobody else to switch to yet.");
	}
	return FString();
}

FVector ANHPlayerController::LeadSpawnPoint(FName SpawnStop) const
{
	const UNHGameData* Data = UNHGameData::Get(this);
	const APawn* Me = GetPawn();
	const FNHBusStop* Stop = (Data && !SpawnStop.IsNone()) ? Data->Stops.Find(SpawnStop) : nullptr;
	// a few metres back from where passengers wait, so they don't stand in the queue
	FVector P = Stop ? FVector(Stop->Wait.X + 150.f, Stop->Wait.Y + 150.f, 0.f)
		: (Me ? Me->GetActorLocation() + Me->GetActorRightVector() * 250.f : FVector::ZeroVector);
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(NHLeadSpawn), false, Me);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, FVector(P.X, P.Y, 20000.f), FVector(P.X, P.Y, -5000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Q))
	{
		P.Z = Hit.ImpactPoint.Z + 100.f; // the capsule's half height, and a bit
	}
	else if (Me)
	{
		P.Z = Me->GetActorLocation().Z;
	}
	return P;
}

void ANHPlayerController::RefreshLeads()
{
	const UNHGameData* Data = UNHGameData::Get(this);
	ANHCharacter* Current = CurrentLead();
	if (!HasAuthority() || !Data || !Current)
	{
		return;
	}
	const ANHPlayerState* Stats = GetPlayerState<ANHPlayerState>();
	if (Current->GetLeadId().IsNone() && Data->Characters.Num() > 0)
	{
		Current->SetLead(Data->Characters[0].Id); // Tunde: the story starts with him
	}
	for (const FNHCharacterInfo& Info : Data->Characters)
	{
		if (!Data->IsPlayable(Info, Stats))
		{
			continue;
		}
		bool bHave = false;
		for (TActorIterator<ANHCharacter> It(GetWorld()); It && !bHave; ++It)
		{
			bHave = It->GetLeadId() == Info.Id && (*It == Current || It->GetOwner() == this);
		}
		if (bHave)
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		const FVector At = LeadSpawnPoint(Info.SpawnStop);
		if (ANHCharacter* Lead = GetWorld()->SpawnActor<ANHCharacter>(Current->GetClass(), At, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Params))
		{
			Lead->SetLead(Info.Id);
			UE_LOG(LogNHGame, Log, TEXT("Lead ready: %s"), *Info.Name);
		}
	}
}

void ANHPlayerController::ServerSwitchLead_Implementation()
{
	const FString Blocker = SwitchBlocker();
	if (!Blocker.IsEmpty())
	{
		ClientToast(Blocker, 2);
		return;
	}
	ANHCharacter* From = CurrentLead();
	ANHCharacter* To = NextLead();
	if (!From || !To)
	{
		return;
	}
	Possess(To);
	From->SetOwner(this); // unpossessing cleared it; the owner is how we know it's still this player's lead
	SetControlRotation(FRotator(-10.f, To->GetActorRotation().Yaw, 0.f));
	ClientLeadSwitched(From);
	ClientToast(FString::Printf(TEXT("Now playing as %s"), *To->DisplayName()), 1);
}

void ANHPlayerController::ClientLeadSwitched_Implementation(AActor* From)
{
	// start the view on the lead we left, then fly across to the new one
	if (From && GetPawn())
	{
		SetViewTarget(From);
		SetViewTargetWithBlend(GetPawn(), 1.2f, EViewTargetBlendFunction::VTBlend_EaseInOut, 2.f);
	}
}

void ANHPlayerController::ServerUseAbility_Implementation()
{
	ANHCharacter* Lead = CurrentLead();
	if (!Lead)
	{
		return;
	}
	const FString Result = Lead->UseAbility(Cast<ANHVehicle>(GetPawn()));
	if (!Result.IsEmpty())
	{
		ClientToast(Result, Lead->IsAbilityActive() || Result.EndsWith(TEXT("!")) ? 1 : 0);
	}
}
