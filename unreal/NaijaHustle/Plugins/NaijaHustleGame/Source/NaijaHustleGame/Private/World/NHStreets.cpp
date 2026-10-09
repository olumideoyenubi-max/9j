#include "World/NHStreets.h"

#include "Components/TextRenderComponent.h"
#include "Core/NHGameData.h"
#include "Dom/JsonObject.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NaijaHustleGame.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "World/NHShapes.h"

namespace NHStreetData
{
	const float Cell = 20000.f; // 200 m
	const TCHAR* Classes[] = { TEXT("motorway"), TEXT("trunk"), TEXT("primary"), TEXT("secondary"), TEXT("tertiary"), TEXT("unclassified"), TEXT("residential"), TEXT("living_street"), TEXT("service"), TEXT("track") };
	FIntPoint CellOf(const FVector2D& P) { return FIntPoint(FMath::FloorToInt(P.X / Cell), FMath::FloorToInt(P.Y / Cell)); }
	const FLinearColor SignGreen(0.02f, 0.22f, 0.09f);
}

ANHStreets::ANHStreets()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ANHStreets* ANHStreets::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	TActorIterator<ANHStreets> It(World);
	return World && It ? *It : nullptr;
}

void ANHStreets::BeginPlay()
{
	Super::BeginPlay();
	// every district that has been built: Data/osm_<district>.json
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(UNHGameData::DataDir() / TEXT("osm_*.json")), true, false);
	int32 Districts = 0;
	for (const FString& File : Files)
	{
		Districts += LoadDistrict(UNHGameData::DataDir() / File) ? 1 : 0;
	}
	int32 Named = 0;
	TSet<FString> Names;
	for (const FStreet& S : Streets)
	{
		if (!S.Name.IsEmpty())
		{
			++Named;
			Names.Add(S.Name);
		}
	}
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: streets: %d districts, %d street segments (%d named, %d different names), %d junctions with a name"), Districts, Streets.Num(), Named, Names.Num(), Junctions.Num());
	DrawHandle = AHUD::OnHUDPostRender.AddUObject(this, &ANHStreets::DrawBanner);
}

void ANHStreets::EndPlay(const EEndPlayReason::Type Reason)
{
	AHUD::OnHUDPostRender.Remove(DrawHandle);
	Super::EndPlay(Reason);
}

bool ANHStreets::LoadDistrict(const FString& Path)
{
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		return false;
	}
	const auto Point = [](const TSharedPtr<FJsonValue>& V)
	{
		const TArray<TSharedPtr<FJsonValue>>& P = V->AsArray();
		return P.Num() >= 2 ? FVector2D(P[0]->AsNumber(), P[1]->AsNumber()) : FVector2D::ZeroVector;
	};
	const TSharedPtr<FJsonObject>* All = nullptr;
	if (Root->TryGetObjectField(TEXT("streets"), All))
	{
		for (const auto& Pair : (*All)->Values)
		{
			const TSharedPtr<FJsonObject> O = Pair.Value->AsObject();
			FStreet S;
			S.Name = O->GetStringField(TEXT("name"));
			const FString Class = O->GetStringField(TEXT("class"));
			for (int32 I = 0; I < UE_ARRAY_COUNT(NHStreetData::Classes); ++I)
			{
				if (Class == NHStreetData::Classes[I])
				{
					S.Rank = static_cast<uint8>(I);
				}
			}
			S.Width = static_cast<float>(O->GetNumberField(TEXT("width"))) * 100.f;
			for (const TSharedPtr<FJsonValue>& V : O->GetArrayField(TEXT("pts")))
			{
				S.Points.Add(Point(V));
			}
			if (S.Points.Num() < 2)
			{
				continue;
			}
			const int32 Index = Streets.Add(MoveTemp(S));
			const FStreet& Street = Streets[Index];
			for (int32 I = 0; I + 1 < Street.Points.Num(); ++I)
			{
				// long segments are few in a street grid: the cells of both ends and the middle are enough to find them
				for (const FVector2D& P : { Street.Points[I], (Street.Points[I] + Street.Points[I + 1]) * 0.5f, Street.Points[I + 1] })
				{
					Cells.FindOrAdd(NHStreetData::CellOf(P)).AddUnique(FIntPoint(Index, I));
				}
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Root->TryGetArrayField(TEXT("junctions"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FJunction J;
			J.At = Point(O->GetField<EJson::Array>(TEXT("at")));
			for (const TSharedPtr<FJsonValue>& N : O->GetArrayField(TEXT("streets")))
			{
				J.Names.Add(N->AsString());
			}
			if (J.Names.Num() > 0)
			{
				JunctionCells.FindOrAdd(NHStreetData::CellOf(J.At)).Add(Junctions.Add(MoveTemp(J)));
			}
		}
	}
	return true;
}

const ANHStreets::FStreet* ANHStreets::Nearest(const FVector2D& At, float MaxDistance, bool bNamedOnly, FVector2D* OutDir) const
{
	const FStreet* Best = nullptr;
	float BestDist = MaxDistance * MaxDistance;
	const FIntPoint Mid = NHStreetData::CellOf(At);
	const int32 Reach = FMath::Max(1, FMath::CeilToInt(MaxDistance / NHStreetData::Cell));
	for (int32 CX = Mid.X - Reach; CX <= Mid.X + Reach; ++CX)
	{
		for (int32 CY = Mid.Y - Reach; CY <= Mid.Y + Reach; ++CY)
		{
			if (const TArray<FIntPoint>* Cell = Cells.Find(FIntPoint(CX, CY)))
			{
				for (const FIntPoint& Seg : *Cell)
				{
					const FStreet& S = Streets[Seg.X];
					if (bNamedOnly && S.Name.IsEmpty())
					{
						continue;
					}
					const FVector2D A = S.Points[Seg.Y], B = S.Points[Seg.Y + 1];
					const float Dist = FMath::PointDistToSegmentSquared(FVector(At, 0.f), FVector(A, 0.f), FVector(B, 0.f));
					if (Dist < BestDist)
					{
						BestDist = Dist;
						Best = &S;
						if (OutDir)
						{
							*OutDir = (B - A).GetSafeNormal();
						}
					}
				}
			}
		}
	}
	return Best;
}

FString ANHStreets::StreetAt(const FVector2D& At, float MaxDistance) const
{
	if (const FStreet* S = Nearest(At, MaxDistance, true))
	{
		return S->Name;
	}
	// no district data here: the main road's name, if one is close
	const UNHGameData* Data = UNHGameData::Get(this);
	FNHRoadSeg Seg;
	FVector2D Point;
	if (Data && Data->bRealCity && Data->NearestRoad(At, Seg, Point) && FVector2D::Distance(Point, At) <= MaxDistance)
	{
		return Data->RoadWays[Seg.Way].Name;
	}
	return FString();
}

FString ANHStreets::PlaceName(const FVector2D& At) const
{
	const UNHGameData* Data = UNHGameData::Get(this);
	const FString Street = StreetAt(At), District = Data ? Data->DistrictAt(FVector(At, 0.f)) : FString();
	return Street.IsEmpty() ? District : District.IsEmpty() ? Street : Street + TEXT(", ") + District;
}

// ------------------------------------------------------------------------------------------------- the banner
void ANHStreets::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	BannerLeft = FMath::Max(0.f, BannerLeft - DeltaSeconds);
	if ((Think -= DeltaSeconds) <= 0.f)
	{
		Think = 0.4f;
		// A new street's name goes up once you have been on it a second: crossing a side street says nothing
		const FVector2D Here(Pawn->GetActorLocation());
		const FString Street = StreetAt(Here, 2000.f);
		if (Street.IsEmpty() || Street == Current)
		{
			Pending.Reset();
		}
		else if (Street != Pending)
		{
			Pending = Street;
			PendingFor = 0.f;
		}
		else if ((PendingFor += 0.4f) >= 1.f)
		{
			Current = Street;
			const UNHGameData* Data = UNHGameData::Get(this);
			const FString District = Data ? Data->DistrictAt(Pawn->GetActorLocation()) : FString();
			Banner = District.IsEmpty() ? Street : Street + TEXT("  ·  ") + District;
			BannerLeft = 4.5f;
		}
		UpdateSigns(Pawn->GetActorLocation());
	}
}

void ANHStreets::DrawBanner(AHUD* Hud, UCanvas* Canvas)
{
	if (BannerLeft <= 0.f || !Hud || !Canvas || !GEngine || Hud->GetWorld() != GetWorld() || !Hud->bShowHUD)
	{
		return;
	}
	const float S = Canvas->ClipY / 1080.f, Fade = FMath::Clamp(FMath::Min(BannerLeft, 4.5f - BannerLeft) * 2.5f, 0.f, 1.f);
	UFont* Font = GEngine->GetLargeFont();
	float W = 0.f, H = 0.f;
	Hud->GetTextSize(Banner, W, H, Font, 1.5f * S);
	const float X = (Canvas->ClipX - W) * 0.5f, Y = Canvas->ClipY * 0.2f;
	Hud->DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f * Fade), X - 26.f * S, Y - 10.f * S, W + 52.f * S, H + 20.f * S);
	Hud->DrawRect(FLinearColor(1.f, 0.77f, 0.f, Fade), X - 26.f * S, Y + H + 10.f * S, W + 52.f * S, 3.f * S);
	Hud->DrawText(Banner, FLinearColor(0.96f, 0.95f, 0.9f, Fade), X, Y, Font, 1.5f * S);
}

// -------------------------------------------------------------------------------------------- names on the maps
void ANHStreets::DrawNames(AHUD* Hud, float X, float Y, float Size, const FVector2D& Corner, float Span, int32 MaxNames, float Scale) const
{
	if (!Hud || !GEngine || Span > 1200000.f)
	{
		return; // from 12 km up the map is too small for names
	}
	// how minor a street may be and still be drawn or named, by how much ground the view covers
	const int32 DrawRank = Span <= 250000.f ? 8 : Span <= 500000.f ? 6 : -1, NameRank = Span <= 90000.f ? 8 : Span <= 250000.f ? 6 : Span <= 600000.f ? 4 : 2;
	const FVector2D Centre = Corner + FVector2D(Span * 0.5f);
	const FIntPoint Lo = NHStreetData::CellOf(Corner), Hi = NHStreetData::CellOf(Corner + FVector2D(Span));
	struct FLabel { FString Name; FVector2D At; float Score; };
	TMap<FString, FLabel> Labels;
	TSet<FIntPoint> Seen;
	for (int32 CX = Lo.X; CX <= Hi.X; ++CX)
	{
		for (int32 CY = Lo.Y; CY <= Hi.Y; ++CY)
		{
			const TArray<FIntPoint>* Cell = Cells.Find(FIntPoint(CX, CY));
			if (!Cell)
			{
				continue;
			}
			for (const FIntPoint& Seg : *Cell)
			{
				const FStreet& S = Streets[Seg.X];
				const FVector2D A = (S.Points[Seg.Y] - Corner) / Span, B = (S.Points[Seg.Y + 1] - Corner) / Span, Mid = (A + B) * 0.5f;
				const bool bIn = Mid.X > 0.03f && Mid.X < 0.97f && Mid.Y > 0.03f && Mid.Y < 0.97f;
				bool bAlready = false;
				Seen.Add(Seg, &bAlready);
				// side streets, which the road graph the map is drawn from does not have
				if (!bAlready && S.Rank >= 5 && S.Rank <= DrawRank && A.X >= 0.f && A.X <= 1.f && A.Y >= 0.f && A.Y <= 1.f && B.X >= 0.f && B.X <= 1.f && B.Y >= 0.f && B.Y <= 1.f)
				{
					Hud->DrawLine(X + A.X * Size, Y + A.Y * Size, X + B.X * Size, Y + B.Y * Size, FLinearColor(0.42f, 0.41f, 0.38f), (S.Rank >= 8 ? 1.f : 1.8f) * Scale);
				}
				if (bIn && !S.Name.IsEmpty() && S.Rank <= NameRank)
				{
					// one label a street: where it passes nearest the middle of the view; main roads first
					const float Score = S.Rank * 1000.f + static_cast<float>(FVector2D::Distance(Mid, FVector2D(0.5f))) * 900.f;
					FLabel& L = Labels.FindOrAdd(S.Name);
					if (L.Name.IsEmpty() || Score < L.Score)
					{
						L = { S.Name, Mid, Score };
					}
				}
			}
		}
	}
	// the main roads beyond the districts, from the road graph
	if (const UNHGameData* Data = UNHGameData::Get(this); Data && Data->bRealCity)
	{
		TArray<FNHRoadSeg> Near;
		Data->RoadsNear(Centre, Span * 0.71f, Near);
		for (const FNHRoadSeg& Seg : Near)
		{
			const FNHRoadWay& Way = Data->RoadWays[Seg.Way];
			if (Way.Name.IsEmpty() || Way.Class > FMath::Min(NameRank, 4) || Labels.Contains(Way.Name))
			{
				continue;
			}
			const FVector2D Mid = ((Data->RoadNodes[Way.Nodes[Seg.Index]] + Data->RoadNodes[Way.Nodes[Seg.Index + 1]]) * 0.5f - Corner) / Span;
			if (Mid.X > 0.03f && Mid.X < 0.97f && Mid.Y > 0.03f && Mid.Y < 0.97f)
			{
				Labels.Add(Way.Name, { Way.Name, Mid, Way.Class * 1000.f + static_cast<float>(FVector2D::Distance(Mid, FVector2D(0.5f))) * 900.f });
			}
		}
	}
	TArray<FLabel> Sorted;
	Labels.GenerateValueArray(Sorted);
	Sorted.Sort([](const FLabel& A, const FLabel& B) { return A.Score < B.Score; });
	UFont* Font = GEngine->GetMediumFont();
	TArray<FBox2D> Used;
	int32 Written = 0;
	for (const FLabel& L : Sorted)
	{
		if (Written >= MaxNames)
		{
			break;
		}
		float W = 0.f, H = 0.f;
		Hud->GetTextSize(L.Name, W, H, Font, 0.8f * Scale);
		const FVector2D At(FMath::Clamp(X + L.At.X * Size - W * 0.5f, X + 2.f, X + Size - W - 2.f), Y + L.At.Y * Size - H * 0.5f);
		const FBox2D Box(At - FVector2D(4.f, 2.f), At + FVector2D(W + 4.f, H + 2.f));
		if (W > Size * 0.9f || Used.ContainsByPredicate([&Box](const FBox2D& Other) { return Other.Intersect(Box); }))
		{
			continue; // would run over another name
		}
		Used.Add(Box);
		Hud->DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), At.X - 3.f, At.Y - 1.f, W + 6.f, H + 2.f);
		Hud->DrawText(L.Name, FLinearColor(0.96f, 0.95f, 0.9f), At.X, At.Y, Font, 0.8f * Scale);
		++Written;
	}
}

// --------------------------------------------------------------------------------------------------- the signs
AActor* ANHStreets::MakeSign(const FJunction& Junction)
{
	// up to two streets' plates on one pole, each lying along its street, at the corner between them
	struct FPlate { FString Name; FVector2D Dir; float Half; };
	TArray<FPlate, TInlineAllocator<2>> Plates;
	for (const FString& Name : Junction.Names)
	{
		const FStreet* Best = nullptr;
		FVector2D BestDir = FVector2D(1.f, 0.f);
		float BestDist = FMath::Square(6000.f);
		const FIntPoint Mid = NHStreetData::CellOf(Junction.At);
		for (int32 CX = Mid.X - 1; CX <= Mid.X + 1; ++CX)
		{
			for (int32 CY = Mid.Y - 1; CY <= Mid.Y + 1; ++CY)
			{
				if (const TArray<FIntPoint>* Cell = Cells.Find(FIntPoint(CX, CY)))
				{
					for (const FIntPoint& Seg : *Cell)
					{
						const FStreet& S = Streets[Seg.X];
						const float Dist = S.Name == Name ? FMath::PointDistToSegmentSquared(FVector(Junction.At, 0.f), FVector(S.Points[Seg.Y], 0.f), FVector(S.Points[Seg.Y + 1], 0.f)) : BestDist;
						if (Dist < BestDist)
						{
							BestDist = Dist;
							Best = &S;
							BestDir = (S.Points[Seg.Y + 1] - S.Points[Seg.Y]).GetSafeNormal();
						}
					}
				}
			}
		}
		if (Best && Plates.Num() < 2)
		{
			Plates.Add({ Name, BestDir, Best->Width * 0.5f });
		}
	}
	if (Plates.Num() == 0)
	{
		return nullptr;
	}
	// off the carriageway of both streets: out along each street's normal by half its width and a metre
	const FVector2D N0(-Plates[0].Dir.Y, Plates[0].Dir.X);
	FVector2D Spot = Junction.At + N0 * (Plates[0].Half + 120.f);
	if (Plates.Num() > 1)
	{
		const FVector2D N1(-Plates[1].Dir.Y, Plates[1].Dir.X);
		Spot += N1 * (Plates[1].Half + 120.f) * (FMath::Abs(N1 | N0) > 0.9f ? 0.f : 1.f);
	}
	float Ground = 0.f;
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, FVector(Spot.X, Spot.Y, 1500.f), FVector(Spot.X, Spot.Y, -1000.f), FCollisionObjectQueryParams(ECC_WorldStatic)))
	{
		Ground = Hit.ImpactPoint.Z;
	}
	AActor* Sign = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(Spot.X, Spot.Y, Ground)));
	if (!Sign)
	{
		return nullptr;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Sign);
	Sign->SetRootComponent(Root);
	Root->RegisterComponent();
	Sign->SetActorLocation(FVector(Spot.X, Spot.Y, Ground));
	NHShapes::AddPiece(Sign, Root, ENHShape::Cylinder, FVector(0.f, 0.f, 150.f), FVector(7.f, 7.f, 300.f), FNHSurface(FLinearColor(0.35f, 0.36f, 0.37f), 0.5f, 0.3f, 0.6f));
	for (int32 I = 0; I < Plates.Num(); ++I)
	{
		// a green plate with the name in white on both faces, the second a little lower
		const float Wide = FMath::Clamp(Plates[I].Name.Len() * 9.5f + 30.f, 90.f, 260.f), Z = 285.f - I * 34.f;
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Plates[I].Dir.Y, Plates[I].Dir.X));
		USceneComponent* Arm = NHShapes::AddPivot(Sign, Root, FVector(0.f, 0.f, Z));
		Arm->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
		NHShapes::AddPiece(Sign, Arm, ENHShape::Box, FVector(Wide * 0.5f + 6.f, 0.f, 0.f), FVector(Wide, 3.f, 28.f), FNHSurface(NHStreetData::SignGreen, 0.45f, 0.2f));
		for (int32 Face = 0; Face < 2; ++Face)
		{
			UTextRenderComponent* Text = NewObject<UTextRenderComponent>(Sign);
			Text->SetupAttachment(Arm);
			Text->SetText(FText::FromString(Plates[I].Name.ToUpper()));
			Text->SetHorizontalAlignment(EHTA_Center);
			Text->SetVerticalAlignment(EVRTA_TextCenter);
			Text->SetWorldSize(15.f);
			Text->SetTextRenderColor(FColor(240, 240, 232));
			Text->SetRelativeLocationAndRotation(FVector(Wide * 0.5f + 6.f, Face ? -1.9f : 1.9f, 0.f), FRotator(0.f, Face ? -90.f : 90.f, 0.f));
			Text->RegisterComponent();
		}
	}
	return Sign;
}

void ANHStreets::UpdateSigns(const FVector& Player)
{
	// signs for the junctions within 160 m, at most thirty; gone beyond 240 m
	const FVector2D Here(Player);
	for (auto It = Signs.CreateIterator(); It; ++It)
	{
		if (!It.Value() || FVector2D::Distance(Junctions[It.Key()].At, Here) > 24000.f)
		{
			if (It.Value())
			{
				It.Value()->Destroy();
			}
			It.RemoveCurrent();
		}
	}
	const FIntPoint Mid = NHStreetData::CellOf(Here);
	int32 MadeNow = 0;
	for (int32 CX = Mid.X - 1; CX <= Mid.X + 1; ++CX)
	{
		for (int32 CY = Mid.Y - 1; CY <= Mid.Y + 1; ++CY)
		{
			if (const TArray<int32>* Cell = JunctionCells.Find(FIntPoint(CX, CY)))
			{
				for (const int32 Index : *Cell)
				{
					if (Signs.Num() < 30 && MadeNow < 4 && !Signs.Contains(Index) && FVector2D::Distance(Junctions[Index].At, Here) < 16000.f)
					{
						Signs.Add(Index, MakeSign(Junctions[Index]));
						++MadeNow; // a few at a time, so coming into a district does not hitch
					}
				}
			}
		}
	}
}
