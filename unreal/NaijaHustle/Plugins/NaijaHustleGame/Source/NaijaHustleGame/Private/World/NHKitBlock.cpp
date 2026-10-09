#include "World/NHKitBlock.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "NaijaHustleGame.h"

namespace
{
	const float Module = 300.f; // a panel's width and a storey's height, cm: the kit's grid
	const float RoofRise = 150.f; // the gable roof module's ridge height
	const int32 DataFloats = 7;

	// paint: what Lagos walls are painted, as tints of a pale plaster
	const FLinearColor WallColours[] = {
		{1.f, 0.93f, 0.75f}, {0.95f, 0.76f, 0.38f}, {0.95f, 0.64f, 0.54f}, {0.62f, 0.85f, 0.66f}, {0.56f, 0.76f, 0.95f}, {1.f, 1.f, 1.f},
		{0.72f, 0.7f, 0.66f}, {1.f, 0.82f, 0.62f}, {0.78f, 0.44f, 0.32f}, {0.9f, 0.88f, 0.6f}, {0.8f, 0.8f, 0.82f}, {0.7f, 0.86f, 0.86f}};
	// shutters, awnings, frames and railings
	const FLinearColor TrimColours[] = {
		{0.3f, 0.45f, 0.8f}, {0.3f, 0.6f, 0.4f}, {0.75f, 0.25f, 0.2f}, {0.95f, 0.95f, 0.95f}, {0.55f, 0.4f, 0.3f}, {0.7f, 0.7f, 0.7f}, {0.9f, 0.75f, 0.3f}};

	// small things: not drawn beyond 150 m and cast no shadow, which the frame rate needs
	const FName Details[] = {"AC_Unit", "Awning", "WaterTank", "Stairs", "Corner"};
	const FName Blocker("_Blocker"); // not a kit piece: an unseen box along each wall, which is all that collides

	float Cross(const FVector2D& A, const FVector2D& B) { return A.X * B.Y - A.Y * B.X; }

	bool Inside(const FVector2D& P, const FVector2D& A, const FVector2D& B, const FVector2D& C)
	{
		const float D1 = Cross(B - A, P - A), D2 = Cross(C - B, P - B), D3 = Cross(A - C, P - C);
		return !((D1 < 0 || D2 < 0 || D3 < 0) && (D1 > 0 || D2 > 0 || D3 > 0));
	}

	/** Cut an outline into triangles by clipping ears; Sign is the sign of its area */
	void Triangulate(const TArray<FVector2D>& P, float Sign, TArray<FIntVector>& Out)
	{
		TArray<int32> Left;
		for (int32 i = 0; i < P.Num(); ++i)
		{
			Left.Add(i);
		}
		int32 Guard = P.Num() * P.Num();
		while (Left.Num() > 3 && Guard-- > 0)
		{
			bool bCut = false;
			for (int32 i = 0; i < Left.Num() && !bCut; ++i)
			{
				const int32 A = Left[(i + Left.Num() - 1) % Left.Num()], B = Left[i], C = Left[(i + 1) % Left.Num()];
				if (Cross(P[B] - P[A], P[C] - P[B]) * Sign <= 0.f)
				{
					continue; // a corner that turns inward
				}
				bool bEmpty = true;
				for (const int32 Other : Left)
				{
					if (Other != A && Other != B && Other != C && Inside(P[Other], P[A], P[B], P[C]))
					{
						bEmpty = false;
						break;
					}
				}
				if (bEmpty)
				{
					Out.Add(FIntVector(A, B, C));
					Left.RemoveAt(i);
					bCut = true;
				}
			}
			if (!bCut)
			{
				break; // an outline that crosses itself: roof what was found
			}
		}
		if (Left.Num() == 3)
		{
			Out.Add(FIntVector(Left[0], Left[1], Left[2]));
		}
	}
}

ANHKitBlock::ANHKitBlock()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void ANHKitBlock::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void ANHKitBlock::BeginPlay()
{
	Super::BeginPlay();
	if (Parts.Num() == 0) // loaded from the level: nothing but the outlines was saved
	{
		Rebuild();
	}
}

void ANHKitBlock::Rebuild()
{
	for (UHierarchicalInstancedStaticMeshComponent* Part : Parts)
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	Parts.Reset();
	Instances = 0;

	TMap<FName, FBatch> Batches;
	for (const FNHKitBuilding& Building : Buildings)
	{
		Build(Building, Batches);
	}
	for (const TPair<FName, FBatch>& Pair : Batches)
	{
		const bool bBlocker = Pair.Key == Blocker;
		UStaticMesh* Mesh = bBlocker ? LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))
			: LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/NaijaHustle/Kit/SM_Kit_%s.SM_Kit_%s"), *Pair.Key.ToString(), *Pair.Key.ToString()));
		if (!Mesh)
		{
			UE_LOG(LogNHGame, Warning, TEXT("NHKitBlock: kit piece %s is missing (run Scripts/import_kit.py)"), *Pair.Key.ToString());
			continue;
		}
		// transient: the instances are worked out again on load, never saved
		UHierarchicalInstancedStaticMeshComponent* Part = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		Part->SetStaticMesh(Mesh);
		Part->SetMobility(EComponentMobility::Static);
		Part->NumCustomDataFloats = DataFloats;
		// tens of thousands of panels each with its own collision cost far too much memory: one box a wall does the job
		Part->SetCollisionProfileName(bBlocker ? TEXT("BlockAll") : TEXT("NoCollision"));
		if (bBlocker)
		{
			Part->SetHiddenInGame(true);
			Part->SetVisibility(false);
			Part->SetCastShadow(false);
		}
		if (Algo::Find(Details, Pair.Key))
		{
			Part->SetCullDistances(12000, 15000);
			Part->SetCastShadow(false);
		}
		Part->SetupAttachment(RootComponent);
		Part->RegisterComponent();
		Part->AddInstances(Pair.Value.Transforms, false, false, false);
		for (int32 i = 0; i < Pair.Value.Transforms.Num(); ++i)
		{
			Part->SetCustomData(i, MakeArrayView(&Pair.Value.Data[i * DataFloats], DataFloats), false);
		}
		Part->MarkRenderStateDirty();
		Parts.Add(Part);
		Instances += bBlocker ? 0 : Pair.Value.Transforms.Num();
	}
}

void ANHKitBlock::Build(const FNHKitBuilding& Building, TMap<FName, FBatch>& Out) const
{
	TArray<FVector2D> P = Building.Footprint;
	if (P.Num() >= 2 && P[0].Equals(P.Last(), 1.f))
	{
		P.Pop();
	}
	if (P.Num() < 3)
	{
		return;
	}
	float Area = 0.f;
	for (int32 i = 0; i < P.Num(); ++i)
	{
		Area += Cross(P[i], P[(i + 1) % P.Num()]);
	}
	int32 Front = Building.FrontEdge;
	// a panel's outside is to the left of its run (local +Y): go round the building so that left is out
	if (Area > 0.f)
	{
		Algo::Reverse(P);
		Front = Front >= 0 ? (2 * P.Num() - 2 - Front) % P.Num() : Front; // the same wall, counted the other way round
	}
	const int32 Num = P.Num();
	if (Front < 0 || Front >= Num)
	{
		float Longest = 0.f;
		for (int32 i = 0; i < Num; ++i)
		{
			const float Length = FVector2D::Distance(P[i], P[(i + 1) % Num]);
			if (Length > Longest)
			{
				Longest = Length;
				Front = i;
			}
		}
	}

	FRandomStream Rng(Building.Seed * 7919 + 17);
	const int32 Floors = FMath::Clamp(Building.Floors, 1, 12);
	const FLinearColor Wall = WallColours[Rng.RandRange(0, UE_ARRAY_COUNT(WallColours) - 1)];
	const FLinearColor Trim = TrimColours[Rng.RandRange(0, UE_ARRAY_COUNT(TrimColours) - 1)];
	const float Fade = Rng.FRandRange(0.f, 0.55f);
	const float Top = GroundZ + Floors * Module;

	auto Put = [&](FName Piece, const FVector& Where, float Yaw, const FVector& Scale = FVector::OneVector)
	{
		FBatch& Batch = Out.FindOrAdd(Piece);
		Batch.Transforms.Add(FTransform(FRotator(0.f, Yaw, 0.f), Where, Scale));
		Batch.Data.Append({Wall.R, Wall.G, Wall.B, Fade, Trim.R, Trim.G, Trim.B});
	};

	// what kind of building: these are fixed for the whole of it, so it reads as one design
	const int32 WindowRoll = Rng.RandRange(0, 99);
	const FName Window = WindowRoll < 45 ? FName("Wall_Window") : WindowRoll < 75 ? FName("Wall_WindowBars") : FName("Wall_Louvre");
	const FName GroundWindow = Window == FName("Wall_Window") ? FName("Wall_WindowBars") : Window; // nobody leaves a ground-floor window unbarred
	const bool bShops = Rng.FRand() < 0.55f;
	const bool bBalconies = Floors >= 2 && Rng.FRand() < 0.6f;
	const int32 BalconyPhase = Rng.RandRange(0, 1);
	int32 StairEdge = -1;
	if (Floors >= 2 && Rng.FRand() < 0.3f)
	{
		for (int32 i = 1; i < Num && StairEdge < 0; ++i)
		{
			const int32 Edge = (Front + i) % Num;
			if (FVector2D::Distance(P[Edge], P[(Edge + 1) % Num]) >= 620.f)
			{
				StairEdge = Edge;
			}
		}
	}

	for (int32 Edge = 0; Edge < Num; ++Edge)
	{
		const FVector2D A = P[Edge], B = P[(Edge + 1) % Num];
		const float Length = FVector2D::Distance(A, B);
		if (Length < 60.f)
		{
			continue;
		}
		const FVector2D Dir = (B - A) / Length;
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
		const int32 Count = FMath::Max(1, FMath::RoundToInt(Length / Module));
		const float Stretch = Length / (Count * Module);
		const FVector Scale(Stretch, 1.f, 1.f);
		const bool bFront = Edge == Front;
		Put("Corner", FVector(A.X, A.Y, GroundZ), Yaw, FVector(1.f, 1.f, Floors));
		// the wall's collision: a 20 cm box its full length and height, just inside the wall line (the cube is 100 cm, centred)
		const FVector2D Middle = (A + B) * 0.5f - FVector2D(-Dir.Y, Dir.X) * 10.f;
		Put(Blocker, FVector(Middle.X, Middle.Y, GroundZ + Floors * Module * 0.5f), Yaw, FVector(Length / 100.f, 0.2f, Floors * Module / 100.f));

		for (int32 i = 0; i < Count; ++i)
		{
			const FVector2D At = A + Dir * (i * Module * Stretch);
			// one choice a column for the upper storeys: windows line up above one another
			const float ColumnRoll = Rng.FRand(), ExtraRoll = Rng.FRand();
			FName Upper = ColumnRoll < 0.82f ? Window : FName("Wall_Plain");
			if (bFront && bBalconies && i % 2 == BalconyPhase && ColumnRoll < 0.75f)
			{
				Upper = "Wall_Balcony";
			}
			FName Ground = ColumnRoll < 0.6f ? GroundWindow : FName("Wall_Plain");
			if (bFront && bShops)
			{
				Ground = ExtraRoll < 0.62f ? "Shop_Shutter" : "Shop_Open";
			}
			else if (bFront && i == Count / 2)
			{
				Ground = "Wall_Door";
			}
			if (Length < 150.f)
			{
				Ground = Upper = "Wall_Plain"; // too narrow for an opening
			}
			for (int32 Floor = 0; Floor < Floors; ++Floor)
			{
				const FVector Where(At.X, At.Y, GroundZ + Floor * Module);
				Put(Floor == 0 ? Ground : Upper, Where, Yaw, Scale);
				if (Floor == 0 && bFront && bShops && ExtraRoll > 0.7f)
				{
					Put("Awning", Where, Yaw, Scale);
				}
				if (Floor > 0 && Upper == Window && Rng.FRand() < 0.14f)
				{
					const FVector2D Under = At + Dir * (215.f * Stretch);
					Put("AC_Unit", FVector(Under.X, Under.Y, Where.Z + 26.f), Yaw);
				}
			}
		}
		if (Edge == StairEdge)
		{
			const FVector2D At = A + Dir * 20.f;
			Put("Stairs", FVector(At.X, At.Y, GroundZ), Yaw);
		}
	}

	// the roof. Measure the building along its front wall and across it
	const FVector2D U = (P[(Front + 1) % Num] - P[Front]).GetSafeNormal(), V(-U.Y, U.X);
	float U0 = FLT_MAX, U1 = -FLT_MAX, V0 = FLT_MAX, V1 = -FLT_MAX;
	for (const FVector2D& Point : P)
	{
		U0 = FMath::Min(U0, Point | U); U1 = FMath::Max(U1, Point | U);
		V0 = FMath::Min(V0, Point | V); V1 = FMath::Max(V1, Point | V);
	}
	const float Wide = U1 - U0, Deep = V1 - V0;
	const bool bSquare = Num == 4 && FMath::Abs(Area) * 0.5f > 0.9f * Wide * Deep;
	const bool bGable = bSquare && Floors <= 2 && FMath::Min(Wide, Deep) <= 1400.f && Rng.FRand() < 0.75f;
	if (bGable)
	{
		// zinc, the ridge along the longer way, eaves 40 cm past the walls
		const bool bAlongU = Wide >= Deep;
		const FVector2D Ridge = bAlongU ? U : V, AcrossDir = bAlongU ? V : U;
		const float R0 = (bAlongU ? U0 : V0) - 30.f, R1 = (bAlongU ? U1 : V1) + 30.f;
		const float Mid = bAlongU ? (V0 + V1) * 0.5f : (U0 + U1) * 0.5f;
		const float WallSpan = bAlongU ? Deep : Wide, Span = WallSpan + 80.f;
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Ridge.Y, Ridge.X));
		const int32 Count = FMath::Max(1, FMath::RoundToInt((R1 - R0) / Module));
		const float Stretch = (R1 - R0) / (Count * Module);
		const float Z = Top - RoofRise * 80.f / Span; // so the slope meets the top of the wall, not the eaves
		for (int32 i = 0; i < Count; ++i)
		{
			const FVector2D At = Ridge * (R0 + i * Module * Stretch) + AcrossDir * Mid;
			Put("Roof_Gable", FVector(At.X, At.Y, Z), Yaw, FVector(Stretch, Span / 100.f, 1.f));
		}
		for (const float End : {R0 + 30.f, R1 - 30.f})
		{
			const FVector2D At = Ridge * End + AcrossDir * Mid;
			Put("Roof_GableEnd", FVector(At.X, At.Y, Top), Yaw, FVector(1.f, WallSpan / 100.f, WallSpan / Span));
		}
	}
	else
	{
		// flat, behind a parapet: laid in right-angled triangles so it fits any outline
		TArray<FIntVector> Triangles;
		Triangulate(P, -1.f, Triangles);
		FVector2D TankAt = P[0];
		float Largest = 0.f;
		for (const FIntVector& T : Triangles)
		{
			FVector2D Corner[3] = {P[T.X], P[T.Y], P[T.Z]};
			int32 Long = 0; // the longest side runs from Corner[Long] to the next
			for (int32 i = 1; i < 3; ++i)
			{
				if (FVector2D::DistSquared(Corner[i], Corner[(i + 1) % 3]) > FVector2D::DistSquared(Corner[Long], Corner[(Long + 1) % 3]))
				{
					Long = i;
				}
			}
			const FVector2D S0 = Corner[Long], S1 = Corner[(Long + 1) % 3], Apex = Corner[(Long + 2) % 3];
			const FVector2D Foot = S0 + (S1 - S0) * (((Apex - S0) | (S1 - S0)) / FMath::Max(1.f, (S1 - S0).SizeSquared()));
			const float Height = FVector2D::Distance(Apex, Foot);
			for (const FVector2D& End : {S0, S1})
			{
				const float Leg = FVector2D::Distance(End, Foot);
				if (Leg < 1.f || Height < 1.f)
				{
					continue;
				}
				const FVector2D X = (End - Foot) / Leg;
				const float Side = (FVector2D(-X.Y, X.X) | (Apex - Foot)) >= 0.f ? 1.f : -1.f;
				Put("Roof_Tri", FVector(Foot.X, Foot.Y, Top - 10.f), FMath::RadiansToDegrees(FMath::Atan2(X.Y, X.X)), FVector(Leg / 100.f, Side * Height / 100.f, 1.f));
			}
			const float Size = FMath::Abs(Cross(S1 - S0, Apex - S0));
			if (Size > Largest)
			{
				Largest = Size;
				TankAt = (S0 + S1 + Apex) / 3.f;
			}
		}
		for (int32 Edge = 0; Edge < Num; ++Edge)
		{
			const FVector2D A = P[Edge], B = P[(Edge + 1) % Num];
			const float Length = FVector2D::Distance(A, B);
			if (Length < 60.f)
			{
				continue;
			}
			const FVector2D Dir = (B - A) / Length;
			const int32 Count = FMath::Max(1, FMath::RoundToInt(Length / Module));
			const float Stretch = Length / (Count * Module);
			for (int32 i = 0; i < Count; ++i)
			{
				const FVector2D At = A + Dir * (i * Module * Stretch);
				Put("Parapet", FVector(At.X, At.Y, Top), FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), FVector(Stretch, 1.f, 1.f));
			}
		}
		if (Largest > 300.f * 300.f && Rng.FRand() < 0.55f)
		{
			Put("WaterTank", FVector(TankAt.X, TankAt.Y, Top - 10.f), Rng.FRandRange(0.f, 360.f));
		}
	}
}
