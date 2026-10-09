#include "Characters/NHOutfitComponent.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/NHGameData.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "NaijaHustleGame.h"

namespace NHOutfit
{
	const TCHAR* SlotNames[] = { TEXT("Hair"), TEXT("Top"), TEXT("Bottom"), TEXT("Shoes") };
	struct FShade { const TCHAR* Name; FLinearColor Colour; };
	// the first leaves the piece as it was made; the others wash its texture to grey and colour that
	const FShade Shades[] = {
		{ TEXT("As made"), FLinearColor::White }, { TEXT("White"), FLinearColor(1.5f, 1.5f, 1.45f) }, { TEXT("Black"), FLinearColor(0.08f, 0.08f, 0.09f) },
		{ TEXT("Red"), FLinearColor(1.3f, 0.12f, 0.1f) }, { TEXT("Green"), FLinearColor(0.1f, 0.75f, 0.2f) }, { TEXT("Blue"), FLinearColor(0.12f, 0.3f, 1.3f) },
		{ TEXT("Yellow"), FLinearColor(1.5f, 1.1f, 0.08f) }, { TEXT("Orange"), FLinearColor(1.5f, 0.5f, 0.06f) }, { TEXT("Purple"), FLinearColor(0.6f, 0.15f, 1.f) },
		{ TEXT("Pink"), FLinearColor(1.5f, 0.4f, 0.75f) }, { TEXT("Brown"), FLinearColor(0.45f, 0.24f, 0.1f) }, { TEXT("Khaki"), FLinearColor(0.9f, 0.75f, 0.45f) } };

	FString Folder(const FString& Person) { return FString::Printf(TEXT("/Game/Characters/Player/%s/Wardrobe"), *Person); }

	/** "DenimSet" -> "Denim Set" */
	FString Spaced(const FString& Name)
	{
		FString Out;
		for (int32 I = 0; I < Name.Len(); ++I)
		{
			if (I > 0 && FChar::IsUpper(Name[I]) && !FChar::IsUpper(Name[I - 1]))
			{
				Out += TEXT(' ');
			}
			Out += Name[I];
		}
		return Out;
	}

	/** Over 12,000 triangles each (build_people_makehuman.py prints the counts) */
	const TArray<FString> Heavy = { TEXT("Cornrows"), TEXT("PrintTank"), TEXT("LongDress") };

	/** Data/wardrobe.json, written by import_wardrobe.py: for each person, the patches of skin (one letter each) every piece covers */
	const FString& Covers(const FString& Person, const FString& Piece)
	{
		static TMap<FString, FString> All;
		static bool bRead = false;
		if (!bRead)
		{
			bRead = true;
			FString Text;
			TSharedPtr<FJsonObject> Root;
			if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(UNHGameData::DataDir(), TEXT("wardrobe.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root.IsValid())
			{
				for (const auto& Who : Root->Values)
				{
					const TSharedPtr<FJsonObject>* Each = nullptr;
					if (Who.Value->TryGetObject(Each))
					{
						for (const auto& One : (*Each)->Values)
						{
							All.Add(FString(Who.Key) + TEXT("/") + FString(One.Key), One.Value->AsString());
						}
					}
				}
			}
		}
		static const FString None;
		const FString* Found = All.Find(Person + TEXT("/") + Piece);
		return Found ? *Found : None;
	}

	float Rand(int32 Seed, int32 K) { return FMath::Frac(FMath::Sin(Seed * 12.9898f + K * 78.233f) * 43758.5453f); }
}

const TArray<FString>& UNHOutfitComponent::People()
{
	static const TArray<FString> Names = { TEXT("Tunde"), TEXT("Emeka"), TEXT("Dayo"), TEXT("Mark"), TEXT("Chen"),
		TEXT("Amaka"), TEXT("Zainab"), TEXT("Ngozi"), TEXT("Kate"), TEXT("Mei"), TEXT("Priya") };
	return Names;
}

bool UNHOutfitComponent::Exists(const FString& Person)
{
	return FPackageName::DoesPackageExist(NHOutfit::Folder(Person) / (TEXT("Body_") + Person));
}

bool UNHOutfitComponent::Dress(USkeletalMeshComponent* InBody, const FString& Person, const FString& Saved)
{
	using namespace NHOutfit;
	if (!InBody || !Exists(Person))
	{
		return false;
	}
	USkeletalMesh* Bare = LoadObject<USkeletalMesh>(nullptr, *(Folder(Person) / FString::Printf(TEXT("Body_%s.Body_%s"), *Person, *Person)));
	if (!Bare)
	{
		return false;
	}
	Undress();
	Body = InBody;
	Body->SetSkeletalMesh(Bare);
	Hidden = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Characters/Player/M_NHPlayerHidden.M_NHPlayerHidden"));

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanPathsSynchronous({ Folder(Person) });
	TArray<FAssetData> Found;
	Registry.GetAssetsByPath(FName(*Folder(Person)), Found);
	Found.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
	for (int32 Slot = 0; Slot < Slots; ++Slot)
	{
		Pieces[Slot].Reset();
		Worn[Slot] = -1;
		Colour[Slot] = 0;
		bOwnColour[Slot] = false;
	}
	for (const FAssetData& Asset : Found)
	{
		TArray<FString> Bits;
		Asset.AssetName.ToString().ParseIntoArray(Bits, TEXT("_"));
		if (Bits.Num() < 2 || Bits[0] == TEXT("Body"))
		{
			continue;
		}
		FPiece Piece;
		Piece.Name = Bits[1];
		Piece.Areas = Covers(Person, Piece.Name);
		Piece.Mesh = Asset.GetSoftObjectPath();
		Piece.bFull = Bits[0] == TEXT("Full");
		Piece.bStart = Bits.Num() > 2 && Bits.Last() == TEXT("D");
		int32 Slot = Piece.bFull ? static_cast<int32>(ENHOutfitSlot::Top) : INDEX_NONE;
		for (int32 I = 0; Slot == INDEX_NONE && I < Slots; ++I)
		{
			if (Bits[0] == SlotNames[I])
			{
				Slot = I;
			}
		}
		if (Slot != INDEX_NONE)
		{
			if (Piece.bStart)
			{
				Worn[Slot] = Pieces[Slot].Num();
			}
			Pieces[Slot].Add(MoveTemp(Piece));
		}
	}
	Read(Saved);
	Apply();
	return true;
}

void UNHOutfitComponent::Undress()
{
	for (TObjectPtr<USkeletalMeshComponent>& Part : Parts)
	{
		if (Part)
		{
			Part->DestroyComponent();
			Part = nullptr;
		}
	}
	if (Body)
	{
		Body->EmptyOverrideMaterials();
	}
	Body = nullptr;
}

const UNHOutfitComponent::FPiece* UNHOutfitComponent::PieceIn(int32 Slot) const
{
	return Pieces[Slot].IsValidIndex(Worn[Slot]) ? &Pieces[Slot][Worn[Slot]] : nullptr;
}

void UNHOutfitComponent::Apply()
{
	using namespace NHOutfit;
	if (!Body)
	{
		return;
	}
	const int32 Top = static_cast<int32>(ENHOutfitSlot::Top), Bottom = static_cast<int32>(ENHOutfitSlot::Bottom);
	// nobody goes without a top, or without a bottom under a top that is not a full outfit
	if (!PieceIn(Top) && Pieces[Top].Num() > 0)
	{
		Worn[Top] = 0;
	}
	const bool bFull = PieceIn(Top) && PieceIn(Top)->bFull;
	if (!bFull && !PieceIn(Bottom) && Pieces[Bottom].Num() > 0)
	{
		Worn[Bottom] = 0;
	}
	FString Covered;
	for (int32 Slot = 0; Slot < Slots; ++Slot)
	{
		const FPiece* Piece = Slot == Bottom && bFull ? nullptr : PieceIn(Slot);
		USkeletalMesh* Mesh = Piece ? Cast<USkeletalMesh>(Piece->Mesh.TryLoad()) : nullptr;
		if (!Mesh)
		{
			if (Parts[Slot])
			{
				Parts[Slot]->SetSkeletalMesh(nullptr);
				Parts[Slot]->SetVisibility(false);
			}
			continue;
		}
		if (!Parts[Slot])
		{
			USkeletalMeshComponent* Part = NewObject<USkeletalMeshComponent>(GetOwner());
			Part->SetupAttachment(Body);
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Part->SetCanEverAffectNavigation(false);
			Part->bUseBoundsFromLeaderPoseComponent = true;
			Part->RegisterComponent();
			Part->SetLeaderPoseComponent(Body);
			Parts[Slot] = Part;
		}
		USkeletalMeshComponent* Part = Parts[Slot];
		Part->EmptyOverrideMaterials();
		Part->SetSkeletalMesh(Mesh);
		Part->SetVisibility(true);
		Covered += Piece->Areas;
		const bool bShade = bOwnColour[Slot] || Colour[Slot] > 0;
		if (bShade)
		{
			const FLinearColor Tint = bOwnColour[Slot] ? OwnColour[Slot] : Shades[Colour[Slot]].Colour;
			for (int32 I = 0; I < Part->GetNumMaterials(); ++I)
			{
				if (UMaterialInstanceDynamic* Dyed = Part->CreateDynamicMaterialInstance(I))
				{
					Dyed->SetVectorParameterValue(TEXT("Tint"), Tint);
					Dyed->SetScalarParameterValue(TEXT("Wash"), 1.f);
				}
			}
		}
	}
	// the body's skin is one slot per patch, NH_skin_<letter>: hide the ones under clothes
	const TArray<FName> SkinSlots = Body->GetMaterialSlotNames();
	for (int32 I = 0; I < SkinSlots.Num(); ++I)
	{
		const FString Name = SkinSlots[I].ToString();
		if (Name.StartsWith(TEXT("NH_skin_")) && Name.Len() == 9)
		{
			int32 At = INDEX_NONE;
			Body->SetMaterial(I, Hidden && Covered.FindChar(Name[8], At) ? Hidden.Get() : nullptr);
		}
	}
}

FString UNHOutfitComponent::PieceName(ENHOutfitSlot InSlot) const
{
	const int32 Slot = static_cast<int32>(InSlot);
	const FPiece* Top = PieceIn(static_cast<int32>(ENHOutfitSlot::Top));
	if (InSlot == ENHOutfitSlot::Bottom && Top && Top->bFull)
	{
		return TEXT("(part of the outfit)");
	}
	const FPiece* Piece = PieceIn(Slot);
	return Piece ? NHOutfit::Spaced(Piece->Name) : InSlot == ENHOutfitSlot::Hair ? TEXT("Shaved") : InSlot == ENHOutfitSlot::Shoes ? TEXT("Barefoot") : TEXT("(none)");
}

FString UNHOutfitComponent::ColourName(ENHOutfitSlot InSlot) const
{
	const int32 Slot = static_cast<int32>(InSlot);
	return bOwnColour[Slot] ? TEXT("Own") : NHOutfit::Shades[FMath::Clamp(Colour[Slot], 0, static_cast<int32>(UE_ARRAY_COUNT(NHOutfit::Shades)) - 1)].Name;
}

void UNHOutfitComponent::Step(ENHOutfitSlot InSlot, int32 Dir)
{
	const int32 Slot = static_cast<int32>(InSlot);
	// hair and shoes can also be nothing at all
	const bool bNone = InSlot == ENHOutfitSlot::Hair || InSlot == ENHOutfitSlot::Shoes;
	const int32 Count = Pieces[Slot].Num() + (bNone ? 1 : 0);
	if (!Body || Count == 0)
	{
		return;
	}
	const int32 Now = bNone ? Worn[Slot] + 1 : FMath::Max(Worn[Slot], 0);
	const int32 Next = ((Now + (Dir < 0 ? -1 : 1)) % Count + Count) % Count;
	Worn[Slot] = bNone ? Next - 1 : Next;
	Apply();
}

void UNHOutfitComponent::StepColour(ENHOutfitSlot InSlot, int32 Dir)
{
	const int32 Slot = static_cast<int32>(InSlot), Count = UE_ARRAY_COUNT(NHOutfit::Shades);
	bOwnColour[Slot] = false;
	Colour[Slot] = ((Colour[Slot] + (Dir < 0 ? -1 : 1)) % Count + Count) % Count;
	Apply();
}

FString UNHOutfitComponent::Describe() const
{
	FString Out;
	for (int32 Slot = 0; Slot < Slots; ++Slot)
	{
		const FPiece* Piece = PieceIn(Slot);
		Out += FString::Printf(TEXT("%s%s=%s:%d"), Slot ? TEXT(",") : TEXT(""), NHOutfit::SlotNames[Slot], Piece ? *Piece->Name : TEXT("-"), Colour[Slot]);
	}
	return Out;
}

void UNHOutfitComponent::Read(const FString& Saved)
{
	TArray<FString> Entries;
	Saved.ParseIntoArray(Entries, TEXT(","));
	for (const FString& Entry : Entries)
	{
		FString Key, Value, Name, Shade;
		if (!Entry.Split(TEXT("="), &Key, &Value) || !Value.Split(TEXT(":"), &Name, &Shade))
		{
			continue;
		}
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			if (Key == NHOutfit::SlotNames[Slot])
			{
				Worn[Slot] = Pieces[Slot].IndexOfByPredicate([&Name](const FPiece& P) { return P.Name == Name; });
				Colour[Slot] = FMath::Clamp(FCString::Atoi(*Shade), 0, static_cast<int32>(UE_ARRAY_COUNT(NHOutfit::Shades)) - 1);
			}
		}
	}
}

void UNHOutfitComponent::Pick(int32 Seed, const FLinearColor* Shirt)
{
	using namespace NHOutfit;
	for (int32 Slot = 0; Slot < Slots; ++Slot)
	{
		if (Pieces[Slot].Num() > 0)
		{
			Worn[Slot] = FMath::Min(static_cast<int32>(Rand(Seed, 11 + Slot) * Pieces[Slot].Num()), Pieces[Slot].Num() - 1);
			// the pieces with the most triangles are kept for the player: a crowd in them costs too much
			for (int32 Try = 0; Try < Pieces[Slot].Num() && Heavy.Contains(Pieces[Slot][Worn[Slot]].Name); ++Try)
			{
				Worn[Slot] = (Worn[Slot] + 1) % Pieces[Slot].Num();
			}
		}
		// most bottoms and shoes stay as made; a third take a colour from the list
		bOwnColour[Slot] = false;
		Colour[Slot] = Slot >= static_cast<int32>(ENHOutfitSlot::Bottom) && Rand(Seed, 21 + Slot) < 0.33f ? 1 + static_cast<int32>(Rand(Seed, 31 + Slot) * (UE_ARRAY_COUNT(Shades) - 1.01f)) : 0;
	}
	const int32 Top = static_cast<int32>(ENHOutfitSlot::Top);
	if (Shirt)
	{
		bOwnColour[Top] = true;
		OwnColour[Top] = *Shirt * 1.6f;
	}
	else if (Rand(Seed, 41) < 0.6f)
	{
		Colour[Top] = 1 + static_cast<int32>(Rand(Seed, 42) * (UE_ARRAY_COUNT(Shades) - 1.01f));
	}
	Apply();
}

bool UNHOutfitComponent::WearOneOf(ENHOutfitSlot Slot, int32 Seed, const TArray<FString>& Names)
{
	const int32 S = static_cast<int32>(Slot);
	TArray<int32> Have;
	for (int32 I = 0; I < Pieces[S].Num(); ++I)
	{
		if (Names.Contains(Pieces[S][I].Name))
		{
			Have.Add(I);
		}
	}
	if (Have.Num() == 0)
	{
		return false;
	}
	Worn[S] = Have[FMath::Abs(Seed) % Have.Num()];
	Apply();
	return true;
}

TArray<USkeletalMeshComponent*> UNHOutfitComponent::GetParts() const
{
	TArray<USkeletalMeshComponent*> Out;
	for (const TObjectPtr<USkeletalMeshComponent>& Part : Parts)
	{
		if (Part)
		{
			Out.Add(Part);
		}
	}
	return Out;
}
