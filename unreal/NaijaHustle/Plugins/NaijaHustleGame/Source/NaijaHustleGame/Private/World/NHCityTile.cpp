#include "World/NHCityTile.h"

#include "Components/SpotLightComponent.h"
#include "Components/TextRenderComponent.h"

namespace NHTile
{
	const FName GeneratedTag(TEXT("NHGenerated"));

	FNHSurface Asphalt()   { return FNHSurface(FLinearColor(0.085f, 0.085f, 0.09f), 0.85f, 1.f).As(ENHSurfaceType::Asphalt); } // sun-bleached, worn grey
	FNHSurface Laterite()  { return FNHSurface(FLinearColor(0.36f, 0.16f, 0.07f), 0.95f, 1.f).As(ENHSurfaceType::Dirt); }
	FNHSurface Pavement()  { return FNHSurface(FLinearColor(0.3f, 0.29f, 0.27f), 0.8f, 0.9f).As(ENHSurfaceType::Concrete); }
	FNHSurface Dirt()      { return FNHSurface(FLinearColor(0.27f, 0.15f, 0.08f), 0.95f, 0.9f).As(ENHSurfaceType::Dirt); }
	FNHSurface Earth()     { return FNHSurface(FLinearColor(0.2f, 0.14f, 0.09f), 0.95f, 0.9f).As(ENHSurfaceType::Dirt); }
	FNHSurface Plot()      { return FNHSurface(FLinearColor(0.24f, 0.23f, 0.22f), 0.9f, 0.9f).As(ENHSurfaceType::Concrete); }
	FNHSurface Yard()      { return FNHSurface(FLinearColor(0.28f, 0.27f, 0.26f), 0.85f, 0.9f).As(ENHSurfaceType::Concrete); }
	FNHSurface Grass()     { return FNHSurface(FLinearColor(0.07f, 0.13f, 0.03f), 0.95f, 0.5f); }
	FNHSurface Forecourt() { return FNHSurface(FLinearColor(0.38f, 0.38f, 0.36f), 0.6f, 0.9f).As(ENHSurfaceType::Concrete); }
	FNHSurface Water()     { return FNHSurface(FLinearColor(0.012f, 0.03f, 0.035f), 0.04f, 0.f); }
	FNHSurface Kerb()      { return FNHSurface(FLinearColor(0.45f, 0.43f, 0.4f), 0.75f, 0.8f).As(ENHSurfaceType::Concrete); }
	FNHSurface Concrete()  { return FNHSurface(FLinearColor(0.3f, 0.3f, 0.29f), 0.85f, 0.6f).As(ENHSurfaceType::Concrete); }

	FVector Dir(float YawDeg) { const float A = FMath::DegreesToRadians(YawDeg); return FVector(FMath::Cos(A), FMath::Sin(A), 0.f); }
}

TCHAR ANHCityTile::CellAt(int32 C, int32 R) const
{
	const int32 W = Cols + 2, Index = (R + 1) * W + (C + 1);
	return (C < -1 || R < -1 || C > Cols || R > Rows || !Cells.IsValidIndex(Index)) ? TEXT('#') : Cells[Index];
}

bool ANHCityTile::DustyAt(int32 C, int32 R) const
{
	const int32 W = Cols + 2, Index = (R + 1) * W + (C + 1);
	return DustyMask.IsValidIndex(Index) && DustyMask[Index] == TEXT('1');
}

float ANHCityTile::TopZ(TCHAR Cell) const
{
	switch (Cell)
	{
	case TEXT('R'): return 0.f;
	case TEXT('W'):
	case TEXT('L'): return WaterZ;
	case TEXT('#'): return KerbZ;
	default:        return KerbZ;
	}
}

void ANHCityTile::Build()
{
	ClearGenerated();
	BuildGround();
	BuildDustyStreet();
	BuildMarkings();
	BuildProps();
	BuildShopfronts();
	BuildSigns();
	BuildLamps();
}

void ANHCityTile::ClearGenerated()
{
	TArray<USceneComponent*> Old;
	GetComponents<USceneComponent>(Old);
	for (USceneComponent* C : Old)
	{
		if (C && C->ComponentHasTag(NHTile::GeneratedTag))
		{
			C->DestroyComponent();
		}
	}
	Generated.Reset();
	LampLights.Reset();
}

// ---------------------------------------------------------------------------------------------------- ground
void ANHCityTile::BuildGround()
{
	using namespace NHTile;
	const float X0 = Col0 * CellSize, Y0 = Row0 * CellSize;
	auto SurfaceFor = [&](TCHAR Cell, bool bDusty, int32 C) -> FNHSurface
	{
		switch (Cell)
		{
		case TEXT('R'): return IsLagoon(C) ? Concrete() : (bDusty ? Laterite() : Asphalt());
		case TEXT('K'): return bDusty ? Dirt() : Pavement();
		case TEXT('G'): return bDusty ? Dirt() : Earth();
		case TEXT('P'): return Yard();
		case TEXT('V'): return Grass();
		case TEXT('F'): return Forecourt();
		case TEXT('W'):
		case TEXT('L'): return Water();
		default:        return bDusty ? Dirt() : Plot();
		}
	};

	// top slabs, merged along each row while the cell type stays the same
	for (int32 R = 0; R < Rows; ++R)
	{
		int32 C = 0;
		while (C < Cols)
		{
			const TCHAR Cell = CellAt(C, R);
			const bool bDusty = DustyAt(C, R), bLag = IsLagoon(C);
			int32 E = C;
			while (E + 1 < Cols && CellAt(E + 1, R) == Cell && DustyAt(E + 1, R) == bDusty && IsLagoon(E + 1) == bLag)
			{
				++E;
			}
			const float XA = X0 + C * CellSize, XB = X0 + (E + 1) * CellSize, YA = Y0 + R * CellSize, YB = YA + CellSize;
			const float Top = TopZ(Cell), Bottom = (Cell == TEXT('W') || Cell == TEXT('L')) ? Top - 20.f : (Cell == TEXT('R') && bLag ? -60.f : -40.f);
			AddBox(FVector((XA + XB) * 0.5f, (YA + YB) * 0.5f, (Top + Bottom) * 0.5f), FVector(XB - XA, CellSize, Top - Bottom), SurfaceFor(Cell, bDusty, C), true, FRotator::ZeroRotator, true);
			C = E + 1;
		}
	}

	// per-cell edges: kerbs and gutters beside the road, embankments and bridge railings by the water, bridge piers
	const FIntPoint Sides[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
	for (int32 R = 0; R < Rows; ++R)
	{
		for (int32 C = 0; C < Cols; ++C)
		{
			const TCHAR Cell = CellAt(C, R);
			const FVector Center(X0 + (C + 0.5f) * CellSize, Y0 + (R + 0.5f) * CellSize, 0.f);
			const bool bWet = Cell == TEXT('W') || Cell == TEXT('L');
			const bool bRoad = Cell == TEXT('R');
			const bool bBridge = bRoad && IsLagoon(C);
			for (const FIntPoint& S : Sides)
			{
				const TCHAR N = CellAt(C + S.X, R + S.Y);
				if (N == TEXT('#'))
				{
					continue;
				}
				const bool bNWet = N == TEXT('W') || N == TEXT('L');
				const FVector Out(S.X, S.Y, 0.f);
				const FVector Edge = Center + Out * (CellSize * 0.5f);
				auto EdgeBox = [&](float Inset, float Thick, float Z0, float Z1, const FNHSurface& Surf, bool bSolid)
				{
					const FVector P = Edge - Out * (Inset + Thick * 0.5f) + FVector(0, 0, (Z0 + Z1) * 0.5f);
					const FVector Sz = S.X != 0 ? FVector(Thick, CellSize, Z1 - Z0) : FVector(CellSize, Thick, Z1 - Z0);
					AddBox(P, Sz, Surf, bSolid, FRotator::ZeroRotator, true);
				};
				if (!bWet && !bRoad && N == TEXT('R'))
				{
					if (DustyAt(C, R))
					{
						EdgeBox(0.f, 18.f, -2.f, KerbZ + 2.f, Kerb(), true); // bare kerb stone
					}
					else // tarred roads: kerb stones painted black and white, a metre each, as on Lagos roads
					{
						const FNHSurface White = FNHSurface(FLinearColor(0.62f, 0.61f, 0.57f), 0.75f, 0.8f).As(ENHSurfaceType::Concrete);
						const FNHSurface Black = FNHSurface(FLinearColor(0.035f, 0.035f, 0.035f), 0.75f, 0.8f).As(ENHSurfaceType::Concrete);
						const FVector Along = S.X != 0 ? FVector(0.f, 1.f, 0.f) : FVector(1.f, 0.f, 0.f);
						for (int32 K = 0; K < 4; ++K)
						{
							const FVector P = Edge - Out * 9.f + Along * ((K - 1.5f) * 100.f) + FVector(0, 0, KerbZ * 0.5f);
							const int32 Stripe = FMath::FloorToInt32((S.X != 0 ? P.Y : P.X) / 100.f); // by world position, so the pattern runs on from cell to cell
							AddBox(P, S.X != 0 ? FVector(18.f, 100.f, KerbZ + 4.f) : FVector(100.f, 18.f, KerbZ + 4.f), (Stripe & 1) ? Black : White, true, FRotator::ZeroRotator, true);
						}
					}
				}
				if (bRoad && !bBridge && N != TEXT('R') && !bNWet)
				{
					EdgeBox(0.f, 25.f, 0.f, 0.6f, FNHSurface(FLinearColor(0.02f, 0.02f, 0.02f), 0.4f, 1.f), false); // gutter
				}
				if (!bWet && !bBridge && bNWet)
				{
					EdgeBox(0.f, 30.f, WaterZ - 60.f, TopZ(Cell), Concrete(), true); // embankment
				}
				if (bBridge && bNWet)
				{
					EdgeBox(0.f, 20.f, 0.f, 100.f, Concrete(), true); // bridge parapet
				}
			}
			if (bBridge && (Col0 + C) % 4 == 0 && R % 2 == 0)
			{
				AddShape(ENHShape::Cylinder, Center + FVector(0, 0, (WaterZ - 100.f - 60.f) * 0.5f), FVector(120.f, 120.f, 160.f + FMath::Abs(WaterZ)), Concrete(), true, FRotator::ZeroRotator, true);
			}
		}
	}
}

// Oke-Erupe's dirt roads: long low rut mounds along the way the road runs, and rubbish heaps on the verges.
// All of it is detail (no collision), so driving and walking are unchanged.
void ANHCityTile::BuildDustyStreet()
{
	using namespace NHTile;
	const float X0 = Col0 * CellSize, Y0 = Row0 * CellSize;
	const FLinearColor Litter[] = { FLinearColor(0.02f, 0.02f, 0.022f), FLinearColor(0.03f, 0.1f, 0.35f), FLinearColor(0.6f, 0.6f, 0.58f),
		FLinearColor(0.45f, 0.08f, 0.05f), FLinearColor(0.6f, 0.45f, 0.05f), FLinearColor(0.02f, 0.02f, 0.022f) };
	for (int32 R = 0; R < Rows; ++R)
	{
		for (int32 C = 0; C < Cols; ++C)
		{
			if (!DustyAt(C, R))
			{
				continue;
			}
			const TCHAR Cell = CellAt(C, R);
			const int32 GC = Col0 + C, GR = Row0 + R; // city cell: the same result whichever tile builds it
			const FVector Center(X0 + (C + 0.5f) * CellSize, Y0 + (R + 0.5f) * CellSize, 0.f);
			auto IsRoad = [&](int32 DC, int32 DR) { return CellAt(C + DC, R + DR) == TEXT('R'); };

			if (Cell == TEXT('R') && !IsLagoon(C))
			{
				const bool bAlongX = IsRoad(-1, 0) && IsRoad(1, 0), bAlongY = IsRoad(0, -1) && IsRoad(0, 1);
				const float Yaw = (bAlongX && !bAlongY) ? 0.f : (bAlongY && !bAlongX) ? 90.f : Hash01(GC, GR, 10) * 180.f;
				const int32 Num = 1 + static_cast<int32>(Hash01(GC, GR, 11) * 2.f);
				for (int32 K = 0; K < Num; ++K)
				{
					const FVector Size(280.f + 320.f * Hash01(GC, GR, 40 + K), 45.f + 40.f * Hash01(GC, GR, 50 + K), 20.f + 14.f * Hash01(GC, GR, 60 + K));
					const FVector P = Center + FVector((Hash01(GC, GR, 20 + K) - 0.5f) * CellSize * 0.8f, (Hash01(GC, GR, 30 + K) - 0.5f) * CellSize * 0.8f, -0.3f * Size.Z);
					FNHSurface Mound = Laterite();
					Mound.Color *= 0.94f + 0.12f * Hash01(GC, GR, 70 + K);
					// most of the flattened sphere is under the road: only its cap shows, a soft ridge 4 to 7 cm high and a few metres long
					AddShapeTransform(ENHShape::Sphere, FTransform(FRotator(0.f, Yaw + (Hash01(GC, GR, 80 + K) - 0.5f) * 16.f, 0.f), P, Size / 100.f), Mound, false, true);
				}
			}
			else if ((Cell == TEXT('K') || Cell == TEXT('G')) && (IsRoad(1, 0) || IsRoad(-1, 0) || IsRoad(0, 1) || IsRoad(0, -1)) && Hash01(GC, GR, 90) < 0.22f)
			{
				const FVector Base = Center + FVector((Hash01(GC, GR, 91) - 0.5f) * 220.f, (Hash01(GC, GR, 92) - 0.5f) * 220.f, KerbZ);
				const float Wide = 150.f + 90.f * Hash01(GC, GR, 93), High = 50.f + 30.f * Hash01(GC, GR, 94);
				AddShapeTransform(ENHShape::Sphere, FTransform(FRotator(0.f, Hash01(GC, GR, 95) * 180.f, 0.f), Base, FVector(Wide, Wide * 0.8f, High) / 100.f),
					FNHSurface(FLinearColor(0.09f, 0.075f, 0.06f), 0.95f, 0.9f).As(ENHSurfaceType::Dirt), false, true);
				const int32 Bags = 6 + static_cast<int32>(Hash01(GC, GR, 96) * 4.f);
				for (int32 K = 0; K < Bags; ++K)
				{
					const float A = Hash01(GC, GR, 100 + K) * 2.f * PI, D = Hash01(GC, GR, 110 + K) * Wide * 0.42f;
					const float S = 18.f + 24.f * Hash01(GC, GR, 120 + K);
					const FVector P = Base + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D * 0.8f, High * 0.5f * (1.f - D / Wide) + S * 0.2f);
					const FNHSurface Bag = FNHSurface(Litter[static_cast<int32>(Hash01(GC, GR, 130 + K) * 6.f) % 6], 0.5f, 0.6f).As(ENHSurfaceType::Tarp);
					AddShapeTransform(K % 2 ? ENHShape::Sphere : ENHShape::Box, FTransform(FRotator(Hash01(GC, GR, 140 + K) * 30.f, Hash01(GC, GR, 150 + K) * 360.f, 0.f), P, FVector(S, S * 0.8f, S * 0.6f) / 100.f), Bag, false, true);
				}
			}
		}
	}
}

void ANHCityTile::BuildMarkings()
{
	const float X0 = Col0 * CellSize, Y0 = Row0 * CellSize;
	for (const FNHMarking& M : Markings)
	{
		// nobody paints lines on a dirt road
		if (DustyAt(FMath::FloorToInt32(((M.A.X + M.B.X) * 0.5f - X0) / CellSize), FMath::FloorToInt32(((M.A.Y + M.B.Y) * 0.5f - Y0) / CellSize)))
		{
			continue;
		}
		const FVector2D D = M.B - M.A;
		const float Len = FMath::Max(1.f, static_cast<float>(D.Size()));
		const FVector Mid((M.A.X + M.B.X) * 0.5f, (M.A.Y + M.B.Y) * 0.5f, 0.6f);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
		// worn paint: duller than fresh, and on the asphalt's material so the road's grime runs across it
		AddBox(Mid, FVector(Len, M.Width, 1.f), FNHSurface(M.Color * 0.7f, 0.7f, 0.8f).As(ENHSurfaceType::Asphalt), false, FRotator(0.f, Yaw, 0.f), true);
	}
}

void ANHCityTile::BuildProps()
{
	for (const FNHPropInstance& P : Props)
	{
		AddShapeTransform(P.Shape, P.Transform, P.Surface, P.bSolid, true);
	}
}

// ---------------------------------------------------------------------------------------------------- shop fronts and signs
void ANHCityTile::BuildShopfronts()
{
	using namespace NHTile;
	const FLinearColor Bright[] = { FLinearColor(0.75f, 0.12f, 0.05f), FLinearColor(0.03f, 0.36f, 0.15f), FLinearColor(0.9f, 0.55f, 0.f), FLinearColor(0.05f, 0.2f, 0.7f), FLinearColor(0.7f, 0.08f, 0.26f), FLinearColor(0.2f, 0.03f, 0.37f) };
	const FNHSurface Frame = FNHSurface(FLinearColor(0.06f, 0.06f, 0.06f), 0.5f, 0.5f, 0.4f).As(ENHSurfaceType::Metal), Steel = FNHSurface(FLinearColor(0.42f, 0.44f, 0.45f), 0.45f, 0.3f, 0.6f).As(ENHSurfaceType::Metal);
	const FNHSurface Inside(FLinearColor(0.02f, 0.018f, 0.015f), 0.8f, 0.f, 0.f, 9.f, 1.f), Wood = FNHSurface(FLinearColor(0.22f, 0.14f, 0.08f), 0.8f, 0.6f).As(ENHSurfaceType::Wood);
	for (int32 I = 0; I < Shopfronts.Num(); ++I)
	{
		const FNHShopfront& F = Shopfronts[I];
		const FVector N = Dir(F.Yaw), T(-N.Y, N.X, 0.f);
		const FRotator Rot(0.f, F.Yaw, 0.f);
		const float W = F.Width, H = F.Height;
		auto Piece = [&](float Out, float Side, float Z, const FVector& Size, const FNHSurface& S, bool bSolid = false)
		{
			AddBox(F.Location + N * Out + T * Side + FVector(0, 0, Z), Size, S, bSolid, Rot, true);
		};
		// frame: two posts and a lintel
		Piece(4.f, -W * 0.5f, H * 0.5f, FVector(8.f, 8.f, H), Frame);
		Piece(4.f, W * 0.5f, H * 0.5f, FVector(8.f, 8.f, H), Frame);
		Piece(4.f, 0.f, H + 6.f, FVector(8.f, W + 8.f, 12.f), Frame);
		const float Pick = Hash01(I, Col0, Row0);
		switch (F.State)
		{
		case ENHShopfrontState::Shutter:
			Piece(3.f, 0.f, H * 0.5f, FVector(4.f, W - 8.f, H - 4.f), Steel);
			break;
		case ENHShopfrontState::Painted:
			Piece(3.f, 0.f, H * 0.5f, FVector(4.f, W - 8.f, H - 4.f), FNHSurface(Bright[static_cast<int32>(Pick * 6.f) % 6], 0.6f, 0.3f, 0.2f).As(ENHSurfaceType::Metal));
			break;
		case ENHShopfrontState::Half:
			Piece(3.f, 0.f, H * 0.78f, FVector(4.f, W - 8.f, H * 0.44f), Steel);
			Piece(1.f, 0.f, H * 0.28f, FVector(2.f, W - 8.f, H * 0.56f), Inside);
			break;
		case ENHShopfrontState::Open:
			if (!F.bEnterable)
			{
				Piece(1.f, 0.f, H * 0.5f, FVector(2.f, W - 8.f, H - 4.f), Inside);
			}
			Piece(40.f, W * 0.15f, 45.f, FVector(55.f, W * 0.5f, 90.f), Wood, true); // counter out front
			break;
		}
		if (Pick < 0.55f) // a sloping awning over half the fronts
		{
			AddBox(F.Location + N * 60.f + FVector(0, 0, H + 40.f), FVector(120.f, W + 20.f, 4.f), FNHSurface(Bright[static_cast<int32>(Pick * 97.f) % 6], 0.7f, 1.f).As(ENHSurfaceType::Tarp), false, FRotator(-14.f, F.Yaw, 0.f), true);
		}
	}
}

void ANHCityTile::BuildSigns()
{
	using namespace NHTile;
	for (const FNHSign& S : Signs)
	{
		const FVector N = Dir(S.Yaw);
		const FVector Mid = S.Location + N * 3.f + FVector(0, 0, S.Height * 0.5f);
		AddBox(Mid, FVector(6.f, S.Width, S.Height), FNHSurface(S.Background, 0.6f, 0.2f, 0.f, 3.f, 0.f), false, FRotator(0.f, S.Yaw, 0.f), true);

		auto Text = [&](const FString& Str, float ZOff, float MaxSize)
		{
			if (Str.IsEmpty())
			{
				return;
			}
			// the default font is roughly 0.6 em per character: shrink long names to fit the board
			const float Size = FMath::Min(MaxSize, S.Width * 0.92f / FMath::Max(1.f, Str.Len() * 0.6f));
			UTextRenderComponent* T = NewObject<UTextRenderComponent>(this, NAME_None, RF_Transactional);
			T->CreationMethod = EComponentCreationMethod::Instance;
			T->ComponentTags.Add(GeneratedTag);
			T->SetupAttachment(GetRootComponent());
			T->SetText(FText::FromString(Str));
			T->SetHorizontalAlignment(EHTA_Center);
			T->SetVerticalAlignment(EVRTA_TextCenter);
			T->SetWorldSize(Size);
			T->SetTextRenderColor(S.Foreground.ToFColor(true));
			T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			T->RegisterComponent();
			AddInstanceComponent(T);
			T->SetWorldLocationAndRotation(Mid + N * 4.f + FVector(0, 0, ZOff), FRotator(0.f, S.Yaw, 0.f));
			Generated.Add(T);
		};
		if (S.Sub.IsEmpty())
		{
			Text(S.Title, 0.f, S.Height * 0.6f);
		}
		else
		{
			Text(S.Title, S.Height * 0.14f, S.Height * 0.42f);
			Text(S.Sub, -S.Height * 0.3f, S.Height * 0.2f);
		}
	}
}

// ---------------------------------------------------------------------------------------------------- lamps
void ANHCityTile::BuildLamps()
{
	for (const FVector& Head : LampHeads)
	{
		AddBox(Head, FVector(70.f, 35.f, 16.f), FNHSurface(FLinearColor(0.5f, 0.5f, 0.48f), 0.4f, 0.3f, 0.3f, 400.f, 1.f).As(ENHSurfaceType::Metal), false, FRotator::ZeroRotator, true);

		USpotLightComponent* L = NewObject<USpotLightComponent>(this, NAME_None, RF_Transactional);
		L->CreationMethod = EComponentCreationMethod::Instance;
		L->ComponentTags.Add(NHTile::GeneratedTag);
		L->SetupAttachment(GetRootComponent());
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensityUnits(ELightUnits::Lumens);
		L->SetIntensity(LampLumens * NightAlpha);
		L->SetLightColor(LampColor);
		L->SetAttenuationRadius(2600.f);
		L->SetInnerConeAngle(25.f);
		L->SetOuterConeAngle(62.f);
		L->SetCastShadows(false); // dozens of shadowed spot lights are expensive; step 8 decides which ones earn shadows
		L->SetVisibility(NightAlpha > 0.f);
		L->RegisterComponent();
		AddInstanceComponent(L);
		L->SetWorldLocationAndRotation(Head - FVector(0, 0, 12.f), FRotator(-90.f, 0.f, 0.f));
		Generated.Add(L);
		LampLights.Add(L);
	}
}

void ANHCityTile::SetNightLights(float Alpha)
{
	NightAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
	for (USpotLightComponent* L : LampLights)
	{
		if (L)
		{
			L->SetIntensity(LampLumens * NightAlpha);
			L->SetVisibility(NightAlpha > 0.f);
		}
	}
}
