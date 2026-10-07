#include "World/NHCityTile.h"

#include "Components/SpotLightComponent.h"
#include "Components/TextRenderComponent.h"

namespace NHTile
{
	const FName GeneratedTag(TEXT("NHGenerated"));

	FNHSurface Asphalt()   { return FNHSurface(FLinearColor(0.045f, 0.045f, 0.05f), 0.82f, 1.f); }
	FNHSurface Laterite()  { return FNHSurface(FLinearColor(0.36f, 0.16f, 0.07f), 0.95f, 1.f); }
	FNHSurface Pavement()  { return FNHSurface(FLinearColor(0.3f, 0.29f, 0.27f), 0.8f, 0.9f); }
	FNHSurface Dirt()      { return FNHSurface(FLinearColor(0.27f, 0.15f, 0.08f), 0.95f, 0.9f); }
	FNHSurface Earth()     { return FNHSurface(FLinearColor(0.2f, 0.14f, 0.09f), 0.95f, 0.9f); }
	FNHSurface Plot()      { return FNHSurface(FLinearColor(0.24f, 0.23f, 0.22f), 0.9f, 0.9f); }
	FNHSurface Yard()      { return FNHSurface(FLinearColor(0.28f, 0.27f, 0.26f), 0.85f, 0.9f); }
	FNHSurface Grass()     { return FNHSurface(FLinearColor(0.07f, 0.13f, 0.03f), 0.95f, 0.5f); }
	FNHSurface Forecourt() { return FNHSurface(FLinearColor(0.38f, 0.38f, 0.36f), 0.6f, 0.9f); }
	FNHSurface Water()     { return FNHSurface(FLinearColor(0.012f, 0.03f, 0.035f), 0.04f, 0.f); }
	FNHSurface Kerb()      { return FNHSurface(FLinearColor(0.45f, 0.43f, 0.4f), 0.75f, 0.8f); }
	FNHSurface Concrete()  { return FNHSurface(FLinearColor(0.3f, 0.3f, 0.29f), 0.85f, 0.6f); }

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
					EdgeBox(0.f, 18.f, -2.f, KerbZ + 2.f, Kerb(), true); // kerb stone
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

void ANHCityTile::BuildMarkings()
{
	for (const FNHMarking& M : Markings)
	{
		const FVector2D D = M.B - M.A;
		const float Len = FMath::Max(1.f, static_cast<float>(D.Size()));
		const FVector Mid((M.A.X + M.B.X) * 0.5f, (M.A.Y + M.B.Y) * 0.5f, 0.6f);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
		AddBox(Mid, FVector(Len, M.Width, 1.f), FNHSurface(M.Color, 0.6f, 0.8f), false, FRotator(0.f, Yaw, 0.f), true);
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
	const FNHSurface Frame(FLinearColor(0.06f, 0.06f, 0.06f), 0.5f, 0.5f, 0.4f), Steel(FLinearColor(0.42f, 0.44f, 0.45f), 0.45f, 0.3f, 0.6f);
	const FNHSurface Inside(FLinearColor(0.02f, 0.018f, 0.015f), 0.8f, 0.f, 0.f, 9.f, 1.f), Wood(FLinearColor(0.22f, 0.14f, 0.08f), 0.8f, 0.6f);
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
			Piece(3.f, 0.f, H * 0.5f, FVector(4.f, W - 8.f, H - 4.f), FNHSurface(Bright[static_cast<int32>(Pick * 6.f) % 6], 0.6f, 0.3f, 0.2f));
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
			AddBox(F.Location + N * 60.f + FVector(0, 0, H + 40.f), FVector(120.f, W + 20.f, 4.f), FNHSurface(Bright[static_cast<int32>(Pick * 97.f) % 6], 0.7f, 1.f), false, FRotator(-14.f, F.Yaw, 0.f), true);
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
		AddBox(Head, FVector(70.f, 35.f, 16.f), FNHSurface(FLinearColor(0.5f, 0.5f, 0.48f), 0.4f, 0.3f, 0.3f, 400.f, 1.f), false, FRotator::ZeroRotator, true);

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
