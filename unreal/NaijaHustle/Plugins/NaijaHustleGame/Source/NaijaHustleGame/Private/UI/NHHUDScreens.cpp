// The HUD's own screens: the map with its pin, the pause menu and the inventory wheel (see NHHUD.h)
#include "UI/NHHUD.h"

#include "Core/NHGameData.h"
#include "Core/NHHustleSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/NHGameDirector.h"
#include "Kismet/GameplayStatics.h"
#include "Lighting/NHLightingRig.h"
#include "Misc/ConfigCacheIni.h"
#include "Player/NHCharacter.h"
#include "Player/NHPlayerController.h"
#include "Vehicles/NHTraffic.h"
#include "Vehicles/NHVehicle.h"

namespace NHScreens
{
	const FLinearColor Ink(0.96f, 0.95f, 0.9f);
	const FLinearColor Muted(0.7f, 0.68f, 0.62f);
	const FLinearColor Yellow(1.f, 0.77f, 0.f);
	const FLinearColor PinBlue(0.2f, 0.75f, 1.f);
	const FLinearColor Land(0.2f, 0.19f, 0.16f);
	const TCHAR* Section = TEXT("NaijaHustle");

	enum { Resume, Character, Lighting, Traffic, Look, Resolution, Minimap, Quit, Lines };
	const TCHAR* LineNames[] = { TEXT("Resume"), TEXT("Character"), TEXT("Lighting"), TEXT("Traffic"), TEXT("Look speed"), TEXT("Resolution"), TEXT("Minimap"), TEXT("Quit game") };
	const TCHAR* TrafficNames[] = { TEXT("None"), TEXT("Light"), TEXT("Normal"), TEXT("Heavy") };
	const TCHAR* PresetNames[] = { TEXT("Day"), TEXT("Dusty noon"), TEXT("Sunset"), TEXT("Night rain"), TEXT("Harsh morning"), TEXT("Golden evening") };

	enum { Phone, Wardrobe, CarKeys, Torch, Wallet, Hail, Slots };
	const TCHAR* SlotNames[] = { TEXT("PHONE"), TEXT("WARDROBE"), TEXT("CAR KEYS"), TEXT("TORCH"), TEXT("WALLET"), TEXT("HAIL") };
	const TCHAR* SlotHints[] = { TEXT("Open the map"), TEXT("Next character"), TEXT("Find my car"), TEXT("Light on or off"), TEXT("What I have"), TEXT("Stop a ride") };

	FColor RoadColor(uint8 Class)
	{
		return Class <= 1 ? FColor(217, 158, 51) : Class == 2 ? FColor(204, 199, 178) : FColor(140, 138, 128);
	}
}

// ---------------------------------------------------------------------------------------------- opening, closing
void ANHHUD::Open(EScreen NewScreen)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || NewScreen == Screen)
	{
		return;
	}
	if (Screen == EScreen::None)
	{
		PC->SetIgnoreLookInput(true);
		PC->SetIgnoreMoveInput(true);
	}
	Screen = NewScreen;
	if (Screen == EScreen::None)
	{
		PC->ResetIgnoreLookInput();
		PC->ResetIgnoreMoveInput();
	}
	// the map and the menu stop the game; the wheel slows it to a quarter while you choose
	PC->SetPause(Screen == EScreen::Map || Screen == EScreen::Menu);
	UGameplayStatics::SetGlobalTimeDilation(this, Screen == EScreen::Wheel ? 0.25f : 1.f);
	PC->bShowMouseCursor = Screen == EScreen::Map || Screen == EScreen::Wheel;
	if (Screen == EScreen::Wheel && Canvas)
	{
		PC->SetMouseLocation(FMath::RoundToInt(Canvas->ClipX * 0.5f), FMath::RoundToInt(Canvas->ClipY * 0.5f));
	}
}

void ANHHUD::ToggleMap()
{
	if (Screen == EScreen::Map)
	{
		Open(EScreen::None);
	}
	else if (Screen != EScreen::Wheel)
	{
		const APawn* Pawn = GetOwningPawn();
		MapCentre = Pawn ? FVector2D(Pawn->GetActorLocation()) : CityCorner + FVector2D(CitySpan * 0.5f);
		MapZoom = 4.f;
		Open(EScreen::Map);
	}
}

void ANHHUD::ToggleMenu()
{
	if (Screen == EScreen::None)
	{
		MenuLine = 0;
		Open(EScreen::Menu);
	}
	else if (Screen != EScreen::Wheel)
	{
		Open(EScreen::None); // Esc also closes the map
	}
}

void ANHHUD::SetWheel(bool bOpen)
{
	if (bOpen && Screen == EScreen::None)
	{
		WheelSlot = -1;
		Open(EScreen::Wheel);
	}
	else if (!bOpen && Screen == EScreen::Wheel)
	{
		const int32 Slot = WheelSlot;
		Open(EScreen::None);
		UseWheel(Slot);
	}
}

void ANHHUD::Nav(int32 DX, int32 DY)
{
	if (Screen == EScreen::Menu)
	{
		MenuLine = (MenuLine + DY + NHScreens::Lines) % NHScreens::Lines;
		if (DX != 0)
		{
			MenuChange(MenuLine, DX);
		}
	}
	else if (Screen == EScreen::Map)
	{
		MapCentre += FVector2D(DX, DY) * MapSpan * 0.15f;
	}
	else if (Screen == EScreen::Wheel)
	{
		WheelSlot = ((WheelSlot < 0 ? 0 : WheelSlot + DX + DY) + NHScreens::Slots) % NHScreens::Slots;
	}
}

void ANHHUD::Accept()
{
	if (Screen == EScreen::Menu)
	{
		if (MenuLine == NHScreens::Resume)
		{
			Open(EScreen::None);
		}
		else if (MenuLine == NHScreens::Quit)
		{
			GetOwningPlayerController()->ConsoleCommand(TEXT("quit"));
		}
		else
		{
			MenuChange(MenuLine, 1);
		}
	}
}

void ANHHUD::Zoom(int32 Dir)
{
	if (Screen == EScreen::Map)
	{
		MapZoom = FMath::Clamp(MapZoom * (Dir > 0 ? 1.5f : 1.f / 1.5f), 1.f, 40.f);
	}
}

void ANHHUD::Click(bool bRight)
{
	APlayerController* PC = GetOwningPlayerController();
	float MouseX = 0.f, MouseY = 0.f;
	if (Screen != EScreen::Map || !PC || !PC->GetMousePosition(MouseX, MouseY))
	{
		return;
	}
	if (bRight)
	{
		if (bHasPin)
		{
			bHasPin = false;
			Toast(this, TEXT("Pin cleared"), 0);
		}
		return;
	}
	const FVector2D In((MouseX - MapAt.X) / MapSide, (MouseY - MapAt.Y) / MapSide);
	if (In.X < 0.f || In.X > 1.f || In.Y < 0.f || In.Y > 1.f)
	{
		return;
	}
	const FVector2D World = MapCentre + (In - FVector2D(0.5f)) * MapSpan;
	// on or beside a stop: pin the stop itself and call it by name; otherwise the place, named by its district
	const UNHGameData* Data = UNHGameData::Get(this);
	FString Label = Data ? Data->DistrictAt(FVector(World, 0.f)) : FString();
	FVector2D At = World;
	if (Data)
	{
		for (const TPair<FName, FNHBusStop>& Stop : Data->Stops)
		{
			if (FVector2D::Distance(Stop.Value.Kerb, World) < MapSpan * 0.02f)
			{
				At = Stop.Value.Kerb;
				Label = Stop.Value.Name + TEXT(" stop");
			}
		}
	}
	SetPin(At, Label.IsEmpty() ? TEXT("the pin") : Label);
}

void ANHHUD::SetPin(const FVector2D& World, const FString& Label)
{
	bHasPin = true;
	Pin = World;
	PinLabel = Label;
	Toast(this, FString::Printf(TEXT("Pinned: %s"), *Label), 1);
}

// ------------------------------------------------------------------------------------------------------ the map
void ANHHUD::BuildCityMap()
{
	const UNHGameData* Data = UNHGameData::Get(this);
	if (!Data || !Data->bRealCity || Data->RoadNodes.Num() == 0)
	{
		return;
	}
	FBox2D Box(ForceInit);
	for (const FVector2D& N : Data->RoadNodes)
	{
		Box += N;
	}
	CitySpan = FMath::Max(Box.GetSize().X, Box.GetSize().Y) * 1.04f;
	CityCorner = Box.GetCenter() - FVector2D(CitySpan * 0.5f);
	const int32 Size = 2048;
	CityTex = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
	if (!CityTex)
	{
		return;
	}
	CityTex->AddressX = TA_Clamp;
	CityTex->AddressY = TA_Clamp;
	FColor* Px = static_cast<FColor*>(CityTex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
	const FColor Ground = NHScreens::Land.ToFColor(true);
	for (int32 I = 0; I < Size * Size; ++I)
	{
		Px[I] = Ground;
	}
	// minor roads first, so the main ones are drawn over them
	for (int32 Pass = 5; Pass >= 0; --Pass)
	{
		for (const FNHRoadWay& Way : Data->RoadWays)
		{
			if (Way.Class != Pass)
			{
				continue;
			}
			const FColor Color = NHScreens::RoadColor(Way.Class);
			const int32 Wide = Way.Class <= 2 ? 1 : 0; // pixels either side
			for (int32 I = 0; I + 1 < Way.Nodes.Num(); ++I)
			{
				const FVector2D A = (Data->RoadNodes[Way.Nodes[I]] - CityCorner) / CitySpan * Size, B = (Data->RoadNodes[Way.Nodes[I + 1]] - CityCorner) / CitySpan * Size;
				const int32 Steps = FMath::Max(1, FMath::CeilToInt(FMath::Max(FMath::Abs(B.X - A.X), FMath::Abs(B.Y - A.Y))));
				for (int32 T = 0; T <= Steps; ++T)
				{
					const FVector2D P = FMath::Lerp(A, B, static_cast<float>(T) / Steps);
					for (int32 DX = -Wide; DX <= Wide; ++DX)
					{
						for (int32 DY = -Wide; DY <= Wide; ++DY)
						{
							const int32 X = FMath::RoundToInt(P.X) + DX, Y = FMath::RoundToInt(P.Y) + DY;
							if (X > 1 && Y > 1 && X < Size - 2 && Y < Size - 2) // the rim stays ground, which is what clamping repeats
							{
								Px[Y * Size + X] = Color;
							}
						}
					}
				}
			}
		}
	}
	CityTex->GetPlatformData()->Mips[0].BulkData.Unlock();
	CityTex->UpdateResource();
}

void ANHHUD::DrawRoads(float X, float Y, float Size, const FVector2D& Corner, float Span, float Thick)
{
	const UNHGameData* Data = UNHGameData::Get(this);
	DrawRect(NHScreens::Land, X, Y, Size, Size);
	TArray<FNHRoadSeg> Near;
	Data->RoadsNear(Corner + FVector2D(Span * 0.5f), Span * 0.71f, Near);
	for (int32 Pass = 5; Pass >= 0; --Pass) // widest last
	{
		for (const FNHRoadSeg& Seg : Near)
		{
			const FNHRoadWay& Way = Data->RoadWays[Seg.Way];
			if (Way.Class != Pass)
			{
				continue;
			}
			// clip the segment to the square (Liang-Barsky)
			const FVector2D A = (Data->RoadNodes[Way.Nodes[Seg.Index]] - Corner) / Span, D = (Data->RoadNodes[Way.Nodes[Seg.Index + 1]] - Corner) / Span - A;
			float T0 = 0.f, T1 = 1.f;
			bool bSeen = true;
			for (int32 Edge = 0; Edge < 4 && bSeen; ++Edge)
			{
				const float P = Edge == 0 ? -D.X : Edge == 1 ? D.X : Edge == 2 ? -D.Y : D.Y;
				const float Q = Edge == 0 ? A.X : Edge == 1 ? 1.f - A.X : Edge == 2 ? A.Y : 1.f - A.Y;
				if (FMath::IsNearlyZero(P))
				{
					bSeen = Q >= 0.f;
				}
				else if (P < 0.f)
				{
					T0 = FMath::Max(T0, Q / P);
				}
				else
				{
					T1 = FMath::Min(T1, Q / P);
				}
			}
			if (bSeen && T0 < T1)
			{
				const FVector2D From = A + D * T0, To = A + D * T1;
				DrawLine(X + From.X * Size, Y + From.Y * Size, X + To.X * Size, Y + To.Y * Size, FLinearColor(NHScreens::RoadColor(Way.Class)),
					(Way.Class <= 1 ? 5.f : Way.Class == 2 ? 4.f : 2.5f) * Thick * S);
			}
		}
	}
}

void ANHHUD::DrawMapScreen(float VW, float VH)
{
	using namespace NHScreens;
	const UNHGameData* Data = UNHGameData::Get(this);
	APlayerController* PC = GetOwningPlayerController();
	const APawn* Pawn = GetOwningPawn();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Large = GEngine->GetLargeFont();
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.82f), 0.f, 0.f, VW, VH);
	if (!Data || !Data->bRealCity || !CityTex)
	{
		Text(TEXT("This level has no city map: the minimap shows all of it."), VW * 0.5f, VH * 0.5f, Ink, Medium, 1.3f, true);
		Text(TEXT("M  close"), VW * 0.5f, VH * 0.5f + 40.f * S, Muted, Medium, 1.f, true);
		return;
	}
	MapSide = VH * 0.86f;
	MapAt = FVector2D((VW - MapSide) * 0.5f, VH * 0.045f);
	MapSpan = CitySpan / MapZoom;
	// keep the view on the city
	const FVector2D Lo = CityCorner + FVector2D(MapSpan * 0.5f), Hi = CityCorner + FVector2D(CitySpan - MapSpan * 0.5f);
	MapCentre = FVector2D(FMath::Clamp(MapCentre.X, Lo.X, FMath::Max(Lo.X, Hi.X)), FMath::Clamp(MapCentre.Y, Lo.Y, FMath::Max(Lo.Y, Hi.Y)));
	const FVector2D Corner = MapCentre - FVector2D(MapSpan * 0.5f);
	if (MapSpan <= 500000.f)
	{
		DrawRoads(MapAt.X, MapAt.Y, MapSide, Corner, MapSpan, 1.f); // close in: the roads themselves, sharp at any zoom
	}
	else
	{
		const FVector2D UV = (Corner - CityCorner) / CitySpan;
		DrawTexture(CityTex, MapAt.X, MapAt.Y, MapSide, MapSide, UV.X, UV.Y, MapSpan / CitySpan, MapSpan / CitySpan, FLinearColor::White, BLEND_Opaque);
	}
	const auto ToScreen = [&](const FVector2D& W, FVector2D& Out)
	{
		const FVector2D In = (W - Corner) / MapSpan;
		Out = MapAt + In * MapSide;
		return In.X >= 0.f && In.X <= 1.f && In.Y >= 0.f && In.Y <= 1.f;
	};
	FVector2D P;
	for (const TPair<FName, FNHBusStop>& Stop : Data->Stops)
	{
		if (ToScreen(Stop.Value.Kerb, P))
		{
			DrawRect(Yellow, P.X - 5.f * S, P.Y - 5.f * S, 10.f * S, 10.f * S);
			Text(Stop.Value.Name, P.X + 10.f * S, P.Y - 10.f * S, Ink, Medium, 0.9f);
		}
	}
	if (const ANHGameDirector* Dir = ANHGameDirector::Get(this); Dir && Dir->bMarker && ToScreen(FVector2D(Dir->Marker), P))
	{
		DrawRect(FLinearColor(1.f, 0.2f, 0.2f), P.X - 7.f * S, P.Y - 7.f * S, 14.f * S, 14.f * S);
		Text(TEXT("Job"), P.X + 12.f * S, P.Y - 10.f * S, FLinearColor(1.f, 0.5f, 0.45f), Medium, 0.9f);
	}
	const ANHPlayerController* NHPC = Cast<ANHPlayerController>(PC);
	if (const ANHVehicle* Car = NHPC ? NHPC->GetLastVehicle() : nullptr; Car && Car != Pawn && ToScreen(FVector2D(Car->GetActorLocation()), P))
	{
		DrawRect(FLinearColor(0.4f, 0.9f, 0.5f), P.X - 5.f * S, P.Y - 5.f * S, 10.f * S, 10.f * S);
		Text(TEXT("My car"), P.X + 10.f * S, P.Y - 10.f * S, FLinearColor(0.4f, 0.9f, 0.5f), Medium, 0.9f);
	}
	if (bHasPin && ToScreen(Pin, P))
	{
		DrawRect(PinBlue, P.X - 8.f * S, P.Y - 8.f * S, 16.f * S, 16.f * S);
		Text(PinLabel, P.X + 13.f * S, P.Y - 10.f * S, PinBlue, Medium, 1.f);
	}
	if (Pawn && ToScreen(FVector2D(Pawn->GetActorLocation()), P))
	{
		const float A = FMath::DegreesToRadians(Pawn->GetActorRotation().Yaw);
		DrawLine(P.X, P.Y, P.X + FMath::Cos(A) * 18.f * S, P.Y + FMath::Sin(A) * 18.f * S, FLinearColor::White, 3.f * S);
		DrawRect(FLinearColor::White, P.X - 6.f * S, P.Y - 6.f * S, 12.f * S, 12.f * S);
		Text(TEXT("You"), P.X + 12.f * S, P.Y + 4.f * S, FLinearColor::White, Medium, 0.9f);
	}
	// what is under the cursor, and how far away
	FString Under = TEXT("LAGOS");
	float MouseX = 0.f, MouseY = 0.f;
	if (PC && PC->GetMousePosition(MouseX, MouseY) && MouseX >= MapAt.X && MouseX <= MapAt.X + MapSide && MouseY >= MapAt.Y && MouseY <= MapAt.Y + MapSide)
	{
		const FVector2D World = Corner + FVector2D((MouseX - MapAt.X) / MapSide, (MouseY - MapAt.Y) / MapSide) * MapSpan;
		Under = Data->DistrictAt(FVector(World, 0.f)).ToUpper();
		if (Pawn)
		{
			Under += FString::Printf(TEXT("   %.1f km from you"), FVector2D::Distance(World, FVector2D(Pawn->GetActorLocation())) / 100000.f);
		}
	}
	Text(Under, VW * 0.5f, MapAt.Y + MapSide + 8.f * S, Yellow, Large, 1.f, true);
	Text(FString::Printf(TEXT("Click: pin a place     Right-click: clear the pin     Wheel: zoom (%.0f km across)     Arrows: move     M: close"), MapSpan / 100000.f),
		VW * 0.5f, MapAt.Y + MapSide + 44.f * S, Muted, Medium, 0.9f, true);
	float TW = 0.f, TH = 0.f;
	GetTextSize(TEXT("(c) OpenStreetMap contributors"), TW, TH, Medium, 0.8f * S);
	Text(TEXT("(c) OpenStreetMap contributors"), MapAt.X + MapSide - TW - 6.f * S, MapAt.Y + MapSide - TH - 6.f * S, Muted, Medium, 0.8f);
}

// ------------------------------------------------------------------------------------------------- the pause menu
FString ANHHUD::MenuValue(int32 Line) const
{
	using namespace NHScreens;
	const ANHPlayerController* PC = Cast<ANHPlayerController>(GetOwningPlayerController());
	switch (Line)
	{
	case Character:
	{
		const ANHCharacter* Char = PC ? PC->GetOnFootCharacter() : nullptr;
		const FString Name = Char ? Char->SkinName() : FString();
		return Name.IsEmpty() ? TEXT("(none in this project)") : Name;
	}
	case Lighting:
	{
		const ANHLightingRig* Rig = ANHLightingRig::Find(this);
		return Rig ? PresetNames[FMath::Clamp(static_cast<int32>(Rig->Preset), 0, 5)] : TEXT("(no lighting rig)");
	}
	case Traffic: return ANHTraffic::Get(this) ? TrafficNames[TrafficLevel] : TEXT("(not in this level)");
	case Look: return FString::Printf(TEXT("%d%%"), FMath::RoundToInt((PC ? PC->LookScale : 1.f) * 100.f));
	case Resolution: return FString::Printf(TEXT("%d%%"), ScreenPercent);
	case Minimap: return bShowMinimap ? TEXT("On") : TEXT("Off");
	default: return FString();
	}
}

void ANHHUD::MenuChange(int32 Line, int32 Dir)
{
	using namespace NHScreens;
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetOwningPlayerController());
	if (!PC)
	{
		return;
	}
	switch (Line)
	{
	case Character:
		if (ANHCharacter* Char = PC->GetOnFootCharacter())
		{
			Char->WearNextSkin(Dir);
		}
		break;
	case Lighting:
		if (ANHLightingRig* Rig = ANHLightingRig::Find(this))
		{
			Rig->ApplyPreset(static_cast<ENHLightingPreset>((static_cast<int32>(Rig->Preset) + Dir + 6) % 6));
			if (ANHGameDirector* Director = ANHGameDirector::Get(this))
			{
				Director->SetManualLighting(); // or the clock would put its own preset back
			}
		}
		break;
	case Traffic: TrafficLevel = (TrafficLevel + Dir + 4) % 4; break;
	case Look: PC->LookScale = FMath::Clamp(PC->LookScale + 0.1f * Dir, 0.3f, 2.5f); break;
	case Resolution: ScreenPercent = FMath::Clamp(ScreenPercent + 10 * Dir, 50, 100); break;
	case Minimap: bShowMinimap = !bShowMinimap; break;
	default: return;
	}
	ApplySettings();
	SaveSettings();
}

void ANHHUD::LoadSettings()
{
	GConfig->GetInt(NHScreens::Section, TEXT("Traffic"), TrafficLevel, GGameUserSettingsIni);
	GConfig->GetInt(NHScreens::Section, TEXT("ScreenPercent"), ScreenPercent, GGameUserSettingsIni);
	GConfig->GetBool(NHScreens::Section, TEXT("Minimap"), bShowMinimap, GGameUserSettingsIni);
	TrafficLevel = FMath::Clamp(TrafficLevel, 0, 3);
	ScreenPercent = FMath::Clamp(ScreenPercent, 50, 100);
}

void ANHHUD::SaveSettings() const
{
	const ANHPlayerController* PC = Cast<ANHPlayerController>(GetOwningPlayerController());
	GConfig->SetInt(NHScreens::Section, TEXT("Traffic"), TrafficLevel, GGameUserSettingsIni);
	GConfig->SetInt(NHScreens::Section, TEXT("ScreenPercent"), ScreenPercent, GGameUserSettingsIni);
	GConfig->SetBool(NHScreens::Section, TEXT("Minimap"), bShowMinimap, GGameUserSettingsIni);
	GConfig->SetFloat(NHScreens::Section, TEXT("LookScale"), PC ? PC->LookScale : 1.f, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void ANHHUD::ApplySettings()
{
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetOwningPlayerController());
	if (!PC)
	{
		return;
	}
	if (!bSettingsApplied)
	{
		bSettingsApplied = true;
		float Saved = 1.f;
		if (GConfig->GetFloat(NHScreens::Section, TEXT("LookScale"), Saved, GGameUserSettingsIni))
		{
			PC->LookScale = FMath::Clamp(Saved, 0.3f, 2.5f);
		}
	}
	if (ANHTraffic* Cars = ANHTraffic::Get(this))
	{
		Cars->SetDensity(TrafficLevel);
	}
	PC->ConsoleCommand(FString::Printf(TEXT("r.ScreenPercentage %d"), ScreenPercent));
}

void ANHHUD::DrawMenu(float VW, float VH)
{
	using namespace NHScreens;
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Large = GEngine->GetLargeFont();
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), 0.f, 0.f, VW, VH);
	const float W = 760.f * S, RowH = 58.f * S, H = 150.f * S + RowH * Lines, X = (VW - W) * 0.5f, Y = (VH - H) * 0.5f;
	Panel(X, Y, W, H, FLinearColor(0.05f, 0.05f, 0.05f, 0.94f));
	DrawRect(Yellow, X, Y, W, 5.f * S);
	Text(TEXT("NAIJA HUSTLE"), X + 30.f * S, Y + 22.f * S, Yellow, Large, 1.3f);
	Text(TEXT("Paused"), X + W - 110.f * S, Y + 30.f * S, Muted, Medium, 1.f);
	float Yy = Y + 92.f * S;
	for (int32 Line = 0; Line < Lines; ++Line)
	{
		const bool bOn = Line == MenuLine;
		Panel(X + 22.f * S, Yy, W - 44.f * S, RowH - 8.f * S, bOn ? FLinearColor(1.f, 0.77f, 0.f, 0.22f) : FLinearColor(1.f, 1.f, 1.f, 0.06f));
		if (bOn)
		{
			DrawRect(Yellow, X + 22.f * S, Yy, 5.f * S, RowH - 8.f * S);
		}
		Text(LineNames[Line], X + 44.f * S, Yy + 12.f * S, bOn ? Yellow : Ink, Medium, 1.2f);
		const FString Value = MenuValue(Line);
		if (!Value.IsEmpty())
		{
			float TW = 0.f, TH = 0.f;
			const FString Shown = bOn ? FString::Printf(TEXT("<   %s   >"), *Value) : Value;
			GetTextSize(Shown, TW, TH, Medium, 1.2f * S);
			Text(Shown, X + W - 44.f * S - TW, Yy + 12.f * S, bOn ? Ink : Muted, Medium, 1.2f);
		}
		Yy += RowH;
	}
	Text(TEXT("Up / Down: choose     Left / Right: change     Enter: select     Esc: back to the game"), VW * 0.5f, Y + H - 40.f * S, Muted, Medium, 0.95f, true);
}

// ------------------------------------------------------------------------------------------- the inventory wheel
void ANHHUD::UseWheel(int32 Slot)
{
	using namespace NHScreens;
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetOwningPlayerController());
	APawn* Pawn = GetOwningPawn();
	if (!PC || !Pawn)
	{
		return;
	}
	switch (Slot)
	{
	case Phone:
		ToggleMap();
		break;
	case Wardrobe:
		if (ANHCharacter* Char = PC->GetOnFootCharacter())
		{
			const FString Name = Char->WearNextSkin();
			Toast(this, Name.IsEmpty() ? FString(TEXT("Nothing else to wear in this project")) : FString::Printf(TEXT("Now: %s"), *Name), 1);
		}
		break;
	case CarKeys:
		if (const ANHVehicle* Car = PC->GetLastVehicle(); Car && Car != Pawn)
		{
			SetPin(FVector2D(Car->GetActorLocation()), TEXT("your ") + Car->DisplayName());
		}
		else
		{
			Toast(this, Car ? TEXT("You are sitting in it") : TEXT("You have not driven anything yet"), 0);
		}
		break;
	case Torch:
		if (ANHVehicle* Car = Cast<ANHVehicle>(Pawn))
		{
			Car->SetHeadlights(!Car->HeadlightsOn());
		}
		else if (ANHCharacter* Char = Cast<ANHCharacter>(Pawn))
		{
			Char->ToggleTorch();
			Toast(this, Char->TorchOn() ? TEXT("Torch on") : TEXT("Torch off"), 0);
		}
		break;
	case Wallet:
		if (const UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this))
		{
			Toast(this, FString::Printf(TEXT("Wallet: %s   Cred %d   Jobs done %d"), *UNHHustleSubsystem::Naira(Hustle->Cash), Hustle->Cred, Hustle->Jobs), 1);
		}
		break;
	case Hail:
		if (ANHTraffic* Cars = ANHTraffic::Get(this))
		{
			const FString Name = Cast<ANHVehicle>(Pawn) ? FString() : Cars->Hail(Pawn->GetActorLocation());
			Toast(this, Name.IsEmpty() ? FString(TEXT("Nobody near enough to flag down")) : FString::Printf(TEXT("The %s is pulling up for you"), *Name), Name.IsEmpty() ? 0 : 1);
		}
		else
		{
			Toast(this, TEXT("No traffic to flag down here"), 0);
		}
		break;
	default:
		break;
	}
}

void ANHHUD::DrawWheel(float VW, float VH)
{
	using namespace NHScreens;
	UFont* Medium = GEngine->GetMediumFont();
	APlayerController* PC = GetOwningPlayerController();
	const FVector2D Centre(VW * 0.5f, VH * 0.5f);
	const float Radius = 250.f * S;
	// point with the mouse: past the middle of the wheel, the slot it points toward
	float MouseX = 0.f, MouseY = 0.f;
	if (PC && PC->GetMousePosition(MouseX, MouseY))
	{
		const FVector2D Aim = FVector2D(MouseX, MouseY) - Centre;
		if (Aim.Size() > 60.f * S)
		{
			const float Turn = FMath::RadiansToDegrees(FMath::Atan2(Aim.X, -Aim.Y)); // 0 up, clockwise
			WheelSlot = FMath::RoundToInt(FMath::Fmod(Turn + 360.f, 360.f) / (360.f / Slots)) % Slots;
		}
	}
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), 0.f, 0.f, VW, VH);
	for (int32 Slot = 0; Slot < Slots; ++Slot)
	{
		const float A = FMath::DegreesToRadians(Slot * 360.f / Slots);
		const FVector2D At = Centre + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * Radius;
		const bool bOn = Slot == WheelSlot;
		const float W = 250.f * S, H = 92.f * S;
		Panel(At.X - W * 0.5f, At.Y - H * 0.5f, W, H, bOn ? FLinearColor(1.f, 0.77f, 0.f, 0.9f) : FLinearColor(0.05f, 0.05f, 0.05f, 0.88f));
		DrawLine(Centre.X + FMath::Sin(A) * 50.f * S, Centre.Y - FMath::Cos(A) * 50.f * S, Centre.X + FMath::Sin(A) * (Radius - 70.f * S), Centre.Y - FMath::Cos(A) * (Radius - 70.f * S),
			bOn ? Yellow : FLinearColor(1.f, 1.f, 1.f, 0.2f), (bOn ? 4.f : 2.f) * S);
		Text(SlotNames[Slot], At.X, At.Y - 32.f * S, bOn ? FLinearColor(0.05f, 0.05f, 0.05f) : Ink, Medium, 1.3f, true, !bOn);
		Text(SlotHints[Slot], At.X, At.Y + 6.f * S, bOn ? FLinearColor(0.15f, 0.12f, 0.f) : Muted, Medium, 0.95f, true, !bOn);
	}
	Text(WheelSlot >= 0 ? TEXT("Let go of Tab to use it") : TEXT("Point at one, then let go of Tab"), Centre.X, Centre.Y - 12.f * S, Ink, Medium, 1.f, true);
}
