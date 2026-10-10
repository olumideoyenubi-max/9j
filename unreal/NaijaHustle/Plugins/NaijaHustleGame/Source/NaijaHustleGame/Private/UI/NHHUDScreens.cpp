// The HUD's own screens: the map with its pin, the pause menu and the inventory wheel (see NHHUD.h)
#include "UI/NHHUD.h"
#include "Gameplay/NHEstate.h"
#include "Gameplay/NHInventory.h"

#include "Audio/NHAudioSubsystem.h"
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
#include "Phone/NHPhone.h"
#include "Characters/NHOutfitComponent.h"
#include "Player/NHCharacter.h"
#include "Player/NHPlayerController.h"
#include "Vehicles/NHCarTheft.h"
#include "Vehicles/NHTraffic.h"
#include "World/NHStreets.h"
#include "Vehicles/NHVehicle.h"

namespace NHScreens
{
	const FLinearColor Ink(0.96f, 0.95f, 0.9f);
	const FLinearColor Muted(0.7f, 0.68f, 0.62f);
	const FLinearColor Yellow(1.f, 0.77f, 0.f);
	const FLinearColor Good(0.3f, 0.85f, 0.4f);
	const FLinearColor Bad(0.95f, 0.25f, 0.2f);
	const FLinearColor PinBlue(0.2f, 0.75f, 1.f);
	const FLinearColor Land(0.2f, 0.19f, 0.16f);
	const TCHAR* Section = TEXT("NaijaHustle");

	enum { Resume, Character, Clothes, Lighting, Traffic, Look, Resolution, Minimap, AudioPage, Controls, Credits, Quit, Lines };
	const TCHAR* LineNames[] = { TEXT("Resume"), TEXT("Character"), TEXT("Clothes"), TEXT("Lighting"), TEXT("Traffic"), TEXT("Look speed"), TEXT("Resolution"), TEXT("Minimap"), TEXT("Audio"), TEXT("Controls"), TEXT("Credits"), TEXT("Quit game") };
	// the Controls page: a heading (no key) or a key and what it does
	const TCHAR* ControlList[][2] = {
		{ TEXT("ON FOOT"), nullptr }, { TEXT("W A S D"), TEXT("Move") }, { TEXT("Mouse"), TEXT("Look") }, { TEXT("Left Shift"), TEXT("Run while held") }, { TEXT("R"), TEXT("Run: stays on until pressed again") },
		{ TEXT("Left Ctrl or C"), TEXT("Roll") }, { TEXT("T"), TEXT("Fire or swing what is in your hand") }, { TEXT("Right mouse"), TEXT("Aim the gun in your hand") }, { TEXT("X"), TEXT("Crouch, and up again") }, { TEXT("G"), TEXT("Climb what is in front, up to 3.3 m") }, { TEXT("B"), TEXT("Bag: what you carry; eat, hold or drop it") }, { TEXT("Space"), TEXT("Jump; climbs a ledge, wall or car in front") }, { TEXT("F"), TEXT("Get in; try a car's handle; pull a driver out") },
		{ TEXT("E"), TEXT("Talk, act, next line; join wires when hotwiring") },
		{ TEXT("DRIVING"), nullptr }, { TEXT("W / S"), TEXT("Accelerate / brake and reverse") }, { TEXT("A / D"), TEXT("Steer") }, { TEXT("Space"), TEXT("Handbrake") },
		{ TEXT("K"), TEXT("Headlights on / off") }, { TEXT("V"), TEXT("Cabin view") }, { TEXT("H"), TEXT("Horn") }, { TEXT("Hold R"), TEXT("Radio wheel: point at a station, let go") }, { TEXT("T"), TEXT("Radio: next song") }, { TEXT("F"), TEXT("Get out") }, { TEXT("E"), TEXT("Do business at the mechanic, paint shop, chop shop") },
		{ TEXT("ANYWHERE"), nullptr }, { TEXT("P"), TEXT("Phone (arrows, Enter, Backspace)") }, { TEXT("M"), TEXT("Map: click to pin, right-click to clear, wheel to zoom") },
		{ TEXT("Hold Tab"), TEXT("Inventory wheel: phone, weapons, keys...") }, { TEXT("1 2 3 4"), TEXT("Choices in a panel") }, { TEXT("L / F1"), TEXT("Lighting: next preset / menu") }, { TEXT("F2"), TEXT("Streaming overlay: loaded cells") }, { TEXT("Esc"), TEXT("This menu") } };
	// the Clothes page: a slot's piece, or its colour
	struct FClothesLine { const TCHAR* Name; ENHOutfitSlot Slot; bool bColour; };
	const FClothesLine ClothesLines[] = { { TEXT("Hair"), ENHOutfitSlot::Hair, false }, { TEXT("Top or outfit"), ENHOutfitSlot::Top, false }, { TEXT("Top colour"), ENHOutfitSlot::Top, true },
		{ TEXT("Bottom"), ENHOutfitSlot::Bottom, false }, { TEXT("Bottom colour"), ENHOutfitSlot::Bottom, true }, { TEXT("Shoes"), ENHOutfitSlot::Shoes, false }, { TEXT("Shoe colour"), ENHOutfitSlot::Shoes, true } };
	// the Audio page: the seven sliders (ENHVolume), then these
	enum { AudioSubtitles = static_cast<int32>(ENHVolume::Count), AudioSubtitleSize, AudioMono, AudioLines };
	const TCHAR* SubtitleSizes[] = { TEXT("Small"), TEXT("Medium"), TEXT("Large") };
	const TCHAR* TrafficNames[] = { TEXT("None"), TEXT("Light"), TEXT("Normal"), TEXT("Heavy") };
	const TCHAR* PresetNames[] = { TEXT("Day"), TEXT("Dusty noon"), TEXT("Sunset"), TEXT("Night rain"), TEXT("Harsh morning"), TEXT("Golden evening") };

	enum { Phone, Wardrobe, CarKeys, Torch, Wallet, Hail, Machete, Pistol, AK47, Slots };
	const TCHAR* SlotNames[] = { TEXT("PHONE"), TEXT("WARDROBE"), TEXT("CAR KEYS"), TEXT("TORCH"), TEXT("WALLET"), TEXT("HAIL"), TEXT("MACHETE"), TEXT("PISTOL"), TEXT("AK-47") };
	const TCHAR* SlotHints[] = { TEXT("Chats, rides, map"), TEXT("Next character"), TEXT("Lock, unlock, find"), TEXT("Light on or off"), TEXT("What I have"), TEXT("Stop a ride"),
		TEXT("Take in hand"), TEXT("Take in hand"), TEXT("Take in hand") };
	const TCHAR* WeaponIds[] = { TEXT("machete"), TEXT("pistol"), TEXT("ak47") };
	// the radio wheel's ids: a station's index, or one of these
	enum { RadioNextSong = 1000, RadioSwitchOff };

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
	if (Screen != EScreen::Menu)
	{
		OpenClothes(false);
		bMenuAudio = false;
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

void ANHHUD::TogglePhone()
{
	ANHPhone* Phone = ANHPhone::Get(this);
	if (Phone && Screen == EScreen::None)
	{
		Phone->Toggle();
	}
}

void ANHHUD::Back()
{
	if (Screen == EScreen::Place)
	{
		if (ANHEstate* Estate = ANHEstate::Get(this))
		{
			Estate->CloseMenu();
		}
		return;
	}
	if (Screen == EScreen::Bag)
	{
		Open(EScreen::None);
		return;
	}
	ANHPhone* Phone = ANHPhone::Get(this);
	if (Phone && Screen == EScreen::None)
	{
		Phone->Back();
	}
}

void ANHHUD::OpenWardrobe()
{
	Open(EScreen::None);
	Open(EScreen::Menu);
	OpenClothes(true);
}

void ANHHUD::ToggleBag()
{
	if (Screen == EScreen::Bag)
	{
		Open(EScreen::None);
	}
	else if (Screen == EScreen::None && Cast<ANHCharacter>(GetOwningPawn()))
	{
		BagLine = 0;
		bBagDrop = false;
		Open(EScreen::Bag);
	}
}

void ANHHUD::DrawListMenu(const FString& Title, const FString& Heading, const TArray<FMenuRow>& Rows, int32 Chosen, const FString& Keys)
{
	using namespace NHScreens;
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Large = GEngine->GetLargeFont();
	const float X = 70.f * S, Y = 150.f * S, W = 560.f * S, Banner = 104.f * S, Bar = 44.f * S, RowH = 46.f * S;
	const int32 Showing = FMath::Min(Rows.Num(), 10), First = FMath::Clamp(Chosen - 6, 0, FMath::Max(0, Rows.Num() - Showing));
	// the banner and the bar under it
	Panel(X, Y, W, Banner, FLinearColor(0.02f, 0.02f, 0.02f, 0.96f));
	DrawRect(Yellow, X, Y + Banner - 6.f * S, W, 6.f * S);
	Text(Title, X + W * 0.5f, Y + 26.f * S, Yellow, Large, 1.9f, true);
	Panel(X, Y + Banner, W, Bar, FLinearColor(0.f, 0.f, 0.f, 0.98f));
	Text(Heading, X + 16.f * S, Y + Banner + 9.f * S, Ink, Medium, 1.05f);
	const FString Place = FString::Printf(TEXT("%d / %d"), Rows.Num() ? Chosen + 1 : 0, Rows.Num());
	float TW = 0.f, TH = 0.f;
	GetTextSize(Place, TW, TH, Medium, 1.05f * S);
	Text(Place, X + W - 16.f * S - TW, Y + Banner + 9.f * S, Ink, Medium, 1.05f);
	// the rows: the chosen one is a light bar with dark writing
	float Yy = Y + Banner + Bar;
	for (int32 I = First; I < First + Showing; ++I)
	{
		const bool bOn = I == Chosen;
		Panel(X, Yy, W, RowH, bOn ? FLinearColor(0.96f, 0.95f, 0.9f, 0.96f) : FLinearColor(0.f, 0.f, 0.f, I % 2 ? 0.62f : 0.7f));
		const FLinearColor Pen = bOn ? FLinearColor(0.03f, 0.03f, 0.03f) : Ink;
		Text(Rows[I].Label, X + 16.f * S, Yy + 10.f * S, Pen, Medium, 1.15f);
		GetTextSize(Rows[I].Value, TW, TH, Medium, 1.1f * S);
		Text(Rows[I].Value, X + W - 16.f * S - TW, Yy + 11.f * S, bOn ? Pen : Muted, Medium, 1.1f);
		Yy += RowH;
	}
	if (Rows.Num() == 0)
	{
		Panel(X, Yy, W, RowH, FLinearColor(0.f, 0.f, 0.f, 0.7f));
		Text(TEXT("Nothing here"), X + 16.f * S, Yy + 10.f * S, Muted, Medium, 1.15f);
		Yy += RowH;
	}
	// what the chosen row is, and the keys
	DrawRect(Yellow, X, Yy + 4.f * S, W, 3.f * S);
	const TArray<FString> Help = Rows.IsValidIndex(Chosen) ? Wrap(Rows[Chosen].Help, W - 32.f * S, Medium, 1.f * S) : TArray<FString>();
	const float HelpH = (Help.Num() + 1) * 26.f * S + 22.f * S;
	Panel(X, Yy + 7.f * S, W, HelpH, FLinearColor(0.f, 0.f, 0.f, 0.86f));
	float Hy = Yy + 16.f * S;
	for (const FString& Line : Help)
	{
		Text(Line, X + 16.f * S, Hy, Ink, Medium, 1.f);
		Hy += 26.f * S;
	}
	Text(Keys, X + 16.f * S, Hy + 4.f * S, Muted, Medium, 0.9f);
}

void ANHHUD::DrawBag(float VW, float VH)
{
	const ANHCharacter* Me = Cast<ANHCharacter>(GetOwningPawn());
	const UNHInventoryComponent* Bag = Me ? Me->GetInventory() : nullptr;
	if (!Bag)
	{
		Open(EScreen::None); // got into a vehicle with it open
		return;
	}
	TArray<FMenuRow> Rows;
	for (const FNHItemStack& Stack : Bag->GetStacks())
	{
		const FNHItemDef* D = UNHInventoryComponent::Def(Stack.Item);
		if (!D)
		{
			continue;
		}
		const bool bHeld = !D->Weapon.IsNone() && Me->Equipped() == D->Weapon;
		const TCHAR* Does = bBagDrop ? TEXT("Enter: drop one.") : !D->Weapon.IsNone() ? (bHeld ? TEXT("Enter: put it away.") : TEXT("Enter: take it in your hand."))
			: D->Heal > 0 ? TEXT("Enter: use one.") : !D->AmmoFor.IsNone() ? TEXT("Used as you fire.") : TEXT("Carried.");
		Rows.Add({ D->Label + (bHeld ? TEXT("  (in hand)") : TEXT("")), FString::Printf(TEXT("x%d   %.1f kg"), Stack.Count, D->Weight * Stack.Count / 1000.f), D->Description + TEXT(" ") + Does });
	}
	BagLine = FMath::Clamp(BagLine, 0, FMath::Max(0, Rows.Num() - 1));
	DrawListMenu(TEXT("BAG"), FString::Printf(TEXT("%s    %.1f of %.0f kg"), bBagDrop ? TEXT("DROP") : TEXT("USE"), Bag->Weight() / 1000.f, UNHInventoryComponent::MaxWeight() / 1000.f), Rows, BagLine,
		TEXT("Up / Down: choose    Enter: do it    Left / Right: use or drop    B: close"));
}

void ANHHUD::DrawPlace(float VW, float VH)
{
	const ANHEstate* Estate = ANHEstate::Get(this);
	if (!Estate)
	{
		return;
	}
	FString Title, Heading;
	TArray<FNHMenuLine> Lines;
	Estate->Menu(Title, Heading, Lines);
	TArray<FMenuRow> Rows;
	for (const FNHMenuLine& Line : Lines)
	{
		Rows.Add({ Line.Label, Line.Value, Line.Help });
	}
	PlaceLine = FMath::Clamp(PlaceLine, 0, FMath::Max(0, Rows.Num() - 1));
	DrawListMenu(Title, Heading, Rows, PlaceLine, TEXT("Up / Down: choose    Enter: do it    E or Backspace: leave"));
}

void ANHHUD::DrawDashboard(const ANHVehicle* V, float VW, float VH)
{
	using namespace NHScreens;
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Large = GEngine->GetLargeFont();
	int32 Gear = 0;
	float Rpm = 0.f;
	V->Readings(Gear, Rpm);
	const float Kmh = FMath::Abs(V->Speed) * 0.036f;
	// the dial reads to the next 20 over the vehicle's top speed
	const float Full = FMath::CeilToFloat(V->TopSpeed() * 0.036f / 20.f) * 20.f + 20.f;
	const float R = 105.f * S, Gap = 36.f * S;
	const FVector2D Speedo(VW - 60.f * S - R, VH - 70.f * S - R), Revs(Speedo.X - 2.f * R - Gap, Speedo.Y);
	Panel(Revs.X - R - 22.f * S, Speedo.Y - R - 22.f * S, 4.f * R + Gap + 44.f * S, 2.f * R + 78.f * S, FLinearColor(0.f, 0.f, 0.f, 0.62f));
	// a dial: from seven o'clock round the top to five o'clock, 240 degrees
	const auto Dial = [this, R, Medium](const FVector2D& C, float Value, float Top, float Step, int32 Minor, float RedFrom, const TFunctionRef<FString(float)>& Label)
	{
		const auto Angle = [Top](float Now) { return FMath::DegreesToRadians(150.f + 240.f * FMath::Clamp(Now / Top, 0.f, 1.f)); };
		const int32 Ticks = FMath::RoundToInt(Top / Step) * Minor;
		for (int32 I = 0; I <= Ticks; ++I)
		{
			const float Now = Top * I / Ticks, A = Angle(Now);
			const bool bMajor = I % Minor == 0;
			const FLinearColor Colour = Now >= RedFrom ? FLinearColor(0.95f, 0.15f, 0.1f) : NHScreens::Ink;
			const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
			DrawLine(C.X + Dir.X * R, C.Y + Dir.Y * R, C.X + Dir.X * (R - (bMajor ? 16.f : 8.f) * S), C.Y + Dir.Y * (R - (bMajor ? 16.f : 8.f) * S), Colour, (bMajor ? 3.f : 1.5f) * S);
			if (bMajor)
			{
				Text(Label(Now), C.X + Dir.X * (R - 34.f * S), C.Y + Dir.Y * (R - 34.f * S) - 9.f * S, Colour, Medium, 0.85f, true);
			}
		}
		const float A = Angle(Value);
		DrawLine(C.X - FMath::Cos(A) * 14.f * S, C.Y - FMath::Sin(A) * 14.f * S, C.X + FMath::Cos(A) * (R - 20.f * S), C.Y + FMath::Sin(A) * (R - 20.f * S), FLinearColor(1.f, 0.3f, 0.05f), 4.f * S);
		DrawRect(NHScreens::Ink, C.X - 5.f * S, C.Y - 5.f * S, 10.f * S, 10.f * S);
	};
	Dial(Speedo, Kmh, Full, 20.f, 2, BIG_NUMBER, [](float Now) { return FString::FromInt(FMath::RoundToInt(Now)); });
	Dial(Revs, Rpm, 8000.f, 1000.f, 2, 6500.f, [](float Now) { return FString::FromInt(FMath::RoundToInt(Now / 1000.f)); });
	Text(FString::FromInt(FMath::RoundToInt(Kmh)), Speedo.X, Speedo.Y + 30.f * S, Ink, Large, 1.5f, true);
	Text(TEXT("km/h"), Speedo.X, Speedo.Y + 66.f * S, Muted, Medium, 0.8f, true);
	Text(Gear < 0 ? TEXT("R") : Gear == 0 ? TEXT("N") : *FString::FromInt(Gear), Revs.X, Revs.Y + 30.f * S, Gear < 0 ? Bad : Yellow, Large, 1.5f, true);
	Text(TEXT("x1000 rpm"), Revs.X, Revs.Y + 66.f * S, Muted, Medium, 0.8f, true);
	// under the dials: the fuel, how far it has gone, and the lights that are lit
	const float Left = Revs.X - R, Wide = 4.f * R + Gap, Y = Speedo.Y + R + 18.f * S;
	Text(TEXT("E"), Left, Y - 3.f * S, V->Fuel < 0.12f ? Bad : Muted, Medium, 0.9f);
	Panel(Left + 20.f * S, Y, 150.f * S, 12.f * S, FLinearColor(1.f, 1.f, 1.f, 0.15f));
	DrawRect(V->Fuel < 0.12f ? Bad : V->Fuel < 0.25f ? Yellow : Good, Left + 20.f * S, Y, 150.f * S * V->Fuel, 12.f * S);
	Text(TEXT("F"), Left + 178.f * S, Y - 3.f * S, Muted, Medium, 0.9f);
	Text(FString::Printf(TEXT("%.0f L"), V->Fuel * V->TankLitres()), Left + 200.f * S, Y - 3.f * S, V->Fuel < 0.12f ? Bad : Ink, Medium, 0.9f);
	const FString Gone = FString::Printf(TEXT("%07.1f km"), V->Odometer / 100000.0);
	float TW = 0.f, TH = 0.f;
	GetTextSize(Gone, TW, TH, Medium, 0.9f * S);
	Text(Gone, Left + Wide - TW, Y - 3.f * S, Ink, Medium, 0.9f);
	const float Lights = Left + 270.f * S;
	Text(TEXT("LIGHTS"), Lights, Y - 3.f * S, V->HeadlightsOn() ? FLinearColor(0.3f, 0.9f, 0.4f) : FLinearColor(1.f, 1.f, 1.f, 0.22f), Medium, 0.8f);
	Text(TEXT("BRAKE"), Lights + 70.f * S, Y - 3.f * S, V->HandbrakeOn() ? Bad : FLinearColor(1.f, 1.f, 1.f, 0.22f), Medium, 0.8f);
	Text(TEXT("ENGINE"), Lights + 130.f * S, Y - 3.f * S, V->MaxHealth > 0.f && V->Health / V->MaxHealth < 0.35f ? Yellow : FLinearColor(1.f, 1.f, 1.f, 0.22f), Medium, 0.8f);
}

void ANHHUD::ToggleMenu()
{
	if (Screen == EScreen::Place)
	{
		Back();
		return;
	}
	if (Screen == EScreen::Bag)
	{
		Open(EScreen::None); // Esc closes the bag
		return;
	}
	if (Screen == EScreen::Menu && bMenuClothes)
	{
		OpenClothes(false); // Esc on the Clothes page: back to the menu
		return;
	}
	if (Screen == EScreen::Menu && (bMenuControls || bMenuCredits || bMenuAudio))
	{
		bMenuControls = bMenuCredits = bMenuAudio = false; // Esc on the Audio, Controls or Credits page: back to the menu
		return;
	}
	if (ANHPhone* Phone = ANHPhone::Get(this); Phone && Phone->IsOpen() && Screen == EScreen::None)
	{
		Phone->Close(); // Esc puts the phone away first
		return;
	}
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

void ANHHUD::FillWheel(bool bRadio)
{
	using namespace NHScreens;
	bRadioWheel = bRadio;
	WheelItems.Reset();
	if (!bRadio)
	{
		const ANHPlayerController* PC = Cast<ANHPlayerController>(GetOwningPlayerController());
		const ANHCharacter* Char = PC ? PC->GetOnFootCharacter() : Cast<ANHCharacter>(GetOwningPawn());
		if (!Char)
		{
			Char = Cast<ANHCharacter>(GetOwningPawn());
		}
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			const bool bHeld = Slot >= Machete && Char && Char->Equipped() == WeaponIds[Slot - Machete];
			WheelItems.Add({ SlotNames[Slot], bHeld ? TEXT("In hand: put away") : SlotHints[Slot], Slot });
		}
		return;
	}
	const UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this);
	const ANHVehicle* Car = Cast<ANHVehicle>(GetOwningPawn());
	for (int32 I = 0; Audio && I < Audio->GetStations().Num(); ++I)
	{
		const UNHAudioSubsystem::FRadioStation& Station = Audio->GetStations()[I];
		const bool bOn = Audio->RadioOn() && !Audio->RadioOnPhone() && Audio->RadioStationIndex() == I;
		WheelItems.Add({ Station.Name.ToUpper(), bOn ? TEXT("On now") : FString::Printf(TEXT("%d songs"), Station.Tracks.Num()), I });
	}
	WheelItems.Add({ TEXT("NEXT SONG"), TEXT("Skip this one"), RadioNextSong });
	WheelItems.Add({ TEXT("RADIO OFF"), FString(), RadioSwitchOff });
}

void ANHHUD::SetWheel(bool bOpen)
{
	if (bOpen && Screen == EScreen::None)
	{
		WheelSlot = -1;
		FillWheel(false);
		Open(EScreen::Wheel);
	}
	else if (!bOpen && Screen == EScreen::Wheel && !bRadioWheel)
	{
		const int32 Slot = WheelSlot;
		Open(EScreen::None);
		UseWheel(Slot);
	}
}

void ANHHUD::SetRadioWheel(bool bOpen)
{
	if (bOpen && Screen == EScreen::None && Cast<ANHVehicle>(GetOwningPawn()))
	{
		WheelSlot = -1;
		FillWheel(true);
		Open(EScreen::Wheel);
	}
	else if (!bOpen && Screen == EScreen::Wheel && bRadioWheel)
	{
		const int32 Id = WheelItems.IsValidIndex(WheelSlot) ? WheelItems[WheelSlot].Id : -1;
		Open(EScreen::None);
		UseRadioWheel(Id);
	}
}

void ANHHUD::UseRadioWheel(int32 Id)
{
	UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this);
	ANHVehicle* Car = Cast<ANHVehicle>(GetOwningPawn());
	if (!Audio || !Car || Id < 0)
	{
		return;
	}
	if (Id == NHScreens::RadioNextSong)
	{
		Audio->RadioOn() ? Audio->RadioNextTrack() : Toast(this, TEXT("The radio is off"), 0);
	}
	else if (Id == NHScreens::RadioSwitchOff)
	{
		Audio->RadioOff();
		Toast(this, TEXT("Radio off"), 0);
	}
	else
	{
		Audio->RadioPlay(Id, Car);
	}
}

void ANHHUD::Nav(int32 DX, int32 DY)
{
	if (Screen == EScreen::Place)
	{
		PlaceLine = FMath::Max(0, PlaceLine + DY); // DrawPlace keeps it on the list
		return;
	}
	if (Screen == EScreen::Bag)
	{
		const ANHCharacter* Me = Cast<ANHCharacter>(GetOwningPawn());
		const int32 Lines = Me && Me->GetInventory() ? Me->GetInventory()->GetStacks().Num() : 0;
		BagLine = Lines > 0 ? (BagLine + DY + Lines) % Lines : 0;
		bBagDrop = DX != 0 ? !bBagDrop : bBagDrop;
		return;
	}
	if (ANHPhone* Phone = ANHPhone::Get(this); Phone && Phone->IsOpen() && Screen == EScreen::None)
	{
		if (DY != 0)
		{
			Phone->Move(DY);
		}
		if (DX != 0)
		{
			Phone->Change(DX);
		}
		return;
	}
	if (Screen == EScreen::Menu && bMenuClothes)
	{
		const int32 Count = UE_ARRAY_COUNT(NHScreens::ClothesLines);
		ClothesLine = (ClothesLine + DY + Count) % Count;
		ANHPlayerController* PC = Cast<ANHPlayerController>(GetOwningPlayerController());
		if (ANHCharacter* Char = DX != 0 && PC ? PC->GetOnFootCharacter() : nullptr)
		{
			Char->ChangeOutfit(NHScreens::ClothesLines[ClothesLine].Slot, DX, NHScreens::ClothesLines[ClothesLine].bColour);
		}
		return;
	}
	if (Screen == EScreen::Menu && bMenuAudio)
	{
		AudioLine = (AudioLine + DY + NHScreens::AudioLines) % NHScreens::AudioLines;
		if (DX != 0)
		{
			AudioChange(AudioLine, DX);
		}
		return;
	}
	if (Screen == EScreen::Menu && (bMenuControls || bMenuCredits))
	{
		return;
	}
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
		const int32 Count = FMath::Max(1, WheelItems.Num());
		WheelSlot = ((WheelSlot < 0 ? 0 : WheelSlot + DX + DY) + Count) % Count;
	}
}

void ANHHUD::Accept()
{
	if (Screen == EScreen::Place)
	{
		if (ANHEstate* Estate = ANHEstate::Get(this))
		{
			Estate->Choose(PlaceLine);
		}
		return;
	}
	if (Screen == EScreen::Bag)
	{
		ANHCharacter* Me = Cast<ANHCharacter>(GetOwningPawn());
		UNHInventoryComponent* Bag = Me ? Me->GetInventory() : nullptr;
		if (Bag && Bag->GetStacks().IsValidIndex(BagLine))
		{
			const FName Item = Bag->GetStacks()[BagLine].Item;
			const FNHItemDef* D = UNHInventoryComponent::Def(Item);
			if (bBagDrop)
			{
				Bag->ServerDrop(Item, 1);
			}
			else if (D && !D->Weapon.IsNone())
			{
				Me->Equip(D->Weapon);
			}
			else
			{
				Bag->ServerUse(Item);
			}
		}
		return;
	}
	if (ANHPhone* Phone = ANHPhone::Get(this); Phone && Screen == EScreen::None)
	{
		Phone->Select(); // a row, the next line of a call, answering, or skipping a ride
		return;
	}
	if (Screen == EScreen::Menu)
	{
		if (bMenuClothes || MenuLine == NHScreens::Clothes)
		{
			OpenClothes(!bMenuClothes);
		}
		else if (bMenuAudio)
		{
			AudioChange(AudioLine, 1);
		}
		else if (!bMenuControls && !bMenuCredits && MenuLine == NHScreens::AudioPage)
		{
			bMenuAudio = true;
			AudioLine = 0;
		}
		else if (bMenuCredits || (!bMenuControls && MenuLine == NHScreens::Credits))
		{
			bMenuCredits = !bMenuCredits;
		}
		else if (bMenuControls || MenuLine == NHScreens::Controls)
		{
			bMenuControls = !bMenuControls;
		}
		else if (MenuLine == NHScreens::Resume)
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
	PinRouteAt = -100.f; // work the way out afresh
	PinRoute.Reset();
	PinTurn.Reset();
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
	if (const ANHStreets* Streets = ANHStreets::Get(this))
	{
		Streets->DrawNames(this, MapAt.X, MapAt.Y, MapSide, Corner, MapSpan, 40, S); // more names the closer the view
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
	if (const ANHEstate* Estate = ANHEstate::Get(this))
	{
		// homes: the player's own in gold with their names, the homes of the other people to play in white with theirs
		const UNHHustleSubsystem* Money = UNHHustleSubsystem::Get(this);
		TArray<FNHPlaceCard> Cards;
		Estate->Cards(Cards);
		for (const FNHPlaceCard& Card : Cards)
		{
			if ((Card.bOwned || !Card.Resident.IsEmpty()) && ToScreen(Card.Where, P))
			{
				const FLinearColor Colour = Card.bOwned ? FLinearColor(1.f, 0.8f, 0.1f) : FLinearColor(0.9f, 0.9f, 0.9f);
				DrawRect(Colour, P.X - 7.f * S, P.Y - 3.f * S, 14.f * S, 11.f * S);
				DrawRect(Colour, P.X - 4.f * S, P.Y - 7.f * S, 8.f * S, 4.f * S);
				DrawRect(Colour, P.X - 1.5f * S, P.Y - 10.f * S, 3.f * S, 3.f * S);
				const bool bHome = Money && Money->Home == Card.Id;
				Text(Card.bOwned ? Card.Name + (bHome ? TEXT(" (home)") : Card.bIsHome ? TEXT(" (yours)") : TEXT(" (your business)")) : Card.Resident + TEXT("'s home"), P.X + 12.f * S, P.Y - 10.f * S, Colour, Medium, 0.9f);
			}
		}
	}
	for (const FNHPlace& Place : Data->Places)
	{
		if (ToScreen(Place.Pos, P))
		{
			DrawRect(FLinearColor(0.4f, 0.9f, 0.5f), P.X - 5.f * S, P.Y - 5.f * S, 10.f * S, 10.f * S);
			Text(Place.Name, P.X + 10.f * S, P.Y - 10.f * S, FLinearColor(0.4f, 0.9f, 0.5f), Medium, 0.9f);
		}
	}
	FString RideLabel;
	if (FVector2D RideAt; ANHPhone::Get(this) && ANHPhone::Get(this)->RideMarker(RideAt, RideLabel) && ToScreen(RideAt, P))
	{
		DrawRect(FLinearColor(0.95f, 0.35f, 0.3f), P.X - 6.f * S, P.Y - 6.f * S, 12.f * S, 12.f * S);
		Text(RideLabel, P.X + 10.f * S, P.Y - 10.f * S, FLinearColor(0.95f, 0.35f, 0.3f), Medium, 0.9f);
	}
	if (bHasPin)
	{
		DrawPath(PinRoute, MapAt.X, MapAt.Y, MapSide, Corner, MapSpan, PinBlue, 4.f); // the way there
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
	case Clothes:
	{
		const ANHCharacter* Char = PC ? PC->GetOnFootCharacter() : nullptr;
		return Char && Char->GetOutfit() && Char->GetOutfit()->HasWardrobe() ? TEXT("Change") : TEXT("(this character comes as dressed)");
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
	case AudioPage: return TEXT("Volumes, subtitles, mono");
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
	if (bMenuClothes)
	{
		DrawClothes(VW, VH);
		return;
	}
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), 0.f, 0.f, VW, VH);
	if (bMenuAudio)
	{
		DrawAudio(VW, VH);
		return;
	}
	if (bMenuCredits)
	{
		// what the map is made from, and what is and is not real in it
		static const TCHAR* Text1[] = {
			TEXT("THE MAP"),
			TEXT("Streets, their names, the coastline, building outlines and land use are from OpenStreetMap."),
			TEXT("(c) OpenStreetMap contributors. The data is available under the Open Database Licence (ODbL):"),
			TEXT("openstreetmap.org/copyright"),
			TEXT(""),
			TEXT("WHAT IS REAL AND WHAT IS NOT"),
			TEXT("The streets, areas and public landmarks of Lagos are real, and named as they are."),
			TEXT("Every character, business, brand, app, bank, fuel station, union, gang, official and organisation"),
			TEXT("in this game is fictional. Any resemblance to a real person or a real organisation is coincidental."),
			TEXT("Nothing that happens in the story is a statement about anybody real."),
			TEXT(""),
			TEXT("MODELS AND OTHER MATERIAL"),
			TEXT("Credits and licences for third-party models are listed in ASSETS.md in the project.") };
		const float CW = 1280.f * S, CH = (150.f + 38.f * UE_ARRAY_COUNT(Text1)) * S, CX = (VW - CW) * 0.5f, CY = (VH - CH) * 0.5f;
		Panel(CX, CY, CW, CH, FLinearColor(0.05f, 0.05f, 0.05f, 0.95f));
		DrawRect(Yellow, CX, CY, CW, 5.f * S);
		Text(TEXT("CREDITS"), CX + 30.f * S, CY + 22.f * S, Yellow, Large, 1.3f);
		for (int32 I = 0; I < UE_ARRAY_COUNT(Text1); ++I)
		{
			const bool bHeading = FString(Text1[I]).ToUpper().Equals(Text1[I], ESearchCase::CaseSensitive) && FCString::Strlen(Text1[I]) > 0;
			Text(Text1[I], CX + 30.f * S, CY + (90.f + 38.f * I) * S, bHeading ? Yellow : Ink, Medium, bHeading ? 1.05f : 1.f);
		}
		Text(TEXT("Enter or Esc: back"), VW * 0.5f, CY + CH - 40.f * S, Ink, Medium, 0.95f, true);
		return;
	}
	if (bMenuControls)
	{
		// every key, in two columns
		const int32 Count = UE_ARRAY_COUNT(ControlList), PerColumn = (Count + 1) / 2;
		const float CW = 1500.f * S, CH = (150.f + 44.f * PerColumn) * S, CX = (VW - CW) * 0.5f, CY = (VH - CH) * 0.5f;
		Panel(CX, CY, CW, CH, FLinearColor(0.05f, 0.05f, 0.05f, 0.95f));
		DrawRect(Yellow, CX, CY, CW, 5.f * S);
		Text(TEXT("CONTROLS"), CX + 30.f * S, CY + 22.f * S, Yellow, Large, 1.3f);
		for (int32 I = 0; I < Count; ++I)
		{
			const float RX = CX + 30.f * S + (I / PerColumn) * CW * 0.5f, RY = CY + (90.f + 44.f * (I % PerColumn)) * S;
			if (!ControlList[I][1])
			{
				Text(ControlList[I][0], RX, RY + 6.f * S, Yellow, Medium, 1.05f);
			}
			else
			{
				Panel(RX, RY, 200.f * S, 36.f * S, FLinearColor(1.f, 1.f, 1.f, 0.1f));
				Text(ControlList[I][0], RX + 100.f * S, RY + 6.f * S, Ink, Medium, 1.f, true);
				Text(ControlList[I][1], RX + 216.f * S, RY + 6.f * S, Muted, Medium, 1.f);
			}
		}
		Text(TEXT("Enter or Esc: back"), VW * 0.5f, CY + CH - 40.f * S, Ink, Medium, 0.95f, true);
		return;
	}
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
	Text(TEXT("Up / Down: choose     Left / Right: change     Enter: select     Esc: back to the game"), VW * 0.5f, Y + H - 40.f * S, Ink, Medium, 0.95f, true);
}

// ------------------------------------------------------------------------------------------- the Audio page
FString ANHHUD::AudioValue(int32 Line) const
{
	using namespace NHScreens;
	const UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this);
	if (!Audio)
	{
		return FString();
	}
	switch (Line)
	{
	case AudioSubtitles: return Audio->SubtitlesOn() ? TEXT("On") : TEXT("Off");
	case AudioSubtitleSize: return SubtitleSizes[Audio->GetSubtitleSize()];
	case AudioMono: return Audio->MonoOn() ? TEXT("On") : TEXT("Off");
	default: return FString::Printf(TEXT("%d%%"), Audio->GetVolume(static_cast<ENHVolume>(Line)));
	}
}

void ANHHUD::AudioChange(int32 Line, int32 Dir)
{
	using namespace NHScreens;
	UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this);
	if (!Audio)
	{
		return;
	}
	switch (Line)
	{
	case AudioSubtitles: Audio->SetSubtitles(!Audio->SubtitlesOn()); break;
	case AudioSubtitleSize: Audio->SetSubtitleSize((Audio->GetSubtitleSize() + Dir + 3) % 3); break;
	case AudioMono: Audio->SetMono(!Audio->MonoOn()); break;
	default: Audio->SetVolume(static_cast<ENHVolume>(Line), Audio->GetVolume(static_cast<ENHVolume>(Line)) + 10 * Dir); break;
	}
}

void ANHHUD::DrawAudio(float VW, float VH)
{
	using namespace NHScreens;
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Large = GEngine->GetLargeFont();
	const UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this);
	const float W = 860.f * S, RowH = 54.f * S, H = 190.f * S + RowH * AudioLines, X = (VW - W) * 0.5f, Y = (VH - H) * 0.5f;
	Panel(X, Y, W, H, FLinearColor(0.05f, 0.05f, 0.05f, 0.95f));
	DrawRect(Yellow, X, Y, W, 5.f * S);
	Text(TEXT("AUDIO"), X + 30.f * S, Y + 22.f * S, Yellow, Large, 1.3f);
	float Yy = Y + 92.f * S;
	for (int32 Line = 0; Line < AudioLines; ++Line)
	{
		const bool bOn = Line == AudioLine;
		const bool bSlider = Line < static_cast<int32>(ENHVolume::Count);
		Panel(X + 22.f * S, Yy, W - 44.f * S, RowH - 8.f * S, bOn ? FLinearColor(1.f, 0.77f, 0.f, 0.22f) : FLinearColor(1.f, 1.f, 1.f, 0.06f));
		if (bOn)
		{
			DrawRect(Yellow, X + 22.f * S, Yy, 5.f * S, RowH - 8.f * S);
		}
		const TCHAR* Name = bSlider ? UNHAudioSubsystem::VolumeName(static_cast<ENHVolume>(Line)) : Line == AudioSubtitles ? TEXT("Subtitles") : Line == AudioSubtitleSize ? TEXT("Subtitle size") : TEXT("Mono audio");
		Text(Name, X + 44.f * S, Yy + 10.f * S, bOn ? Yellow : Ink, Medium, 1.2f);
		if (bSlider && Audio)
		{
			// the slider as a bar, between the name and the number
			const float BX = X + 300.f * S, BW = 330.f * S, K = Audio->GetVolume(static_cast<ENHVolume>(Line)) / 100.f;
			Panel(BX, Yy + 18.f * S, BW, 10.f * S, FLinearColor(1.f, 1.f, 1.f, 0.15f));
			DrawRect(bOn ? Yellow : Muted, BX, Yy + 18.f * S, BW * K, 10.f * S);
		}
		const FString Value = AudioValue(Line);
		float TW = 0.f, TH = 0.f;
		const FString Shown = bOn ? FString::Printf(TEXT("<   %s   >"), *Value) : Value;
		GetTextSize(Shown, TW, TH, Medium, 1.2f * S);
		Text(Shown, X + W - 44.f * S - TW, Yy + 10.f * S, bOn ? Ink : Muted, Medium, 1.2f);
		Yy += RowH;
	}
	const TCHAR* Hint = !Audio || !Audio->IsBuilt() ? TEXT("No sound device: the settings are kept for when there is one.")
		: AudioLine == AudioSubtitles ? TEXT("Lines with no recorded voice yet are always shown.")
		: AudioLine == AudioMono ? TEXT("The same sound in both ears.")
		: AudioLine == static_cast<int32>(ENHVolume::Master) ? TEXT("Everything.")
		: AudioLine == static_cast<int32>(ENHVolume::SFX) ? TEXT("Vehicles, weapons, footsteps, the phone and menu sounds.") : TEXT("");
	Text(Hint, VW * 0.5f, Y + H - 84.f * S, Muted, Medium, 0.95f, true);
	Text(TEXT("Up / Down: choose     Left / Right: change     Esc: back"), VW * 0.5f, Y + H - 44.f * S, Ink, Medium, 0.95f, true);
}

void ANHHUD::OpenClothes(bool bOpen)
{
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetOwningPlayerController());
	ANHCharacter* Char = PC ? PC->GetOnFootCharacter() : nullptr;
	bMenuClothes = bOpen && Char && Char->GetOutfit() && Char->GetOutfit()->HasWardrobe();
	if (Char)
	{
		Char->ShowFront(bMenuClothes); // the body turns to the camera while you choose
	}
}

void ANHHUD::DrawClothes(float VW, float VH)
{
	using namespace NHScreens;
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Large = GEngine->GetLargeFont();
	const ANHPlayerController* PC = Cast<ANHPlayerController>(GetOwningPlayerController());
	const ANHCharacter* Char = PC ? PC->GetOnFootCharacter() : nullptr;
	const UNHOutfitComponent* Outfit = Char ? Char->GetOutfit() : nullptr;
	if (!Outfit)
	{
		return;
	}
	// down the right-hand side, leaving the body in view
	const int32 Count = UE_ARRAY_COUNT(ClothesLines);
	const float W = 640.f * S, RowH = 58.f * S, H = 150.f * S + RowH * Count, X = VW - W - 60.f * S, Y = (VH - H) * 0.5f;
	Panel(X, Y, W, H, FLinearColor(0.05f, 0.05f, 0.05f, 0.94f));
	DrawRect(Yellow, X, Y, W, 5.f * S);
	Text(TEXT("CLOTHES"), X + 30.f * S, Y + 22.f * S, Yellow, Large, 1.3f);
	Text(*Char->SkinName(), X + 30.f * S, Y + 62.f * S, Muted, Medium, 0.95f);
	float Yy = Y + 92.f * S;
	for (int32 Line = 0; Line < Count; ++Line)
	{
		const bool bOn = Line == ClothesLine;
		Panel(X + 22.f * S, Yy, W - 44.f * S, RowH - 8.f * S, bOn ? FLinearColor(1.f, 0.77f, 0.f, 0.22f) : FLinearColor(1.f, 1.f, 1.f, 0.06f));
		if (bOn)
		{
			DrawRect(Yellow, X + 22.f * S, Yy, 5.f * S, RowH - 8.f * S);
		}
		Text(ClothesLines[Line].Name, X + 44.f * S, Yy + 12.f * S, bOn ? Yellow : Ink, Medium, 1.2f);
		const FString Value = ClothesLines[Line].bColour ? Outfit->ColourName(ClothesLines[Line].Slot) : Outfit->PieceName(ClothesLines[Line].Slot);
		float TW = 0.f, TH = 0.f;
		const FString Shown = bOn ? FString::Printf(TEXT("<   %s   >"), *Value) : Value;
		GetTextSize(Shown, TW, TH, Medium, 1.2f * S);
		Text(Shown, X + W - 44.f * S - TW, Yy + 12.f * S, bOn ? Ink : Muted, Medium, 1.2f);
		Yy += RowH;
	}
	Text(TEXT("Up / Down: choose     Left / Right: change     Esc: back"), X + W * 0.5f, Y + H - 40.f * S, Ink, Medium, 0.95f, true);
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
		TogglePhone();
		break;
	case Wardrobe:
		if (ANHCharacter* Char = PC->GetOnFootCharacter())
		{
			const FString Name = Char->WearNextSkin();
			Toast(this, Name.IsEmpty() ? FString(TEXT("Nothing else to wear in this project")) : FString::Printf(TEXT("Now: %s"), *Name), 1);
		}
		break;
	case CarKeys:
		if (ANHCarTheft* Theft = ANHCarTheft::Get(this); Theft && Theft->UseKeys(PC->GetLastVehicle()))
		{
			// your own car, near enough for the remote: locked or unlocked
		}
		else if (const ANHVehicle* Car = PC->GetLastVehicle(); Car && Car != Pawn)
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
	case Machete:
	case Pistol:
	case AK47:
		if (ANHCharacter* Char = Cast<ANHCharacter>(Pawn))
		{
			const FName Held = Char->Equip(WeaponIds[Slot - Machete]);
			Toast(this, Held.IsNone() ? FString::Printf(TEXT("%s put away"), SlotNames[Slot]) : FString::Printf(TEXT("%s in hand"), *ANHCharacter::WeaponName(Held)), 0);
		}
		else
		{
			Toast(this, TEXT("Not while driving"), 0);
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
	const int32 Count = FMath::Max(1, WheelItems.Num());
	// more than six round the wheel: a wider ring of narrower cards
	const float Radius = (Count > 6 ? 340.f : 250.f) * S, W = (Count > 6 ? 200.f : 250.f) * S, H = 92.f * S;
	// point with the mouse: past the middle of the wheel, the slot it points toward
	float MouseX = 0.f, MouseY = 0.f;
	if (PC && PC->GetMousePosition(MouseX, MouseY))
	{
		const FVector2D Aim = FVector2D(MouseX, MouseY) - Centre;
		if (Aim.Size() > 60.f * S)
		{
			const float Turn = FMath::RadiansToDegrees(FMath::Atan2(Aim.X, -Aim.Y)); // 0 up, clockwise
			WheelSlot = FMath::RoundToInt(FMath::Fmod(Turn + 360.f, 360.f) / (360.f / Count)) % Count;
		}
	}
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), 0.f, 0.f, VW, VH);
	for (int32 Slot = 0; Slot < WheelItems.Num(); ++Slot)
	{
		const float A = FMath::DegreesToRadians(Slot * 360.f / Count);
		const FVector2D At = Centre + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * Radius;
		const bool bOn = Slot == WheelSlot;
		Panel(At.X - W * 0.5f, At.Y - H * 0.5f, W, H, bOn ? FLinearColor(1.f, 0.77f, 0.f, 0.9f) : FLinearColor(0.05f, 0.05f, 0.05f, 0.88f));
		DrawLine(Centre.X + FMath::Sin(A) * 50.f * S, Centre.Y - FMath::Cos(A) * 50.f * S, Centre.X + FMath::Sin(A) * (Radius - 70.f * S), Centre.Y - FMath::Cos(A) * (Radius - 70.f * S),
			bOn ? Yellow : FLinearColor(1.f, 1.f, 1.f, 0.2f), (bOn ? 4.f : 2.f) * S);
		Text(WheelItems[Slot].Name, At.X, At.Y - 32.f * S, bOn ? FLinearColor(0.05f, 0.05f, 0.05f) : Ink, Medium, 1.3f, true, !bOn);
		Text(WheelItems[Slot].Hint, At.X, At.Y + 6.f * S, bOn ? FLinearColor(0.15f, 0.12f, 0.f) : Muted, Medium, 0.95f, true, !bOn);
	}
	const TCHAR* Key = bRadioWheel ? TEXT("R") : TEXT("Tab");
	if (bRadioWheel)
	{
		const UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this);
		Text(TEXT("RADIO"), Centre.X, Centre.Y - 44.f * S, Yellow, Medium, 1.1f, true);
		Text(Audio && Audio->RadioOn() ? Audio->RadioNowPlaying() : FString(TEXT("Off")), Centre.X, Centre.Y + 24.f * S, Muted, Medium, 0.9f, true);
	}
	Text(WheelSlot >= 0 ? FString::Printf(TEXT("Let go of %s to use it"), Key) : FString::Printf(TEXT("Point at one, then let go of %s"), Key), Centre.X, Centre.Y - 12.f * S, Ink, Medium, 1.f, true);
}

// --------------------------------------------------------------------------------------------------- the phone
TArray<FString> ANHHUD::Wrap(const FString& Str, float MaxWidth, UFont* Font, float Scale)
{
	TArray<FString> Lines, Words;
	Str.ParseIntoArray(Words, TEXT(" "));
	FString Line;
	for (const FString& Word : Words)
	{
		const FString Try = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
		float W = 0.f, H = 0.f;
		GetTextSize(Try, W, H, Font, Scale * S);
		if (W > MaxWidth && !Line.IsEmpty())
		{
			Lines.Add(Line);
			Line = Word;
		}
		else
		{
			Line = Try;
		}
	}
	if (!Line.IsEmpty() || Lines.Num() == 0)
	{
		Lines.Add(Line);
	}
	return Lines;
}

void ANHHUD::DrawPhone(float VW, float VH)
{
	using namespace NHScreens;
	ANHPhone* Phone = ANHPhone::Get(this);
	if (!Phone || !Phone->IsOpen())
	{
		return;
	}
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Large = GEngine->GetLargeFont();
	// A smartphone standing left of the minimap: a rounded metal body, a black bezel, the screen with its status bar
	// and the cut-out at the top, and the bar at the bottom you would swipe up from.
	const float W = 410.f * S, H = 840.f * S, X = VW - 28.f * S - 300.f * S - 40.f * S - W, Y = 26.f * S;
	// a box with rounded corners, drawn as thin strips that get shorter toward the top and bottom
	const auto Round = [this](float RX, float RY, float RW, float RH, float Radius, const FLinearColor& Colour)
	{
		const float Strip = FMath::Max(1.f, Radius / 14.f);
		for (float D = 0.f; D < Radius; D += Strip)
		{
			const float In = Radius - FMath::Sqrt(FMath::Max(0.f, Radius * Radius - FMath::Square(Radius - D - Strip * 0.5f)));
			DrawRect(Colour, RX + In, RY + D, RW - 2.f * In, Strip + 0.5f);
			DrawRect(Colour, RX + In, RY + RH - D - Strip, RW - 2.f * In, Strip + 0.5f);
		}
		DrawRect(Colour, RX, RY + Radius - 0.5f, RW, RH - 2.f * Radius + 1.f);
	};
	const FLinearColor Metal(0.2f, 0.2f, 0.215f, 1.f), Black(0.f, 0.f, 0.f, 1.f), Blue(0.04f, 0.52f, 1.f), Card(1.f, 1.f, 1.f, 0.07f);
	DrawRect(Metal, X - 4.f * S, Y + 150.f * S, 5.f * S, 34.f * S);                       // the switch and the volume buttons
	DrawRect(Metal, X - 4.f * S, Y + 210.f * S, 5.f * S, 62.f * S);
	DrawRect(Metal, X - 4.f * S, Y + 286.f * S, 5.f * S, 62.f * S);
	DrawRect(Metal, X + W - 1.f * S, Y + 240.f * S, 5.f * S, 96.f * S);                   // the side button
	Round(X, Y, W, H, 58.f * S, Metal);
	Round(X + 4.f * S, Y + 4.f * S, W - 8.f * S, H - 8.f * S, 54.f * S, Black);
	const float SX = X + 13.f * S, SY = Y + 13.f * S, SW = W - 26.f * S, SH = H - 26.f * S, In = 18.f * S;
	const bool bHome = Phone->OnHome();
	if (bHome)
	{
		// the wallpaper: a Lagos evening, deep blue down to orange over the lagoon
		const int32 Bands = 160;
		const float Radius = 46.f * S;
		for (int32 I = 0; I < Bands; ++I)
		{
			const float K = static_cast<float>(I) / (Bands - 1), BY = SY + SH * I / Bands, D = FMath::Min(BY - SY, SY + SH - BY - SH / Bands);
			const float Cut = D < Radius ? Radius - FMath::Sqrt(FMath::Max(0.f, Radius * Radius - FMath::Square(Radius - D))) : 0.f;
			const FLinearColor Sky = K < 0.6f ? FMath::Lerp(FLinearColor(0.02f, 0.04f, 0.16f), FLinearColor(0.35f, 0.1f, 0.3f), K / 0.6f) : FMath::Lerp(FLinearColor(0.35f, 0.1f, 0.3f), FLinearColor(0.95f, 0.45f, 0.1f), (K - 0.6f) / 0.4f);
			DrawRect(FLinearColor(Sky.R, Sky.G, Sky.B, 1.f), SX + Cut, BY, SW - 2.f * Cut, SH / Bands + 1.f);
		}
	}
	else
	{
		Round(SX, SY, SW, SH, 46.f * S, FLinearColor(0.045f, 0.045f, 0.055f, 1.f));
	}
	// the status bar: the time, the cut-out, the signal, the network and the battery
	const UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const float Hour = Hustle ? Hustle->HourOfDay() : 9.68f;
	Text(FString::Printf(TEXT("%d:%02d"), FMath::FloorToInt(Hour), FMath::FloorToInt(FMath::Frac(Hour) * 60.f)), SX + 34.f * S, SY + 16.f * S, FLinearColor::White, Medium, 1.05f, false, false);
	Round(SX + SW * 0.5f - 56.f * S, SY + 11.f * S, 112.f * S, 32.f * S, 16.f * S, Black);
	for (int32 I = 0; I < 4; ++I)
	{
		DrawRect(I < 3 ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.35f), SX + SW - 118.f * S + I * 7.f * S, SY + 30.f * S - (5.f + I * 3.f) * S, 5.f * S, (5.f + I * 3.f) * S);
	}
	Text(TEXT("5G"), SX + SW - 86.f * S, SY + 17.f * S, FLinearColor::White, Medium, 0.8f, false, false);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.4f), SX + SW - 60.f * S, SY + 18.f * S, 30.f * S, 14.f * S);
	DrawRect(Black, SX + SW - 58.5f * S, SY + 19.5f * S, 27.f * S, 11.f * S);
	DrawRect(FLinearColor::White, SX + SW - 57.f * S, SY + 21.f * S, 19.f * S, 8.f * S);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.4f), SX + SW - 29.f * S, SY + 22.f * S, 2.5f * S, 6.f * S);
	// the bar at the bottom
	Round(SX + SW * 0.5f - 62.f * S, SY + SH - 14.f * S, 124.f * S, 5.f * S, 2.5f * S, FLinearColor(1.f, 1.f, 1.f, 0.85f));

	if (bHome)
	{
		// the apps: icons four to a row with their names under them; the last four sit in the dock
		const int32 Count = Phone->Rows.Num(), Docked = FMath::Min(4, Count), Loose = Count - Docked;
		const float Icon = 70.f * S, Gap = (SW - 4.f * Icon) / 5.f, Top = SY + 150.f * S, RowH = 112.f * S;
		Text(Hustle ? FString::Printf(TEXT("Day %d"), Hustle->Day()) : FString(), SX + SW * 0.5f, SY + 62.f * S, FLinearColor(1.f, 1.f, 1.f, 0.85f), Medium, 1.f, true, false);
		Text(FString::Printf(TEXT("%d:%02d"), FMath::FloorToInt(Hour), FMath::FloorToInt(FMath::Frac(Hour) * 60.f)), SX + SW * 0.5f, SY + 80.f * S, FLinearColor::White, Large, 2.3f, true, false);
		const float DockY = SY + SH - 126.f * S;
		Round(SX + 12.f * S, DockY - 14.f * S, SW - 24.f * S, Icon + 28.f * S, 30.f * S, FLinearColor(1.f, 1.f, 1.f, 0.22f));
		for (int32 I = 0; I < Count; ++I)
		{
			const ANHPhone::FRow& Row = Phone->Rows[I];
			const bool bDock = I >= Loose, bOn = I == Phone->Selected;
			const int32 Slot = bDock ? I - Loose : I;
			const float IX = SX + Gap + (Slot % 4) * (Icon + Gap), IY = bDock ? DockY : Top + (Slot / 4) * RowH;
			if (bOn)
			{
				Round(IX - 5.f * S, IY - 5.f * S, Icon + 10.f * S, Icon + 10.f * S, 21.f * S, FLinearColor::White);
			}
			Round(IX, IY, Icon, Icon, 17.f * S, FLinearColor(Row.BadgeColor.R, Row.BadgeColor.G, Row.BadgeColor.B, 1.f));
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.1f), IX + 8.f * S, IY + 4.f * S, Icon - 16.f * S, Icon * 0.42f); // a sheen across the top half
			Text(Row.Badge.ToUpper(), IX + Icon * 0.5f, IY + 15.f * S, FLinearColor::White, Large, 1.5f, true, false);
			// a red count on an app with something new (its detail starts with a number)
			if (Row.Detail.Len() > 0 && FChar::IsDigit(Row.Detail[0]) && FCString::Atoi(*Row.Detail) > 0)
			{
				Round(IX + Icon - 16.f * S, IY - 8.f * S, 26.f * S, 26.f * S, 13.f * S, FLinearColor(0.95f, 0.15f, 0.15f, 1.f));
				Text(FString::FromInt(FMath::Min(FCString::Atoi(*Row.Detail), 99)), IX + Icon - 3.f * S, IY - 5.f * S, FLinearColor::White, Medium, 0.8f, true, false);
			}
			if (!bDock)
			{
				FString Name = Row.Text;
				float TW = 0.f, TH = 0.f;
				for (GetTextSize(Name, TW, TH, Medium, 0.78f * S); TW > Icon + Gap - 4.f * S && Name.Len() > 3; GetTextSize(Name, TW, TH, Medium, 0.78f * S))
				{
					Name = Name.LeftChop(2).TrimEnd() + TEXT(".");
				}
				Text(Name, IX + Icon * 0.5f, IY + Icon + 6.f * S, FLinearColor::White, Medium, 0.78f, true, true);
			}
		}
		// what the chosen app has to say, over the dock
		if (Phone->Rows.IsValidIndex(Phone->Selected))
		{
			const ANHPhone::FRow& On = Phone->Rows[Phone->Selected];
			Text(On.Text + (On.Detail.IsEmpty() ? FString() : TEXT("  ·  ") + On.Detail), SX + SW * 0.5f, DockY - 46.f * S, FLinearColor::White, Medium, 0.9f, true, true);
		}
		return;
	}

	// ---- inside an app: a bar with Back and the app's name, an address bar in the browser, then its rows as cards
	Text(TEXT("< Back"), SX + In, SY + 62.f * S, Blue, Medium, 1.f, false, false);
	Text(Phone->Title, SX + SW * 0.5f, SY + 60.f * S, FLinearColor::White, Medium, 1.15f, true, false);
	float Top = SY + 98.f * S;
	if (!Phone->Address.IsEmpty())
	{
		Round(SX + 12.f * S, Top, SW - 24.f * S, 38.f * S, 12.f * S, FLinearColor(1.f, 1.f, 1.f, 0.12f));
		DrawRect(FLinearColor(0.4f, 0.85f, 0.5f), SX + 28.f * S, Top + 17.f * S, 10.f * S, 9.f * S);                     // a padlock
		DrawRect(FLinearColor(0.4f, 0.85f, 0.5f), SX + 30.f * S, Top + 11.f * S, 6.f * S, 2.f * S);
		DrawRect(FLinearColor(0.4f, 0.85f, 0.5f), SX + 30.f * S, Top + 11.f * S, 2.f * S, 7.f * S);
		DrawRect(FLinearColor(0.4f, 0.85f, 0.5f), SX + 34.f * S, Top + 11.f * S, 2.f * S, 7.f * S);
		Text(Phone->Address, SX + SW * 0.5f, Top + 9.f * S, FLinearColor(0.92f, 0.92f, 0.95f), Medium, 0.95f, true, false);
		Top += 50.f * S;
	}
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.1f), SX, Top - 6.f * S, SW, 1.f);
	const float Bottom = SY + SH - 52.f * S, TextW = SW - 2.f * In;

	// how tall each row is, then which row to start from so the chosen one is on the screen
	struct FLaid { TArray<FString> Lines, More; float Height; };
	TArray<FLaid> Laid;
	bool bAnyChoice = false;
	for (const ANHPhone::FRow& Row : Phone->Rows)
	{
		FLaid L;
		const float Indent = Row.Badge.IsEmpty() ? 0.f : 56.f * S;
		L.Lines = Wrap(Row.Text, TextW - Indent - (Row.bChoice ? 20.f * S : 0.f), Medium, Row.bChoice ? 1.05f : 0.95f);
		if (!Row.Detail.IsEmpty())
		{
			L.More = Wrap(Row.Detail, TextW - Indent - 20.f * S, Medium, 0.85f);
		}
		L.Height = L.Lines.Num() * (Row.bChoice ? 28.f : 25.f) * S + L.More.Num() * 21.f * S + (Row.bChoice ? 22.f * S : 6.f * S) + (Row.Meter >= 0 ? 14.f * S : 0.f);
		Laid.Add(L);
		bAnyChoice |= Row.bChoice;
	}
	int32 First = bAnyChoice ? 0 : FMath::Clamp(Phone->Scroll, 0, FMath::Max(0, Laid.Num() - 1));
	if (bAnyChoice)
	{
		for (; First < Phone->Selected; ++First)
		{
			float Need = 0.f;
			for (int32 I = First; I <= Phone->Selected && I < Laid.Num(); ++I)
			{
				Need += Laid[I].Height;
			}
			if (Need <= Bottom - Top)
			{
				break;
			}
		}
	}
	float Yy = Top;
	for (int32 I = First; I < Phone->Rows.Num(); ++I)
	{
		const ANHPhone::FRow& Row = Phone->Rows[I];
		const FLaid& L = Laid[I];
		if (Yy + L.Height > Bottom)
		{
			Text(TEXT("..."), SX + SW * 0.5f, Bottom - 20.f * S, Muted, Medium, 1.f, true);
			break;
		}
		const bool bOn = Row.bChoice && I == Phone->Selected;
		float TextX = SX + In;
		if (Row.bChoice)
		{
			Round(SX + 10.f * S, Yy, SW - 20.f * S, L.Height - 7.f * S, 14.f * S, bOn ? FLinearColor(Blue.R, Blue.G, Blue.B, 0.9f) : Card);
			TextX += 6.f * S;
		}
		if (!Row.Badge.IsEmpty())
		{
			// the portrait or the site's icon: a rounded tile with an initial
			Round(TextX, Yy + 8.f * S, 42.f * S, 42.f * S, 11.f * S, FLinearColor(Row.BadgeColor.R, Row.BadgeColor.G, Row.BadgeColor.B, 1.f));
			Text(Row.Badge.ToUpper(), TextX + 21.f * S, Yy + 15.f * S, FLinearColor::White, Medium, 1.2f, true, false);
			TextX += 56.f * S;
		}
		float LineY = Yy + (Row.bChoice ? 8.f : 0.f) * S;
		for (const FString& Line : L.Lines)
		{
			Text(Line, TextX, LineY, bOn ? FLinearColor::White : Row.Color, Medium, Row.bChoice ? 1.05f : 0.95f, false, false);
			LineY += (Row.bChoice ? 28.f : 25.f) * S;
		}
		for (const FString& Line : L.More)
		{
			Text(Line, TextX, LineY - 2.f * S, bOn ? FLinearColor(1.f, 1.f, 1.f, 0.85f) : Muted, Medium, 0.85f, false, false);
			LineY += 21.f * S;
		}
		if (Row.Meter >= 0)
		{
			const float BarW = SW - (TextX - SX) - In - 8.f * S;
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.12f), TextX, LineY + 2.f * S, BarW, 6.f * S);
			DrawRect(Row.Meter >= 60 ? FLinearColor(0.35f, 0.85f, 0.45f) : Row.Meter >= 30 ? Yellow : FLinearColor(1.f, 0.35f, 0.3f), TextX, LineY + 2.f * S, BarW * Row.Meter / 100.f, 6.f * S);
		}
		Yy += L.Height;
	}
	const TArray<FString> Foot = Wrap(Phone->Footer, TextW, Medium, 0.72f);
	Text(Foot[0], SX + SW * 0.5f, SY + SH - 44.f * S, FLinearColor(1.f, 1.f, 1.f, 0.4f), Medium, 0.72f, true, false);
}

// --------------------------------------------------------------------------------------- directions to the pin
void ANHHUD::DrawPath(const TArray<FVector2D>& Path, float X, float Y, float Size, const FVector2D& Corner, float Span, const FLinearColor& Color, float Thick)
{
	for (int32 I = 0; I + 1 < Path.Num(); ++I)
	{
		// clip each stretch to the square (Liang-Barsky)
		const FVector2D A = (Path[I] - Corner) / Span, D = (Path[I + 1] - Corner) / Span - A;
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
			DrawLine(X + From.X * Size, Y + From.Y * Size, X + To.X * Size, Y + To.Y * Size, Color, Thick * S);
		}
	}
}

void ANHHUD::UpdatePinRoute(const FVector& Player)
{
	const UNHGameData* Data = UNHGameData::Get(this);
	const float Now = GetWorld()->GetRealTimeSeconds();
	if (!Data || !Data->bRealCity)
	{
		return;
	}
	const FVector2D Here(Player);
	if (Now - PinRouteAt > 3.f)
	{
		PinRouteAt = Now;
		if (!Data->RoadRoute(Here, Pin, PinRoute))
		{
			PinRoute.Reset();
			PinTurn = TEXT("No road goes there");
			return;
		}
		PinRoute.Insert(Here, 0); // from where you stand to the road
		PinRoute.Add(Pin);
	}
	if (PinRoute.Num() < 3)
	{
		return;
	}
	// the next real turn ahead: where the road's heading swings by more than 40 degrees within 30 m
	float Along = 0.f;
	PinTurn.Reset();
	for (int32 I = 1; I + 1 < PinRoute.Num(); ++I)
	{
		Along += FVector2D::Distance(PinRoute[I - 1], PinRoute[I]);
		if (I < 2)
		{
			continue; // the first stretch is only you walking to the road
		}
		const FVector2D In = (PinRoute[I] - PinRoute[I - 1]).GetSafeNormal();
		FVector2D Out = In;
		float Reach = 0.f;
		for (int32 J = I; J + 1 < PinRoute.Num() && Reach < 3000.f; ++J)
		{
			Reach += FVector2D::Distance(PinRoute[J], PinRoute[J + 1]);
			Out = (PinRoute[J + 1] - PinRoute[I]).GetSafeNormal();
		}
		const float Cross = In.X * Out.Y - In.Y * Out.X, Dot = In | Out;
		if (FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(Cross), Dot)) > 40.f && I + 2 < PinRoute.Num())
		{
			FNHRoadSeg Seg;
			FVector2D OnRoad;
			// the street being turned onto: the district's own data where there is some (it knows the side streets), else the main road's name
			const ANHStreets* Streets = ANHStreets::Get(this);
			FString Road = Streets ? Streets->StreetAt(PinRoute[I] + Out * 2500.f, 1500.f) : FString();
			if (Road.IsEmpty() && Data->NearestRoad(PinRoute[I] + Out * 2500.f, Seg, OnRoad))
			{
				Road = Data->RoadWays[Seg.Way].Name;
			}
			const FString Far = Along >= 100000.f ? FString::Printf(TEXT("%.1f km"), Along / 100000.f) : FString::Printf(TEXT("%d m"), FMath::RoundToInt(Along / 1000.f) * 10);
			PinTurn = FString::Printf(TEXT("In %s turn %s%s"), *Far, Cross > 0.f ? TEXT("right") : TEXT("left"), Road.IsEmpty() ? TEXT("") : *(TEXT(" onto ") + Road)); // Y runs south, so a positive cross is a right turn
			return;
		}
	}
	PinTurn = TEXT("Straight on to the pin");
}
