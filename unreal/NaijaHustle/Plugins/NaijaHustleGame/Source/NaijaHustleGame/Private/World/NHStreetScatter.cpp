#include "World/NHStreetScatter.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "NaijaHustleGame.h"
#include "World/NHStreets.h"

namespace
{
	const float TileSize = 10000.f; // 100 m
	const float Step = 400.f; // a look at each side of the street every 4 m
	const int32 PoleEvery = 10; // steps: a pole every 40 m
	const float WireHeight = 800.f, WireSpan = 1000.f; // as modelled in build_props.py
	const float Land = -16.f; // the land's height in the level, where nothing is found under a prop

	const TCHAR* Names[] = {TEXT("Chair"), TEXT("Drum"), TEXT("Generator"), TEXT("Kiosk"), TEXT("UmbrellaStall"), TEXT("Pole"), TEXT("Wire"), TEXT("Weeds"), TEXT("TrashBag"), TEXT("Litter"), TEXT("Rubble")};
	const FName Solid[] = {"Kiosk", "Drum", "Generator"}; // these block; the rest you pass through
	const FName Small[] = {"Weeds", "Litter", "TrashBag", "Rubble", "Chair"}; // not drawn beyond 90 m, no shadow

	const FLinearColor ChairColours[] = {{0.9f, 0.9f, 0.88f}, {0.9f, 0.9f, 0.88f}, {0.1f, 0.2f, 0.6f}, {0.1f, 0.4f, 0.2f}, {0.6f, 0.08f, 0.08f}, {0.35f, 0.05f, 0.1f}};
	const FLinearColor DrumColours[] = {{0.05f, 0.15f, 0.5f}, {0.5f, 0.07f, 0.05f}, {0.25f, 0.13f, 0.07f}, {0.03f, 0.03f, 0.03f}, {0.6f, 0.5f, 0.05f}};
	const FLinearColor KioskColours[] = {{0.1f, 0.35f, 0.65f}, {0.65f, 0.1f, 0.08f}, {0.1f, 0.45f, 0.2f}, {0.8f, 0.65f, 0.1f}, {0.75f, 0.75f, 0.7f}, {0.85f, 0.4f, 0.05f}};
	const FLinearColor BagColours[] = {{0.02f, 0.02f, 0.02f}, {0.02f, 0.02f, 0.02f}, {0.02f, 0.02f, 0.02f}, {0.8f, 0.8f, 0.8f}, {0.05f, 0.15f, 0.5f}};
	const FLinearColor GeneratorColours[] = {{0.6f, 0.06f, 0.05f}, {0.7f, 0.55f, 0.05f}, {0.05f, 0.2f, 0.55f}, {0.08f, 0.35f, 0.15f}};

	template <int32 N> const FLinearColor& Pick(const FLinearColor (&Colours)[N], FRandomStream& Rng) { return Colours[Rng.RandRange(0, N - 1)]; }

	float Yaw(const FVector2D& Dir) { return FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)); }

	/** The point Distance cm along a line of points, and which way the line runs there */
	bool Along(const TArray<FVector2D>& Points, float Distance, FVector2D& OutAt, FVector2D& OutDir)
	{
		for (int32 i = 0; i + 1 < Points.Num(); ++i)
		{
			const float Length = FVector2D::Distance(Points[i], Points[i + 1]);
			if (Distance <= Length && Length > 1.f)
			{
				OutDir = (Points[i + 1] - Points[i]) / Length;
				OutAt = Points[i] + OutDir * Distance;
				return true;
			}
			Distance -= Length;
		}
		return false;
	}
}

ANHStreetScatter::ANHStreetScatter()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ANHStreetScatter::BeginPlay()
{
	Super::BeginPlay();
	for (const TCHAR* Name : Names)
	{
		if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/NaijaHustle/Props/SM_Prop_%s.SM_Prop_%s"), Name, Name)))
		{
			Meshes.Add(Name, Mesh);
		}
	}
	if (Meshes.Num() < UE_ARRAY_COUNT(Names))
	{
		UE_LOG(LogNHGame, Warning, TEXT("NHStreetScatter: %d of %d props found (run Scripts/import_kit.py with NH_KIT_SET=props)"), Meshes.Num(), (int32)UE_ARRAY_COUNT(Names));
	}
}

void ANHStreetScatter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if ((Think -= DeltaSeconds) > 0.f)
	{
		return;
	}
	Think = 0.2f;
	if (!Streets.IsValid())
	{
		Streets = ANHStreets::Get(this);
	}
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || !Streets.IsValid() || Meshes.Num() == 0)
	{
		return;
	}
	const FVector2D Here(Pawn->GetActorLocation());
	auto Centre = [](const FIntPoint& Tile) { return FVector2D((Tile.X + 0.5f) * TileSize, (Tile.Y + 0.5f) * TileSize); };

	for (auto It = Tiles.CreateIterator(); It; ++It)
	{
		if (FVector2D::Distance(Centre(It.Key()), Here) > Reach + Slack)
		{
			Empty(It.Key());
			It.RemoveCurrent();
		}
	}
	// the nearest empty tile within reach: one a step
	FIntPoint Best(0, 0);
	float BestDistance = Reach;
	const int32 Span = FMath::CeilToInt(Reach / TileSize);
	const FIntPoint Mid(FMath::FloorToInt(Here.X / TileSize), FMath::FloorToInt(Here.Y / TileSize));
	for (int32 X = Mid.X - Span; X <= Mid.X + Span; ++X)
	{
		for (int32 Y = Mid.Y - Span; Y <= Mid.Y + Span; ++Y)
		{
			const float Distance = FVector2D::Distance(Centre(FIntPoint(X, Y)), Here);
			if (Distance < BestDistance && !Tiles.Contains(FIntPoint(X, Y)))
			{
				BestDistance = Distance;
				Best = FIntPoint(X, Y);
			}
		}
	}
	if (BestDistance < Reach)
	{
		Fill(Best);
	}
	else if (!bSaid && Tiles.Num() > 0)
	{
		bSaid = true; // once, when the first surroundings are complete: for the log and the tests
		TMap<FString, int32> Count;
		for (const UInstancedStaticMeshComponent* Part : Parts)
		{
			Count.FindOrAdd(Part->GetStaticMesh()->GetName().RightChop(8)) += Part->GetInstanceCount(); // without "SM_Prop_"
		}
		FString List;
		for (const TPair<FString, int32>& Pair : Count)
		{
			List += FString::Printf(TEXT("%s%s %d"), List.IsEmpty() ? TEXT("") : TEXT(", "), *Pair.Key, Pair.Value);
		}
		UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: scatter: %d props in %d tiles round the player (%s)"), Props, Tiles.Num(), *List);
	}
}

bool ANHStreetScatter::Place(TMap<FName, FBatch>& Out, FName Prop, const FVector2D& At, float InYaw, float Scale, const FLinearColor& Colour, float Room) const
{
	// a ray down from above: it finds the ground, and anything standing here already
	FHitResult Hit;
	float Z = Land;
	if (GetWorld()->LineTraceSingleByChannel(Hit, FVector(At.X, At.Y, 350.f), FVector(At.X, At.Y, -400.f), ECC_WorldStatic))
	{
		if (Hit.bStartPenetrating || Hit.ImpactPoint.Z > 120.f || Hit.ImpactNormal.Z < 0.8f)
		{
			return false; // inside a building, on a roof or a car, or on a slope
		}
		Z = Hit.ImpactPoint.Z;
	}
	if (Room > 0.f && GetWorld()->OverlapBlockingTestByChannel(FVector(At.X, At.Y, Z + 140.f), FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeBox(FVector(Room, Room, 100.f))))
	{
		return false;
	}
	FBatch& Batch = Out.FindOrAdd(Prop);
	Batch.Transforms.Add(FTransform(FRotator(0.f, InYaw, 0.f), FVector(At.X, At.Y, Z), FVector(Scale)));
	Batch.Colours.Append({Colour.R, Colour.G, Colour.B});
	return true;
}

void ANHStreetScatter::Fill(const FIntPoint& Tile)
{
	const FVector2D Min(Tile.X * TileSize, Tile.Y * TileSize), Max = Min + FVector2D(TileSize);
	TArray<int32> Near;
	Streets->StreetsIn(Min - FVector2D(2000.f), Max + FVector2D(2000.f), Near);
	TMap<FName, FBatch> Batches;
	const FBox2D Box(Min, Max);

	// how near the tar of any other street a spot is: props keep clear of junctions
	auto OnAnother = [&](const FVector2D& At, int32 Own)
	{
		for (const int32 Other : Near)
		{
			if (Other == Own)
			{
				continue;
			}
			const TArray<FVector2D>& Points = Streets->StreetPoints(Other);
			const float Clear = Streets->StreetWidth(Other) * 0.5f + 120.f;
			for (int32 i = 0; i + 1 < Points.Num(); ++i)
			{
				if (FMath::PointDistToSegmentSquared(FVector(At, 0.f), FVector(Points[i], 0.f), FVector(Points[i + 1], 0.f)) < Clear * Clear)
				{
					return true;
				}
			}
		}
		return false;
	};

	for (const int32 Index : Near)
	{
		const TArray<FVector2D>& Points = Streets->StreetPoints(Index);
		const float Half = Streets->StreetWidth(Index) * 0.5f;
		const int32 Rank = Streets->StreetRank(Index);
		const bool bSideStreet = Rank >= 4; // traders and rubbish gather on the smaller streets, not the expressway
		FVector2D At, Dir;
		for (int32 K = 0; Along(Points, K * Step, At, Dir); ++K)
		{
			const FVector2D Out(-Dir.Y, Dir.X);
			for (int32 Side = -1; Side <= 1; Side += 2)
			{
				const FVector2D Edge = At + Out * (Side * Half);
				if (!Box.IsInside(Edge))
				{
					continue; // another tile's
				}
				// the same street, step and side give the same clutter every time
				FRandomStream Rng(HashCombine(HashCombine(GetTypeHash(Points[0]), GetTypeHash(Points.Last())), K * 2 + (Side > 0 ? 1 : 0)));
				const FVector2D Away = Out * Side;
				const float Facing = Yaw(-Away) - 90.f; // a prop's front is +Y: turn it to the road
				if (Side > 0 && K % PoleEvery == 0 && Rank <= 8)
				{
					const FVector2D PoleAt = Edge + Away * 90.f;
					FVector2D NextAt, NextDir;
					if (!OnAnother(PoleAt, Index) && Place(Batches, "Pole", PoleAt, Yaw(Dir), 1.f, FLinearColor::White)
						&& Along(Points, (K + PoleEvery) * Step, NextAt, NextDir))
					{
						const FVector2D Next = NextAt + FVector2D(-NextDir.Y, NextDir.X) * (Half + 90.f);
						FBatch& Wires = Batches.FindOrAdd("Wire");
						const FVector Base = Batches["Pole"].Transforms.Last().GetLocation();
						Wires.Transforms.Add(FTransform(FRotator(0.f, Yaw(Next - PoleAt), 0.f), Base + FVector(0.f, 0.f, WireHeight), FVector(FVector2D::Distance(Next, PoleAt) / WireSpan, 1.f, 1.f)));
						Wires.Colours.Append({1.f, 1.f, 1.f});
					}
				}
				if (OnAnother(Edge + Away * 60.f, Index))
				{
					continue;
				}
				if (Rng.FRand() < 0.45f * Density)
				{
					for (int32 i = Rng.RandRange(1, 3); i > 0; --i)
					{
						Place(Batches, "Weeds", Edge + Away * Rng.FRandRange(-15.f, 45.f) + Dir * Rng.FRandRange(-180.f, 180.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.7f, 1.7f), FLinearColor::White);
					}
				}
				if (Rng.FRand() < 0.22f * Density)
				{
					Place(Batches, "Litter", Edge - Away * Rng.FRandRange(0.f, 110.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.8f, 1.3f), Pick(KioskColours, Rng));
				}
				if (!bSideStreet)
				{
					continue;
				}
				const float Roll = Rng.FRand() / FMath::Max(0.01f, Density);
				if (Roll < 0.035f)
				{
					const FVector2D Spot = Edge + Away * 150.f;
					if (Place(Batches, "Kiosk", Spot, Facing, 1.f, Pick(KioskColours, Rng), 110.f))
					{
						const FLinearColor Seat = Pick(ChairColours, Rng);
						for (int32 i = Rng.RandRange(0, 2); i > 0; --i)
						{
							Place(Batches, "Chair", Spot + Dir * (i % 2 ? 170.f : -175.f) - Away * Rng.FRandRange(20.f, 70.f), Facing + Rng.FRandRange(-50.f, 50.f), 1.f, Seat);
						}
						if (Rng.FRand() < 0.4f)
						{
							Place(Batches, "Generator", Spot + Dir * 165.f + Away * 40.f, Facing + 90.f, 1.f, Pick(GeneratorColours, Rng));
						}
					}
				}
				else if (Roll < 0.065f)
				{
					const FVector2D Spot = Edge + Away * 150.f;
					if (Place(Batches, "UmbrellaStall", Spot, Facing + Rng.FRandRange(-15.f, 15.f), 1.f, Pick(KioskColours, Rng), 100.f) && Rng.FRand() < 0.7f)
					{
						Place(Batches, "Chair", Spot + Away * 70.f + Dir * 40.f, Facing + Rng.FRandRange(-30.f, 30.f), 1.f, Pick(ChairColours, Rng));
					}
				}
				else if (Roll < 0.1f)
				{
					for (int32 i = Rng.RandRange(2, 4); i > 0; --i)
					{
						Place(Batches, "TrashBag", Edge + Away * Rng.FRandRange(30.f, 100.f) + Dir * Rng.FRandRange(-70.f, 70.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.8f, 1.25f), Pick(BagColours, Rng));
					}
					Place(Batches, "Litter", Edge + Away * 40.f, Rng.FRandRange(0.f, 360.f), 1.3f, Pick(KioskColours, Rng));
				}
				else if (Roll < 0.125f)
				{
					Place(Batches, "Rubble", Edge + Away * 90.f, Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.8f, 1.4f), FLinearColor::White);
				}
				else if (Roll < 0.15f)
				{
					const FLinearColor Colour = Pick(DrumColours, Rng);
					for (int32 i = Rng.RandRange(1, 2); i > 0; --i)
					{
						Place(Batches, "Drum", Edge + Away * 80.f + Dir * (i * 66.f), Rng.FRandRange(0.f, 360.f), 1.f, Colour, 30.f);
					}
				}
			}
		}
	}

	TArray<TWeakObjectPtr<UInstancedStaticMeshComponent>>& Mine = Tiles.Add(Tile);
	for (const TPair<FName, FBatch>& Pair : Batches)
	{
		UStaticMesh* Mesh = Meshes.FindRef(Pair.Key);
		if (!Mesh || Pair.Value.Transforms.Num() == 0)
		{
			continue;
		}
		UInstancedStaticMeshComponent* Part = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		Part->SetStaticMesh(Mesh);
		Part->NumCustomDataFloats = 3;
		Part->SetCollisionProfileName(Algo::Find(Solid, Pair.Key) ? TEXT("BlockAll") : TEXT("NoCollision"));
		const bool bSmall = Algo::Find(Small, Pair.Key) != nullptr;
		if (bSmall || Pair.Key == FName("Wire"))
		{
			Part->SetCastShadow(false);
		}
		if (bSmall)
		{
			Part->SetCullDistances(7000, 9000);
		}
		Part->SetupAttachment(RootComponent);
		Part->RegisterComponent();
		Part->AddInstances(Pair.Value.Transforms, false, true, false);
		for (int32 i = 0; i < Pair.Value.Transforms.Num(); ++i)
		{
			Part->SetCustomData(i, MakeArrayView(&Pair.Value.Colours[i * 3], 3), false);
		}
		Part->MarkRenderStateDirty();
		Parts.Add(Part);
		Mine.Add(Part);
		Props += Pair.Value.Transforms.Num();
	}
}

void ANHStreetScatter::Empty(const FIntPoint& Tile)
{
	if (const TArray<TWeakObjectPtr<UInstancedStaticMeshComponent>>* Mine = Tiles.Find(Tile))
	{
		for (const TWeakObjectPtr<UInstancedStaticMeshComponent>& Part : *Mine)
		{
			if (Part.IsValid())
			{
				Props -= Part->GetInstanceCount();
				Parts.Remove(Part.Get());
				Part->DestroyComponent();
			}
		}
	}
}
