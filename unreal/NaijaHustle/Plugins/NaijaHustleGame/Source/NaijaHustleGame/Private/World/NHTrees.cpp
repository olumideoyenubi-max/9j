#include "World/NHTrees.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Core/NHGameData.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "NaijaHustleGame.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

ANHTrees::ANHTrees()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ANHTrees::BeginPlay()
{
	Super::BeginPlay();
	const UNHGameData* Data = UNHGameData::Get(this);
	const double Started = FPlatformTime::Seconds();
	// the kinds there are, how often each is planted (of ten), and how much taller than the cone it replaces it stands
	struct FKind { const TCHAR* Path; int32 Share; float Taller; };
	static const FKind Kinds[] = { { TEXT("/Game/Foliage/Coconut_Palm/SM_Coconut_Palm"), 4, 1.7f }, { TEXT("/Game/Foliage/Shade_Tree/SM_Shade_Tree"), 6, 1.35f } };
	TArray<UHierarchicalInstancedStaticMeshComponent*> Beds;
	TArray<int32> Shares;
	TArray<float> Taller;
	float Near = 32000.f, Far = 46000.f; // -NHTreesFar=60000: how far off a tree is still drawn, cm
	FParse::Value(FCommandLine::Get(), TEXT("NHTreesFar="), Far);
	Near = Far * 0.7f;
	for (const FKind& Kind : Kinds)
	{
		UStaticMesh* Mesh = FPackageName::DoesPackageExist(Kind.Path) ? LoadObject<UStaticMesh>(nullptr, Kind.Path) : nullptr;
		if (!Mesh)
		{
			continue;
		}
		UHierarchicalInstancedStaticMeshComponent* Bed = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		Bed->SetStaticMesh(Mesh);
		Bed->SetMobility(EComponentMobility::Static);
		Bed->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Bed->SetCanEverAffectNavigation(false);
		Bed->SetCullDistances(static_cast<int32>(Near), static_cast<int32>(Far));
		Bed->SetCastShadow(true);
		Bed->bCastFarShadow = false;
		Bed->SetupAttachment(GetRootComponent());
		Bed->RegisterComponent();
		Beds.Add(Bed);
		Shares.Add(Kind.Share);
		Taller.Add(Kind.Taller);
	}
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("NaijaHustleGame"));
	FString Text;
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Flat = nullptr;
	if (!Data || !Data->bRealCity || Beds.Num() == 0 || !Plugin || !FFileHelper::LoadFileToString(Text, *(Plugin->GetBaseDir() / TEXT("Data/lagos_trees.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root || !Root->TryGetArrayField(TEXT("trees"), Flat))
	{
		SetActorTickEnabled(false);
		UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: trees: none planted (%d tree models in the project); the map's own stay"), Beds.Num());
		return;
	}
	int32 Whole = 0;
	for (const int32 Share : Shares)
	{
		Whole += Share;
	}
	TArray<TArray<FTransform>> Rows;
	Rows.SetNum(Beds.Num());
	for (int32 I = 0; I + 2 < Flat->Num(); I += 3)
	{
		const float X = static_cast<float>((*Flat)[I]->AsNumber()), Y = static_cast<float>((*Flat)[I + 1]->AsNumber()), Height = static_cast<float>((*Flat)[I + 2]->AsNumber());
		// the same tree on the same spot every time: its kind, its turn and its size come from where it stands
		const uint32 Seed = HashCombine(GetTypeHash(FMath::RoundToInt(X)), GetTypeHash(FMath::RoundToInt(Y)));
		int32 Pick = static_cast<int32>(Seed % Whole), Kind = 0;
		while (Kind < Shares.Num() - 1 && Pick >= Shares[Kind])
		{
			Pick -= Shares[Kind++];
		}
		const float Size = Height / 1000.f * Taller[Kind] * (0.85f + 0.3f * ((Seed >> 8) % 100) / 100.f); // the models are 10 m tall
		Rows[Kind].Add(FTransform(FRotator(0.f, static_cast<float>((Seed >> 16) % 360), 0.f), FVector(X, Y, -16.f), FVector(Size)));
	}
	for (int32 Kind = 0; Kind < Beds.Num(); ++Kind)
	{
		Beds[Kind]->AddInstances(Rows[Kind], false, true);
		Planted += Rows[Kind].Num();
	}
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: trees: %d planted of %d kinds, drawn out to %.0f m, in %.2f s"), Planted, Beds.Num(), Far / 100.f, FPlatformTime::Seconds() - Started);
}

void ANHTrees::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Look -= DeltaSeconds;
	if (Look > 0.f || Planted == 0)
	{
		return;
	}
	Look = 2.f;
	// the map's cones come in a square at a time as the player moves: each is hidden as it arrives
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		const UStaticMeshComponent* Piece = It->GetStaticMeshComponent();
		if (!It->IsHidden() && Piece && Piece->GetStaticMesh() && Piece->GetStaticMesh()->GetPathName().Contains(TEXT("/Tiles/Trees_Low/")))
		{
			It->SetActorHiddenInGame(true);
			++Hidden;
		}
	}
}
