#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NHOutfitComponent.generated.h"

class USkeletalMeshComponent;
class UMaterialInterface;

enum class ENHOutfitSlot : uint8 { Hair, Top, Bottom, Shoes, Count };

/**
 * Clothes that can be changed: dresses one of the people built by Scripts/build_people_makehuman.py (its
 * "wardrobe" mode) from the pieces Scripts/import_wardrobe.py put in /Game/Characters/Player/<Folder>/Wardrobe.
 *
 * The body there has no hair or clothes; each piece is its own mesh on the same skeleton and follows the body's
 * pose. Pieces are found by name, <Slot>_<Piece>[_D]: the slot is Hair, Top, Bottom, Shoes or Full (a top and
 * bottom in one, chosen as a top) and _D marks what the person starts in. The body's skin is split into patches,
 * and Data/wardrobe.json says which of them each piece covers: those are hidden while it is worn. Each slot can
 * also be given a colour.
 * The player's choices are saved per character; ANHPerson uses Pick for a random passer-by.
 */
UCLASS()
class NAIJAHUSTLEGAME_API UNHOutfitComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** The people with a wardrobe, men first; the first Men of them are men */
	static const TArray<FString>& People();
	static constexpr int32 Men = 5;
	static bool Exists(const FString& Folder);

	/** Puts the bare body on InBody and dresses it: in Saved (from Describe) if given, otherwise in what the person starts in. False, changing nothing, if the folder has no wardrobe. */
	bool Dress(USkeletalMeshComponent* InBody, const FString& Folder, const FString& Saved = FString());
	/** Takes the pieces off (the body mesh is left to the caller) */
	void Undress();
	bool HasWardrobe() const { return Body != nullptr; }

	/** What is worn in a slot, as shown to the player */
	FString PieceName(ENHOutfitSlot Slot) const;
	FString ColourName(ENHOutfitSlot Slot) const;
	/** The next piece or colour in a slot (Dir -1: the one before), going round */
	void Step(ENHOutfitSlot Slot, int32 Dir);
	void StepColour(ENHOutfitSlot Slot, int32 Dir);
	/** The choices as one line of text, for saving */
	FString Describe() const;
	/** A passer-by's clothes: pieces chosen by Seed, and the top in Shirt if given */
	void Pick(int32 Seed, const FLinearColor* Shirt = nullptr);
	/** Every piece's mesh component, for the owner to set shadows or visibility on */
	TArray<USkeletalMeshComponent*> GetParts() const;

private:
	struct FPiece
	{
		FString Name;
		FString Areas;
		FSoftObjectPath Mesh;
		bool bFull = false;
		bool bStart = false;
	};
	static constexpr int32 Slots = static_cast<int32>(ENHOutfitSlot::Count);
	TArray<FPiece> Pieces[Slots];
	/** Index into Pieces, -1 for nothing; and into the colour list, 0 for the piece's own colours */
	int32 Worn[Slots] = { -1, -1, -1, -1 };
	int32 Colour[Slots] = { 0, 0, 0, 0 };
	/** A colour of the owner's own instead of one from the list */
	bool bOwnColour[Slots] = { false, false, false, false };
	FLinearColor OwnColour[Slots];

	UPROPERTY() TObjectPtr<USkeletalMeshComponent> Body;
	UPROPERTY() TObjectPtr<USkeletalMeshComponent> Parts[4];
	UPROPERTY() TObjectPtr<UMaterialInterface> Hidden;

	const FPiece* PieceIn(int32 Slot) const;
	void Apply();
	void Read(const FString& Saved);
};
