#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NHInventory.generated.h"

/** One kind of thing that can be carried (Data/items.json) */
struct FNHItemDef
{
	FName Name;
	FString Label, Kind, Description;
	/** Grams each */
	int32 Weight = 0;
	int32 Price = 0;
	/** Health given back when it is used */
	int32 Heal = 0;
	/** Only one may be carried */
	bool bUnique = false;
	bool bUseable = false;
	/** What it puts in the hand (ANHCharacter::Equip), and the weapon its rounds load */
	FName Weapon, AmmoFor;
};

/** So many of one thing */
USTRUCT()
struct FNHItemStack
{
	GENERATED_BODY()
	UPROPERTY() FName Item;
	UPROPERTY() int32 Count = 0;
};

/**
 * What one player carries: a list of things and how many of each, with a limit on the weight.
 *
 * The server owns it. Things are only ever added or taken away on the server (Add, Remove), the list is replicated to
 * the player who owns it, and a player's own machine asks for what it wants done with a Server call (ServerUse,
 * ServerDrop) which the server checks before doing. That is the shape the GTA roleplay frameworks use for their
 * inventories, and it means a second player cannot be given bullets or money by their own machine.
 *
 * It lives on the player's character. Console: NHBag (what you carry), NHGive <item> <how many>.
 */
UCLASS()
class NAIJAHUSTLEGAME_API UNHInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNHInventoryComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The item list, read once from Data/items.json */
	static const TMap<FName, FNHItemDef>& Items();
	static const FNHItemDef* Def(FName Item) { return Items().Find(Item); }
	static int32 MaxWeight();
	/** The rounds a weapon fires ("" for a blade) */
	static FName AmmoOf(FName Weapon);

	const TArray<FNHItemStack>& GetStacks() const { return Stacks; }
	int32 Count(FName Item) const;
	/** Grams carried */
	int32 Weight() const;

	/** Server only. False, and nothing changes, if it is not the server, the item is unknown, it is unique and already carried, or it would be too heavy. */
	bool Add(FName Item, int32 HowMany, const FString& Why);
	/** Server only. False, and nothing changes, if there are not that many. */
	bool Remove(FName Item, int32 HowMany, const FString& Why);
	/** Server only: what a new player starts with (the "start" list in items.json), if they carry nothing */
	void GiveStartingKit();

	/** Eat it, drink it, bind the wound. Asked for by the owner's machine, decided by the server. */
	UFUNCTION(Server, Reliable) void ServerUse(FName Item);
	/** Put some of it down. They are gone: nothing is left on the ground yet. */
	UFUNCTION(Server, Reliable) void ServerDrop(FName Item, int32 HowMany);

	FString Describe() const;

private:
	UPROPERTY(Replicated) TArray<FNHItemStack> Stacks;
};
