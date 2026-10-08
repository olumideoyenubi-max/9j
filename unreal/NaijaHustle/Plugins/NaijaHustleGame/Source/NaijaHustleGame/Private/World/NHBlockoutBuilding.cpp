#include "World/NHBlockoutBuilding.h"

namespace NHBuild
{
	const FLinearColor Iron(0.03f, 0.03f, 0.032f);
	const FLinearColor GlassDark(0.02f, 0.025f, 0.03f);
	const FLinearColor Trim(0.55f, 0.52f, 0.47f);
	const FLinearColor RawConcrete(0.33f, 0.32f, 0.3f);
	const FLinearColor Rust(0.26f, 0.11f, 0.05f);
	const FLinearColor ZincGrey(0.32f, 0.34f, 0.35f);
	const FLinearColor Timber(0.22f, 0.14f, 0.08f);
	const FLinearColor Tarp(0.02f, 0.12f, 0.45f);
	const FLinearColor Bright[] = {
		FLinearColor(0.75f, 0.12f, 0.05f), FLinearColor(0.03f, 0.36f, 0.15f), FLinearColor(0.9f, 0.55f, 0.0f), FLinearColor(0.05f, 0.2f, 0.7f),
		FLinearColor(0.7f, 0.08f, 0.26f), FLinearColor(0.85f, 0.82f, 0.75f), FLinearColor(0.2f, 0.03f, 0.37f), FLinearColor(0.0f, 0.25f, 0.21f) };

	FLinearColor Scale(const FLinearColor& C, float K) { return FLinearColor(FMath::Min(C.R * K, 1.f), FMath::Min(C.G * K, 1.f), FMath::Min(C.B * K, 1.f)); }

	/** Size of a box laid on a face: N = depth out of the wall, T = width along it, H = height */
	FVector FaceSize(const FVector& Normal, float N, float T, float H)
	{
		return FMath::Abs(Normal.X) > 0.5f ? FVector(N, T, H) : FVector(T, N, H);
	}
}

TArray<ANHBlockoutBuilding::FFace> ANHBlockoutBuilding::Faces() const
{
	const float SX = static_cast<float>(Size.X), SY = static_cast<float>(Size.Y);
	return {
		{ FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), SY, SX * 0.5f, ENHFace::East },
		{ FVector(-1.f, 0.f, 0.f), FVector(0.f, -1.f, 0.f), SY, SX * 0.5f, ENHFace::West },
		{ FVector(0.f, 1.f, 0.f), FVector(-1.f, 0.f, 0.f), SX, SY * 0.5f, ENHFace::South },
		{ FVector(0.f, -1.f, 0.f), FVector(1.f, 0.f, 0.f), SX, SY * 0.5f, ENHFace::North },
	};
}

void ANHBlockoutBuilding::Build()
{
	switch (Kind)
	{
	case ENHBuildingKind::Tower:       BuildTower(); break;
	case ENHBuildingKind::Stall:       BuildStall(); break;
	case ENHBuildingKind::Stilt:       BuildStilt(); break;
	case ENHBuildingKind::FuelStation: BuildFuelStation(); break;
	case ENHBuildingKind::BusShelter:  BuildBusShelter(); break;
	case ENHBuildingKind::Footbridge:  BuildFootbridge(); break;
	default:                           BuildHouse(); break;
	}
}

// ---------------------------------------------------------------------------------------------------- houses
void ANHBlockoutBuilding::BuildHouse()
{
	using namespace NHBuild;
	const int32 Storeys = FMath::Clamp(FMath::RoundToInt(Height / 300.f), 1, 12);
	const float SH = Height / Storeys;
	const FNHSurface Wall = FNHSurface(WallColor, 0.9f, 0.3f).As(WallType());

	if (bEnterableShop)
	{
		ShopInterior(SH);
		AddBox(FVector(0, 0, (SH + Height) * 0.5f), FVector(Size.X, Size.Y, Height - SH), Wall);
	}
	else
	{
		AddBox(FVector(0, 0, Height * 0.5f), FVector(Size.X, Size.Y, Height), Wall);
	}
	// dirty plinth and a cornice line at every floor
	AddBox(FVector(0, 0, 22.f), FVector(Size.X + 4.f, Size.Y + 4.f, 44.f), FNHSurface(Scale(WallColor, 0.55f), 0.95f, 0.6f).As(WallType()), false);
	for (int32 S = 1; S < Storeys; ++S)
	{
		AddBox(FVector(0, 0, S * SH), FVector(Size.X + 8.f, Size.Y + 8.f, 12.f), FNHSurface(Scale(WallColor, 1.12f), 0.85f, 0.3f).As(WallType()), false);
	}

	Windows(0.f, SH, Storeys, true);
	Balconies(SH, Storeys);
	if (Roof == ENHRoofStyle::Zinc)
	{
		ZincRoof(Height, Size, 40.f);
	}
	else
	{
		FlatRoof(Height);
	}
}

void ANHBlockoutBuilding::Windows(float Z0, float SH, int32 Storeys, bool bSkipStreetGround)
{
	using namespace NHBuild;
	const TArray<FFace> All = Faces();
	for (int32 FI = 0; FI < All.Num(); ++FI)
	{
		const FFace& F = All[FI];
		const bool bStreet = (StreetFaces & static_cast<int32>(F.Flag)) != 0;
		const int32 Bays = FMath::Max(1, FMath::FloorToInt(F.Length / 250.f));
		const float BW = F.Length / Bays;
		const float WW = FMath::Min(110.f, BW - 40.f), WH = 130.f;
		if (WW < 40.f)
		{
			continue;
		}
		for (int32 S = 0; S < Storeys; ++S)
		{
			if (S == 0 && ((bStreet && bSkipStreetGround) || (bEnterableShop && (ShopFace & static_cast<int32>(F.Flag)))))
			{
				continue; // shop fronts (from the city data) or the shop's own doorway
			}
			for (int32 B = 0; B < Bays; ++B)
			{
				const float H = Hash01(Seed, S * 31 + B, FI);
				if (H < 0.08f)
				{
					continue; // a blank bay
				}
				const float Along = -F.Length * 0.5f + BW * (B + 0.5f);
				const float Z = Z0 + S * SH + 95.f + WH * 0.5f;
				auto At = [&](float Out, float Up, float Side = 0.f) { return F.Normal * (F.Depth + Out) + F.Tangent * (Along + Side) + FVector(0, 0, Z + Up); };

				const bool bLit = Hash01(Seed, B, S + 99) < 0.4f;
				AddBox(At(1.f, 0.f), FaceSize(F.Normal, 4.f, WW + 16.f, WH + 16.f), FNHSurface(Trim, 0.8f, 0.3f).As(WallType()), false);
				AddBox(At(3.f, 0.f), FaceSize(F.Normal, 3.f, WW, WH), FNHSurface(GlassDark, 0.06f, 0.2f, 0.f, bLit ? 14.f : 0.f, 1.f).As(ENHSurfaceType::Glass), false);
				if (Hash01(Seed, B, S + 7) < 0.55f) // a curtain drawn across part of the window
				{
					const FLinearColor Curtain = Bright[static_cast<int32>(Hash01(Seed, S, B + 3) * 8.f) % 8];
					AddBox(At(3.6f, 0.f, -WW * 0.3f), FaceSize(F.Normal, 1.f, WW * 0.38f, WH - 8.f), FNHSurface(Curtain, 0.9f, 0.f, 0.f, bLit ? 6.f : 0.f, 0.f).As(ENHSurfaceType::Fabric), false);
				}
				AddBox(At(6.f, -WH * 0.5f - 10.f), FaceSize(F.Normal, 12.f, WW + 24.f, 6.f), FNHSurface(Trim, 0.85f, 0.6f).As(WallType()), false);
				if (H < (bDusty ? 0.85f : 0.6f)) // burglar bars
				{
					const FNHSurface Bars = FNHSurface(Iron, 0.55f, 0.6f, 0.6f).As(ENHSurfaceType::Metal);
					const int32 N = FMath::Max(3, FMath::FloorToInt(WW / 14.f));
					for (int32 K = 0; K <= N; ++K)
					{
						AddBox(At(8.f, 0.f, -WW * 0.5f + WW * K / N), FaceSize(F.Normal, 2.2f, 2.2f, WH + 8.f), Bars, false);
					}
					for (float Up : { -WH / 6.f, WH / 6.f })
					{
						AddBox(At(8.f, Up), FaceSize(F.Normal, 2.2f, WW + 8.f, 2.2f), Bars, false);
					}
				}
			}
		}
	}
}

void ANHBlockoutBuilding::Balconies(float SH, int32 Storeys)
{
	using namespace NHBuild;
	const TArray<FFace> All = Faces();
	for (int32 FI = 0; FI < All.Num(); ++FI)
	{
		const FFace& F = All[FI];
		if (!(StreetFaces & static_cast<int32>(F.Flag)))
		{
			continue;
		}
		const int32 Bays = FMath::Max(1, FMath::FloorToInt(F.Length / 250.f));
		const float BW = F.Length / Bays, PW = BW - 30.f;
		if (PW < 160.f)
		{
			continue;
		}
		for (int32 S = 1; S < Storeys; ++S)
		{
			for (int32 B = 0; B < Bays; ++B)
			{
				if (Hash01(Seed + 5, S, B * 7 + FI) > (bDusty ? 0.45f : 0.3f))
				{
					continue;
				}
				const float Along = -F.Length * 0.5f + BW * (B + 0.5f), Z = S * SH;
				auto At = [&](float Out, float Up, float Side = 0.f) { return F.Normal * (F.Depth + Out) + F.Tangent * (Along + Side) + FVector(0, 0, Z + Up); };
				const FNHSurface Slab = FNHSurface(Scale(WallColor, 0.85f), 0.9f, 0.6f).As(WallType());
				AddBox(At(55.f, -8.f), FaceSize(F.Normal, 110.f, PW, 16.f), Slab);
				AddBox(At(106.f, 50.f), FaceSize(F.Normal, 8.f, PW, 100.f), FNHSurface(WallColor, 0.9f, 0.3f).As(WallType()));
				for (float Side : { -PW * 0.5f + 4.f, PW * 0.5f - 4.f })
				{
					AddBox(At(55.f, 50.f, Side), FaceSize(F.Normal, 110.f, 8.f, 100.f), FNHSurface(WallColor, 0.9f, 0.3f).As(WallType()));
				}
				if (Hash01(Seed, S, B + 40) < 0.6f) // laundry on a line
				{
					for (int32 K = 0; K < 4; ++K)
					{
						const FLinearColor Cloth = Bright[static_cast<int32>(Hash01(Seed, K, S + B) * 8.f) % 8];
						AddBox(At(70.f, 150.f, -PW * 0.35f + K * PW * 0.22f), FaceSize(F.Normal, 1.f, 40.f, 55.f), FNHSurface(Cloth, 0.95f, 0.f).As(ENHSurfaceType::Fabric), false);
					}
					AddBox(At(70.f, 180.f), FaceSize(F.Normal, 1.f, PW, 1.f), FNHSurface(Iron, 0.6f).As(ENHSurfaceType::Metal), false);
				}
			}
		}
	}
}

void ANHBlockoutBuilding::FlatRoof(float Z)
{
	using namespace NHBuild;
	AddBox(FVector(0, 0, Z + 7.f), FVector(Size.X + 10.f, Size.Y + 10.f, 14.f), FNHSurface(FLinearColor(0.2f, 0.19f, 0.18f), 0.9f, 1.f).As(ENHSurfaceType::Concrete));
	const FNHSurface Wall = FNHSurface(WallColor, 0.9f, 0.3f).As(WallType());
	AddBox(FVector(Size.X * 0.5f - 9.f, 0, Z + 59.f), FVector(18.f, Size.Y, 90.f), Wall);
	AddBox(FVector(-Size.X * 0.5f + 9.f, 0, Z + 59.f), FVector(18.f, Size.Y, 90.f), Wall);
	AddBox(FVector(0, Size.Y * 0.5f - 9.f, Z + 59.f), FVector(Size.X - 36.f, 18.f, 90.f), Wall);
	AddBox(FVector(0, -Size.Y * 0.5f + 9.f, Z + 59.f), FVector(Size.X - 36.f, 18.f, 90.f), Wall);
	if (bDusty) // column stubs with rebar sticking out, waiting for the next floor
	{
		const FNHSurface Stub = FNHSurface(RawConcrete, 0.95f, 0.6f).As(ENHSurfaceType::Concrete), Rebar = FNHSurface(Rust, 0.7f, 0.5f, 0.3f).As(ENHSurfaceType::Metal);
		for (float SX : { -0.5f, 0.f, 0.5f })
		{
			for (float SY : { -0.5f, 0.5f })
			{
				const FVector P(SX * (Size.X - 40.f), SY * (Size.Y - 40.f), Z + 14.f);
				AddBox(P + FVector(0, 0, 35.f), FVector(25.f, 25.f, 70.f), Stub);
				for (float DX : { -8.f, 8.f })
				{
					for (float DY : { -8.f, 8.f })
					{
						AddShape(ENHShape::Cylinder, P + FVector(DX, DY, 110.f), FVector(1.6f, 1.6f, 120.f), Rebar, false);
					}
				}
			}
		}
	}
}

void ANHBlockoutBuilding::ZincRoof(float Z, const FVector2D& Footprint, float Overhang)
{
	using namespace NHBuild;
	const bool bAlongX = Footprint.X >= Footprint.Y;
	const float Long = bAlongX ? Footprint.X : Footprint.Y, Short = bAlongX ? Footprint.Y : Footprint.X;
	const float A = FMath::DegreesToRadians(14.f), Half = Short * 0.5f + Overhang, Rise = FMath::Tan(A) * Short * 0.5f + 10.f;
	const bool bRust = bDusty || Hash01(Seed, 77) < 0.5f;
	const FNHSurface Zinc = FNHSurface(bRust ? Rust : ZincGrey, 0.55f, 1.f, 0.45f).As(ENHSurfaceType::Zinc);
	for (float Side : { 1.f, -1.f })
	{
		const float CenterAcross = Side * Half * 0.5f, CenterZ = Z + Rise - FMath::Tan(A) * Half * 0.5f;
		const FVector C = bAlongX ? FVector(0, CenterAcross, CenterZ) : FVector(CenterAcross, 0, CenterZ);
		// tilt each half so it falls away from the ridge: about X for a ridge along X, about Y otherwise
		const FQuat Q = bAlongX ? FQuat(FVector::XAxisVector, -Side * A) : FQuat(FVector::YAxisVector, Side * A);
		const FVector S = bAlongX ? FVector(Long + Overhang * 2.f, Half / FMath::Cos(A), 5.f) : FVector(Half / FMath::Cos(A), Long + Overhang * 2.f, 5.f);
		AddShapeTransform(ENHShape::Box, FTransform(Q, C, S / 100.f), Zinc, true, false);
	}
	// gable ends, roughly filled
	const FNHSurface Wall = FNHSurface(WallColor, 0.9f, 0.3f).As(WallType());
	for (float End : { 1.f, -1.f })
	{
		const FVector C = bAlongX ? FVector(End * (Long * 0.5f - 6.f), 0, Z + Rise * 0.35f) : FVector(0, End * (Long * 0.5f - 6.f), Z + Rise * 0.35f);
		AddBox(C, bAlongX ? FVector(12.f, Short * 0.5f, Rise * 0.7f) : FVector(Short * 0.5f, 12.f, Rise * 0.7f), Wall, false);
	}
}

void ANHBlockoutBuilding::ShopInterior(float SH)
{
	using namespace NHBuild;
	const FNHSurface Wall = FNHSurface(WallColor, 0.9f, 0.3f).As(WallType()), Inner = FNHSurface(FLinearColor(0.62f, 0.6f, 0.55f), 0.85f).As(ENHSurfaceType::Plaster);
	const float T = 20.f;
	for (const FFace& F : Faces())
	{
		const bool bDoor = (ShopFace & static_cast<int32>(F.Flag)) != 0;
		const FVector Mid = F.Normal * (F.Depth - T * 0.5f) + FVector(0, 0, SH * 0.5f);
		if (!bDoor)
		{
			AddBox(Mid, FaceSize(F.Normal, T, F.Length, SH), Wall);
			continue;
		}
		// wall pieces either side of the doorway, and a lintel over it
		const float DoorW = FMath::Clamp(ShopDoorWidth, 120.f, F.Length - 80.f), DoorH = FMath::Min(260.f, SH - 30.f);
		const float Off = FMath::Clamp(ShopDoorOffset, -F.Length * 0.5f + DoorW * 0.5f + 20.f, F.Length * 0.5f - DoorW * 0.5f - 20.f);
		const float L0 = -F.Length * 0.5f, L1 = Off - DoorW * 0.5f, R0 = Off + DoorW * 0.5f, R1 = F.Length * 0.5f;
		AddBox(Mid + F.Tangent * ((L0 + L1) * 0.5f), FaceSize(F.Normal, T, L1 - L0, SH), Wall);
		AddBox(Mid + F.Tangent * ((R0 + R1) * 0.5f), FaceSize(F.Normal, T, R1 - R0, SH), Wall);
		AddBox(F.Normal * (F.Depth - T * 0.5f) + F.Tangent * Off + FVector(0, 0, (DoorH + SH) * 0.5f), FaceSize(F.Normal, T, DoorW, SH - DoorH), Wall);

		// shelves on the back wall, a counter by the door
		const FVector Back = -F.Normal * (F.Depth - T - 25.f);
		const float BackLen = F.Length - 2.f * T - 20.f;
		for (int32 Shelf = 0; Shelf < 4; ++Shelf)
		{
			const float Z = 40.f + Shelf * 48.f;
			AddBox(Back + FVector(0, 0, Z), FaceSize(F.Normal, 45.f, BackLen, 4.f), FNHSurface(Timber, 0.8f).As(ENHSurfaceType::Wood));
			for (int32 K = 0; K < 10; ++K)
			{
				const float Along = -BackLen * 0.5f + BackLen * (K + 0.5f) / 10.f;
				const FLinearColor Goods = Bright[static_cast<int32>(Hash01(Seed, Shelf, K) * 8.f) % 8];
				const float GH = 18.f + 20.f * Hash01(Seed, K, Shelf + 3);
				AddBox(Back + F.Tangent * Along + FVector(0, 0, Z + 2.f + GH * 0.5f), FaceSize(F.Normal, 30.f, BackLen / 10.f - 6.f, GH), FNHSurface(Goods, 0.6f), false);
			}
		}
		AddBox(F.Normal * (F.Depth - T - 140.f) + F.Tangent * (Off + DoorW * 0.5f + 60.f) + FVector(0, 0, 47.f), FaceSize(F.Normal, 60.f, 150.f, 95.f), FNHSurface(Timber, 0.7f).As(ENHSurfaceType::Wood));
	}
	AddBox(FVector(0, 0, 2.f), FVector(Size.X - 2.f * T, Size.Y - 2.f * T, 4.f), FNHSurface(FLinearColor(0.5f, 0.47f, 0.42f), 0.35f).As(ENHSurfaceType::Concrete));
	AddBox(FVector(0, 0, SH - 12.f), FVector(Size.X - 2.f * T, Size.Y - 2.f * T, 24.f), Inner);
	// a strip light: an emissive panel that Lumen turns into the room's light at night (and a little by day)
	AddBox(FVector(0, 0, SH - 26.f), FVector(FMath::Min(240.f, Size.X * 0.5f), 12.f, 3.f), FNHSurface(FLinearColor(1.f, 0.95f, 0.85f), 0.5f, 0.f, 0.f, 120.f, 0.f), false);
}

// ---------------------------------------------------------------------------------------------------- towers
void ANHBlockoutBuilding::BuildTower()
{
	using namespace NHBuild;
	AddBox(FVector(0, 0, Height * 0.5f), FVector(Size.X, Size.Y, Height), FNHSurface(FLinearColor(0.07f, 0.1f, 0.13f), 0.08f, 0.3f, 0.6f).As(ENHSurfaceType::Glass));
	const float FH = 350.f;
	const int32 Floors = FMath::Max(1, FMath::FloorToInt(Height / FH));
	const TArray<FFace> All = Faces();
	for (int32 FI = 0; FI < All.Num(); ++FI)
	{
		const FFace& F = All[FI];
		const int32 Cols = FMath::Max(1, FMath::FloorToInt(F.Length / 300.f));
		const float CW = F.Length / Cols;
		for (int32 L = 1; L <= Floors; ++L) // spandrel bands
		{
			AddBox(F.Normal * (F.Depth + 3.f) + FVector(0, 0, L * FH - 35.f), FaceSize(F.Normal, 6.f, F.Length + 12.f, 70.f), FNHSurface(FLinearColor(0.5f, 0.51f, 0.52f), 0.6f, 0.3f).As(ENHSurfaceType::Concrete), false);
		}
		for (int32 C = 0; C <= Cols; ++C) // mullions
		{
			AddBox(F.Normal * (F.Depth + 4.f) + F.Tangent * (-F.Length * 0.5f + C * CW) + FVector(0, 0, Height * 0.5f), FaceSize(F.Normal, 8.f, 10.f, Height), FNHSurface(FLinearColor(0.55f, 0.57f, 0.6f), 0.35f, 0.3f, 0.8f).As(ENHSurfaceType::Metal), false);
		}
		for (int32 L = 0; L < Floors; ++L) // offices still lit at night
		{
			for (int32 C = 0; C < Cols; ++C)
			{
				if (Hash01(Seed, L * 17 + C, FI) < 0.35f)
				{
					AddBox(F.Normal * (F.Depth + 1.f) + F.Tangent * (-F.Length * 0.5f + (C + 0.5f) * CW) + FVector(0, 0, L * FH + 140.f), FaceSize(F.Normal, 2.f, CW - 20.f, 240.f),
						FNHSurface(FLinearColor(0.05f, 0.07f, 0.09f), 0.1f, 0.f, 0.f, 10.f, 0.5f).As(ENHSurfaceType::Glass), false);
				}
			}
		}
	}
	AddBox(FVector(0, 0, Height + 150.f), FVector(Size.X * 0.4f, Size.Y * 0.4f, 300.f), FNHSurface(FLinearColor(0.4f, 0.41f, 0.42f), 0.8f, 0.5f).As(ENHSurfaceType::Concrete)); // plant room
	AddBox(FVector(0, 0, Height + 10.f), FVector(Size.X + 12.f, Size.Y + 12.f, 20.f), FNHSurface(FLinearColor(0.5f, 0.51f, 0.52f), 0.6f, 0.3f).As(ENHSurfaceType::Concrete)); // roof edge
}

// ---------------------------------------------------------------------------------------------------- market and lagoon
void ANHBlockoutBuilding::BuildStall()
{
	using namespace NHBuild;
	const FNHSurface Wood = FNHSurface(Timber, 0.85f, 0.6f).As(ENHSurfaceType::Wood);
	const float HX = Size.X * 0.5f - 30.f, HY = Size.Y * 0.5f - 30.f;
	for (float SX : { -1.f, 1.f })
	{
		for (float SY : { -1.f, 1.f })
		{
			AddShape(ENHShape::Cylinder, FVector(SX * HX, SY * HY, 125.f), FVector(8.f, 8.f, 250.f), Wood, true);
		}
	}
	AddBox(FVector(0, 0, 85.f), FVector(Size.X - 60.f, Size.Y - 80.f, 8.f), Wood);
	for (int32 K = 0; K < 8; ++K)
	{
		const FLinearColor Goods = Bright[static_cast<int32>(Hash01(Seed, K) * 8.f) % 8];
		const FVector P((Hash01(Seed, K, 1) - 0.5f) * (Size.X - 120.f), (Hash01(Seed, K, 2) - 0.5f) * (Size.Y - 140.f), 89.f);
		const float S = 25.f + 25.f * Hash01(Seed, K, 3);
		AddShape(K % 3 == 0 ? ENHShape::Sphere : ENHShape::Box, P + FVector(0, 0, S * 0.4f), FVector(S, S, S * 0.8f), FNHSurface(Goods, 0.6f), false);
	}
	const bool bTarp = Hash01(Seed, 5) < 0.45f;
	AddBox(FVector(0, 0, 258.f), FVector(Size.X + 40.f, Size.Y + 40.f, 4.f), FNHSurface(bTarp ? Tarp : Rust, bTarp ? 0.6f : 0.55f, 1.f, bTarp ? 0.f : 0.45f).As(bTarp ? ENHSurfaceType::Tarp : ENHSurfaceType::Zinc), true, FRotator(0.f, 0.f, 4.f));
}

void ANHBlockoutBuilding::BuildStilt()
{
	using namespace NHBuild;
	const FNHSurface Wood = FNHSurface(Timber, 0.9f, 0.8f).As(ENHSurfaceType::Wood);
	for (float SX : { -1.f, 0.f, 1.f })
	{
		for (float SY : { -1.f, 1.f })
		{
			AddShape(ENHShape::Cylinder, FVector(SX * (Size.X * 0.5f - 20.f), SY * (Size.Y * 0.5f - 20.f), 0.f), FVector(14.f, 14.f, 320.f), Wood, true);
		}
	}
	AddBox(FVector(0, 0, 157.f), FVector(Size.X, Size.Y, 14.f), Wood);
	AddBox(FVector(0, 0, 165.f + 130.f), FVector(Size.X - 40.f, Size.Y - 40.f, 260.f), FNHSurface(FLinearColor(0.3f, 0.2f, 0.12f), 0.9f, 0.5f).As(ENHSurfaceType::Wood));
	AddBox(FVector(Size.X * 0.5f - 18.f, 0, 165.f + 100.f), FVector(4.f, 90.f, 200.f), FNHSurface(FLinearColor(0.05f, 0.04f, 0.03f), 0.9f).As(ENHSurfaceType::Wood), false);
	ZincRoof(165.f + 260.f, FVector2D(Size.X - 40.f, Size.Y - 40.f), 30.f);
}

// ---------------------------------------------------------------------------------------------------- street furniture
void ANHBlockoutBuilding::BuildFuelStation()
{
	const FNHSurface White = FNHSurface(FLinearColor(0.8f, 0.8f, 0.78f), 0.5f, 0.3f).As(ENHSurfaceType::Metal), Green = FNHSurface(FLinearColor(0.02f, 0.3f, 0.12f), 0.5f, 0.3f).As(ENHSurfaceType::Metal);
	const FVector2D C(Size.X - 100.f, Size.Y - 100.f);
	AddBox(FVector(0, 0, 480.f), FVector(C.X, C.Y, 50.f), White);
	AddBox(FVector(0, 0, 470.f), FVector(C.X + 10.f, C.Y + 10.f, 30.f), Green, false);
	AddBox(FVector(0, 0, 453.f), FVector(C.X * 0.8f, 20.f, 3.f), FNHSurface(FLinearColor(1.f, 1.f, 1.f), 0.5f, 0.f, 0.f, 90.f, 0.f), false); // canopy lights
	for (float SX : { -0.35f, 0.35f })
	{
		for (float SY : { -0.3f, 0.3f })
		{
			AddShape(ENHShape::Cylinder, FVector(SX * C.X, SY * C.Y, 228.f), FVector(40.f, 40.f, 456.f), White, true);
		}
		AddBox(FVector(SX * C.X, 0, 80.f), FVector(60.f, 40.f, 160.f), Green);
		AddBox(FVector(SX * C.X, 21.f, 120.f), FVector(30.f, 2.f, 25.f), FNHSurface(FLinearColor(0.1f, 0.8f, 0.4f), 0.3f, 0.f, 0.f, 8.f, 0.f), false);
	}
	AddBox(FVector(0, -Size.Y * 0.5f + 140.f, 140.f), FVector(320.f, 250.f, 280.f), White); // kiosk
}

void ANHBlockoutBuilding::BuildBusShelter()
{
	const FNHSurface Yellow = FNHSurface(FLinearColor(0.75f, 0.5f, 0.0f), 0.5f, 0.5f, 0.2f).As(ENHSurfaceType::Metal), Dark = FNHSurface(FLinearColor(0.04f, 0.04f, 0.04f), 0.5f, 0.6f, 0.5f).As(ENHSurfaceType::Metal);
	AddBox(FVector(0, 0, 262.f), FVector(170.f, 380.f, 8.f), Yellow, true, FRotator(-3.f, 0.f, 0.f));
	AddBox(FVector(-80.f, 0, 150.f), FVector(4.f, 360.f, 190.f), FNHSurface(FLinearColor(0.2f, 0.25f, 0.28f), 0.1f, 0.3f).As(ENHSurfaceType::Glass), true);
	for (float Y : { -175.f, 175.f })
	{
		for (float X : { -75.f, 70.f })
		{
			AddShape(ENHShape::Cylinder, FVector(X, Y, 130.f), FVector(8.f, 8.f, 260.f), Dark, true);
		}
	}
	AddBox(FVector(-55.f, 0, 46.f), FVector(40.f, 300.f, 6.f), Dark);
}

void ANHBlockoutBuilding::BuildFootbridge()
{
	const FNHSurface Deck = FNHSurface(FLinearColor(0.35f, 0.34f, 0.32f), 0.85f, 0.8f).As(ENHSurfaceType::Concrete), Rail = FNHSurface(FLinearColor(0.12f, 0.2f, 0.12f), 0.5f, 0.6f, 0.5f).As(ENHSurfaceType::Metal);
	const float DeckZ = 550.f, W = Size.Y, Span = Size.X;
	AddBox(FVector(0, 0, DeckZ - 15.f), FVector(Span, W, 30.f), Deck);
	for (float Side : { -1.f, 1.f })
	{
		AddBox(FVector(0, Side * (W * 0.5f - 3.f), DeckZ + 55.f), FVector(Span, 6.f, 110.f), Rail);
	}
	// a landing at each end, then a 30° ramp down along +Y (the pavement beside the road)
	const float A = FMath::DegreesToRadians(30.f), RampLen = DeckZ / FMath::Sin(A), RampRun = DeckZ / FMath::Tan(A);
	for (float End : { -1.f, 1.f })
	{
		const float X = End * (Span * 0.5f - W * 0.5f);
		AddShape(ENHShape::Cylinder, FVector(X, 0, (DeckZ - 30.f) * 0.5f), FVector(50.f, 50.f, DeckZ - 30.f), Deck, true);
		const FVector C(X, W * 0.5f + RampRun * 0.5f, DeckZ * 0.5f - 15.f);
		// local +Y has to point down-slope: (0, cos A, -sin A), a rotation of -A about X
		const FQuat Q(FVector::XAxisVector, -A);
		AddShapeTransform(ENHShape::Box, FTransform(Q, C, FVector(W, RampLen, 25.f) / 100.f), Deck, true, false);
		for (float Side : { -1.f, 1.f })
		{
			AddShapeTransform(ENHShape::Box, FTransform(Q, C + FVector(Side * (W * 0.5f - 3.f), 0, 60.f), FVector(6.f, RampLen, 110.f) / 100.f), Rail, true, false);
		}
	}
}
