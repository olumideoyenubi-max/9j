#include "Gameplay/NHInventory.h"
#include "Kismet/GameplayStatics.h"
#include "Vehicles/NHVehicle.h"

#include "Core/NHGameData.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "NaijaHustleGame.h"
#include "Net/UnrealNetwork.h"
#include "Player/NHCharacter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UI/NHHUD.h"

namespace NHItems
{
	struct FBook
	{
		TMap<FName, FNHItemDef> Items;
		TMap<FName, int32> Start;
		int32 MaxWeight = 30000;
	};

	const FBook& Book()
	{
		static FBook B;
		static bool bRead = false;
		if (bRead)
		{
			return B;
		}
		bRead = true;
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Text, *(UNHGameData::DataDir() / TEXT("items.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
		{
			UE_LOG(LogNHGame, Warning, TEXT("NAIJA HUSTLE: inventory: Data/items.json could not be read"));
			return B;
		}
		Root->TryGetNumberField(TEXT("maxWeightGrams"), B.MaxWeight);
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (Root->TryGetArrayField(TEXT("items"), List))
		{
			for (const TSharedPtr<FJsonValue>& Value : *List)
			{
				const TSharedPtr<FJsonObject>* J = nullptr;
				if (!Value->TryGetObject(J))
				{
					continue;
				}
				FNHItemDef D;
				FString Name, Weapon, AmmoFor;
				(*J)->TryGetStringField(TEXT("name"), Name);
				(*J)->TryGetStringField(TEXT("label"), D.Label);
				(*J)->TryGetStringField(TEXT("kind"), D.Kind);
				(*J)->TryGetStringField(TEXT("description"), D.Description);
				(*J)->TryGetNumberField(TEXT("weight"), D.Weight);
				(*J)->TryGetNumberField(TEXT("price"), D.Price);
				(*J)->TryGetNumberField(TEXT("heal"), D.Heal);
				(*J)->TryGetBoolField(TEXT("unique"), D.bUnique);
				(*J)->TryGetBoolField(TEXT("useable"), D.bUseable);
				(*J)->TryGetStringField(TEXT("weapon"), Weapon);
				(*J)->TryGetStringField(TEXT("ammoFor"), AmmoFor);
				D.Name = FName(*Name);
				D.Weapon = Weapon.IsEmpty() ? NAME_None : FName(*Weapon);
				D.AmmoFor = AmmoFor.IsEmpty() ? NAME_None : FName(*AmmoFor);
				if (!Name.IsEmpty())
				{
					B.Items.Add(D.Name, D);
				}
			}
		}
		const TSharedPtr<FJsonObject>* Start = nullptr;
		if (Root->TryGetObjectField(TEXT("start"), Start))
		{
			for (const auto& Pair : (*Start)->Values)
			{
				B.Start.Add(FName(*Pair.Key), static_cast<int32>(Pair.Value->AsNumber()));
			}
		}
		UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: inventory: %d kinds of item, %d in the starting kit, %.0f kg carried at most"), B.Items.Num(), B.Start.Num(), B.MaxWeight / 1000.f);
		return B;
	}
}

UNHInventoryComponent::UNHInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNHInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UNHInventoryComponent, Stacks, COND_OwnerOnly); // what you carry is your own business
}

const TMap<FName, FNHItemDef>& UNHInventoryComponent::Items()
{
	return NHItems::Book().Items;
}

int32 UNHInventoryComponent::MaxWeight()
{
	return NHItems::Book().MaxWeight;
}

FName UNHInventoryComponent::AmmoOf(FName Weapon)
{
	for (const TPair<FName, FNHItemDef>& Pair : Items())
	{
		if (!Weapon.IsNone() && Pair.Value.AmmoFor == Weapon)
		{
			return Pair.Key;
		}
	}
	return NAME_None;
}

int32 UNHInventoryComponent::Count(FName Item) const
{
	for (const FNHItemStack& Stack : Stacks)
	{
		if (Stack.Item == Item)
		{
			return Stack.Count;
		}
	}
	return 0;
}

int32 UNHInventoryComponent::Weight() const
{
	int32 Grams = 0;
	for (const FNHItemStack& Stack : Stacks)
	{
		if (const FNHItemDef* D = Def(Stack.Item))
		{
			Grams += D->Weight * Stack.Count;
		}
	}
	return Grams;
}

bool UNHInventoryComponent::Add(FName Item, int32 HowMany, const FString& Why)
{
	const FNHItemDef* D = Def(Item);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !D || HowMany <= 0)
	{
		return false;
	}
	const int32 Have = Count(Item);
	if ((D->bUnique && (Have > 0 || HowMany > 1)) || Weight() + D->Weight * HowMany > MaxWeight())
	{
		return false;
	}
	if (Have > 0)
	{
		for (FNHItemStack& Stack : Stacks)
		{
			if (Stack.Item == Item)
			{
				Stack.Count += HowMany;
			}
		}
	}
	else
	{
		FNHItemStack Stack;
		Stack.Item = Item;
		Stack.Count = HowMany;
		Stacks.Add(Stack);
	}
	UE_LOG(LogNHGame, Verbose, TEXT("NAIJA HUSTLE: inventory: %s +%d %s (%s)"), *GetNameSafe(GetOwner()), HowMany, *Item.ToString(), *Why);
	return true;
}

bool UNHInventoryComponent::Remove(FName Item, int32 HowMany, const FString& Why)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || HowMany <= 0 || Count(Item) < HowMany)
	{
		return false;
	}
	for (int32 I = Stacks.Num() - 1; I >= 0; --I)
	{
		if (Stacks[I].Item == Item)
		{
			Stacks[I].Count -= HowMany;
			if (Stacks[I].Count <= 0)
			{
				Stacks.RemoveAt(I);
			}
		}
	}
	UE_LOG(LogNHGame, Verbose, TEXT("NAIJA HUSTLE: inventory: %s -%d %s (%s)"), *GetNameSafe(GetOwner()), HowMany, *Item.ToString(), *Why);
	return true;
}

void UNHInventoryComponent::GiveStartingKit()
{
	if (Stacks.Num() > 0)
	{
		return;
	}
	for (const TPair<FName, int32>& Pair : NHItems::Book().Start)
	{
		Add(Pair.Key, Pair.Value, TEXT("starting kit"));
	}
}

void UNHInventoryComponent::ServerUse_Implementation(FName Item)
{
	// the server decides: it must be carried, be something that is used up, and do something for this body now
	const FNHItemDef* D = Def(Item);
	ANHCharacter* Body = Cast<ANHCharacter>(GetOwner());
	if (D && Body && Item == TEXT("jerry_can") && Count(Item) > 0)
	{
		// twenty litres into the nearest vehicle within reach
		ANHVehicle* Car = nullptr;
		float Best = FMath::Square(450.f);
		TArray<AActor*> Cars;
		UGameplayStatics::GetAllActorsOfClass(this, ANHVehicle::StaticClass(), Cars);
		for (AActor* Have : Cars)
		{
			if (const float Far = FVector::DistSquared(Have->GetActorLocation(), Body->GetActorLocation()); Far < Best)
			{
				Best = Far;
				Car = Cast<ANHVehicle>(Have);
			}
		}
		if (!Car)
		{
			ANHHUD::Toast(this, TEXT("Stand by the car you want to fill"), 0);
		}
		else if (Car->Fuel > 0.95f)
		{
			ANHHUD::Toast(this, TEXT("Tank don full"), 0);
		}
		else if (Remove(Item, 1, TEXT("poured")))
		{
			Car->Fuel = FMath::Min(1.f, Car->Fuel + 20.f / Car->TankLitres());
			ANHHUD::Toast(this, FString::Printf(TEXT("%s: tank %.0f%%"), *Car->DisplayName(), Car->Fuel * 100.f), 1);
		}
		return;
	}
	if (!D || !Body || !D->bUseable || D->Heal <= 0 || Count(Item) <= 0)
	{
		return;
	}
	if (Body->Health >= 100.f)
	{
		ANHHUD::Toast(this, TEXT("You dey okay. Keep am."), 0);
		return;
	}
	if (Remove(Item, 1, TEXT("used")))
	{
		Body->Health = FMath::Min(100.f, Body->Health + D->Heal);
		ANHHUD::Toast(this, FString::Printf(TEXT("%s. Health %.0f"), *D->Label, Body->Health), 1);
	}
}

void UNHInventoryComponent::ServerDrop_Implementation(FName Item, int32 HowMany)
{
	const FNHItemDef* D = Def(Item);
	HowMany = FMath::Min(HowMany, Count(Item));
	if (D && HowMany > 0 && Remove(Item, HowMany, TEXT("dropped")))
	{
		if (ANHCharacter* Body = Cast<ANHCharacter>(GetOwner()); Body && Body->Equipped() == D->Weapon && !D->Weapon.IsNone())
		{
			Body->Equip(D->Weapon); // it was in the hand: the hand is empty now
		}
		ANHHUD::Toast(this, FString::Printf(TEXT("Dropped %d %s"), HowMany, *D->Label), 0);
	}
}

FString UNHInventoryComponent::Describe() const
{
	FString Out = FString::Printf(TEXT("carrying %.1f of %.0f kg:"), Weight() / 1000.f, MaxWeight() / 1000.f);
	for (const FNHItemStack& Stack : Stacks)
	{
		const FNHItemDef* D = Def(Stack.Item);
		Out += FString::Printf(TEXT(" %s x%d,"), D ? *D->Label : *Stack.Item.ToString(), Stack.Count);
	}
	Out.RemoveFromEnd(TEXT(","));
	return Out;
}
