#include "Core/NHHustleSubsystem.h"

#include "Core/NHGameData.h"
#include "Core/NHGameState.h"
#include "Core/NHPlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NaijaHustleGame.h"

namespace NHSave
{
	const TCHAR* Slot = TEXT("NaijaHustle");
}

UNHHustleSubsystem* UNHHustleSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UNHHustleSubsystem>() : nullptr;
}

FString UNHHustleSubsystem::Naira(int32 Amount)
{
	FString Digits = FString::FormatAsNumber(FMath::Abs(Amount));
	return (Amount < 0 ? TEXT("-N") : TEXT("N")) + Digits;
}

void UNHHustleSubsystem::Save(const UObject* WorldContext)
{
	const ANHPlayerState* Player = ANHPlayerState::Host(WorldContext);
	if (!Player)
	{
		return;
	}
	UNHSaveGame* S = Cast<UNHSaveGame>(UGameplayStatics::CreateSaveGameObject(UNHSaveGame::StaticClass()));
	if (!S)
	{
		return;
	}
	Player->WriteTo(*S);
	if (const ANHGameState* GS = ANHGameState::Get(WorldContext))
	{
		S->Minutes = GS->Minutes;
	}
	UGameplayStatics::SaveGameToSlot(S, NHSave::Slot, 0);
}

bool UNHHustleSubsystem::LoadInto(ANHPlayerState* Player, ANHGameState* World)
{
	if (!Player || !UGameplayStatics::DoesSaveGameExist(NHSave::Slot, 0))
	{
		return false;
	}
	const UNHSaveGame* S = Cast<UNHSaveGame>(UGameplayStatics::LoadGameFromSlot(NHSave::Slot, 0));
	if (!S)
	{
		return false;
	}
	Player->ReadFrom(*S);
	if (World)
	{
		World->SetMinutes(S->Minutes);
	}
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: save loaded (%s, %d jobs done)"), *Naira(S->Cash), S->Done.Num());
	return true;
}

void UNHHustleSubsystem::ResetProgress(const UObject* WorldContext)
{
	UGameplayStatics::DeleteGameInSlot(NHSave::Slot, 0);
	if (ANHPlayerState* Player = ANHPlayerState::Host(WorldContext))
	{
		Player->ResetToStart();
	}
	ANHGameState* GS = ANHGameState::Get(WorldContext);
	const UNHGameData* Data = UNHGameData::Get(WorldContext);
	if (GS && Data)
	{
		GS->SetMinutes(Data->StartMinutes);
	}
}
