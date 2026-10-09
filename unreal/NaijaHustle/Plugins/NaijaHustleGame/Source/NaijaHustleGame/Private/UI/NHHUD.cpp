#include "UI/NHHUD.h"

#include "Audio/NHAudioSubsystem.h"
#include "EngineUtils.h"
#include "Gameplay/NHPerson.h"
#include "Player/NHCharacter.h"
#include "Core/NHGameData.h"
#include "Core/NHHustleSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/NHGameDirector.h"
#include "NaijaHustleGame.h"
#include "Phone/NHPhone.h"
#include "Player/NHPlayerController.h"
#include "Vehicles/NHCarTheft.h"
#include "World/NHStreets.h"
#include "Vehicles/NHVehicle.h"

namespace NHUI
{
	const FLinearColor Ink(0.96f, 0.95f, 0.9f);
	const FLinearColor Muted(0.7f, 0.68f, 0.62f);
	const FLinearColor Yellow(1.f, 0.77f, 0.f);
	const FLinearColor Good(0.35f, 0.85f, 0.45f);
	const FLinearColor Bad(1.f, 0.35f, 0.3f);

	FColor TileColor(TCHAR C)
	{
		switch (C)
		{
		case TEXT('R'): return FColor(58, 58, 62);
		case TEXT('K'): return FColor(132, 126, 116);
		case TEXT('B'): return FColor(176, 160, 134);
		case TEXT('T'): return FColor(112, 132, 152);
		case TEXT('S'): return FColor(160, 104, 64);
		case TEXT('W'): return FColor(36, 84, 112);
		case TEXT('L'): return FColor(104, 76, 56);
		case TEXT('P'): return FColor(146, 144, 134);
		case TEXT('V'): return FColor(66, 104, 48);
		case TEXT('F'): return FColor(196, 196, 186);
		case TEXT('G'): return FColor(116, 96, 72);
		default:        return FColor(20, 20, 20);
		}
	}
}

ANHHUD* ANHHUD::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC ? Cast<ANHHUD>(PC->GetHUD()) : nullptr;
}

void ANHHUD::Toast(const UObject* WorldContext, const FString& Str, int32 Kind)
{
	if (ANHHUD* H = Get(WorldContext))
	{
		H->Toasts.RemoveAll([&Str](const FToast& T) { return T.Text == Str; });
		H->Toasts.Add({ Str, Kind, 4.f });
		if (H->Toasts.Num() > 4)
		{
			H->Toasts.RemoveAt(0);
		}
	}
}

void ANHHUD::Floater(const UObject* WorldContext, const FVector& World, const FString& Str)
{
	if (ANHHUD* H = Get(WorldContext))
	{
		H->Floats.Add({ World, Str, 2.f });
	}
}

void ANHHUD::BeginPlay()
{
	Super::BeginPlay();
	BuildMinimap();
	BuildCityMap();
	LoadSettings();
	FParse::Value(FCommandLine::Get(), TEXT("NHHudShot="), ShotAt);
	FParse::Value(FCommandLine::Get(), TEXT("NHHudOpen="), ShotOpen);
	FParse::Value(FCommandLine::Get(), TEXT("NHHudLead="), ShotLead);
	if (UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this))
	{
		EarnHandle = Hustle->OnEarn.AddWeakLambda(this, [this](int32 Amount, const FString&) { CashDelta = Amount; CashDeltaT = 2.5f; });
	}
}

void ANHHUD::BuildMinimap()
{
	const UNHGameData* Data = UNHGameData::Get(this);
	if (!Data || !Data->bLoaded)
	{
		return;
	}
	const int32 W = Data->Cols, H = Data->Rows;
	MapTex = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
	if (!MapTex)
	{
		return;
	}
	MapTex->Filter = TF_Nearest;
	MapTex->AddressX = TA_Clamp;
	MapTex->AddressY = TA_Clamp;
	FTexture2DMipMap& Mip = MapTex->GetPlatformData()->Mips[0];
	FColor* Px = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	for (int32 R = 0; R < H; ++R)
	{
		for (int32 C = 0; C < W; ++C)
		{
			Px[R * W + C] = NHUI::TileColor(Data->Tiles.IsValidIndex(R) && C < Data->Tiles[R].Len() ? Data->Tiles[R][C] : TEXT('#'));
		}
	}
	Mip.BulkData.Unlock();
	MapTex->UpdateResource();
}

float ANHHUD::Text(const FString& Str, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale, bool bCentre, bool bShadow)
{
	float W = 0.f, H = 0.f;
	GetTextSize(Str, W, H, Font, Scale * S);
	const float DX = bCentre ? -W * 0.5f : 0.f;
	if (bShadow)
	{
		DrawText(Str, FLinearColor(0.f, 0.f, 0.f, 0.8f), X + DX + 2.f * S, Y + 2.f * S, Font, Scale * S);
	}
	DrawText(Str, Color, X + DX, Y, Font, Scale * S);
	return H;
}

void ANHHUD::Panel(float X, float Y, float W, float H, const FLinearColor& Color)
{
	DrawRect(Color, X, Y, W, H);
}

void ANHHUD::DrawStars(float X, float Y, int32 Stars, float Size)
{
	for (int32 I = 0; I < 5; ++I)
	{
		const FLinearColor C = I < Stars ? NHUI::Yellow : FLinearColor(1.f, 1.f, 1.f, 0.18f);
		const float CX = X + I * Size * 1.25f + Size * 0.5f, CY = Y + Size * 0.5f;
		// a five-pointed star from line segments
		for (int32 K = 0; K < 5; ++K)
		{
			const float A0 = -PI / 2.f + K * 4.f * PI / 5.f, A1 = -PI / 2.f + (K + 1) * 4.f * PI / 5.f;
			DrawLine(CX + FMath::Cos(A0) * Size * 0.5f, CY + FMath::Sin(A0) * Size * 0.5f, CX + FMath::Cos(A1) * Size * 0.5f, CY + FMath::Sin(A1) * Size * 0.5f, C, 2.f * S);
		}
	}
}

void ANHHUD::DrawMinimap(float X, float Y, float Size, const FVector& Player, float Yaw)
{
	const UNHGameData* Data = UNHGameData::Get(this);
	if (!MapTex || !Data)
	{
		return;
	}
	const float Cells = 44.f; // ~176 m across
	// how much of the world the map shows: the small city's 176 m, or 600 m of the real one's roads
	const float Span = Data->bRealCity ? 60000.f : Cells * Data->CellSize;
	const FVector2D Corner(Player.X - Span * 0.5f, Player.Y - Span * 0.5f);
	Panel(X - 4.f * S, Y - 4.f * S, Size + 8.f * S, Size + 8.f * S, FLinearColor(0.f, 0.f, 0.f, 0.7f));
	if (Data->bRealCity)
	{
		DrawRoads(X, Y, Size, Corner, Span, 1.f); // the real city has no cell map: the roads around the player, from the road graph
		if (const ANHStreets* Streets = ANHStreets::Get(this))
		{
			Streets->DrawNames(this, X, Y, Size, Corner, Span, 4, S); // side streets, and the nearest few names
		}
	}
	else
	{
		const float PC = Player.X / Data->CellSize, PR = Player.Y / Data->CellSize;
		const float U0 = (PC - Cells * 0.5f) / Data->Cols, V0 = (PR - Cells * 0.5f) / Data->Rows;
		DrawTexture(MapTex, X, Y, Size, Size, U0, V0, Cells / Data->Cols, Cells / Data->Rows, FLinearColor::White, BLEND_Opaque);
	}
	auto ToMap = [&](const FVector& W, bool bClamp, FVector2D& Out)
	{
		const float MX = (W.X - Corner.X) / Span, MY = (W.Y - Corner.Y) / Span;
		const bool bInside = MX >= 0.f && MX <= 1.f && MY >= 0.f && MY <= 1.f;
		Out = FVector2D(X + FMath::Clamp(MX, 0.02f, 0.98f) * Size, Y + FMath::Clamp(MY, 0.02f, 0.98f) * Size);
		return bInside || bClamp;
	};
	if (const ANHGameDirector* Dir = ANHGameDirector::Get(this))
	{
		FVector2D M;
		for (const FVector& Stop : Dir->RouteStopPoints)
		{
			if (ToMap(Stop, false, M))
			{
				DrawRect(NHUI::Yellow, M.X - 4.f * S, M.Y - 4.f * S, 8.f * S, 8.f * S);
			}
		}
		if (Dir->bMarker && ToMap(Dir->Marker, true, M))
		{
			DrawRect(FLinearColor(1.f, 0.2f, 0.2f), M.X - 6.f * S, M.Y - 6.f * S, 12.f * S, 12.f * S);
		}
	}
	for (const FNHPlace& Place : Data->Places)
	{
		if (FVector2D M; ToMap(FVector(Place.Pos, 0.f), false, M))
		{
			DrawRect(FLinearColor(0.4f, 0.9f, 0.5f), M.X - 4.f * S, M.Y - 4.f * S, 8.f * S, 8.f * S); // the mechanic, the paint shop, the chop shop
		}
	}
	FString RideLabel;
	if (FVector2D RideAt, M; ANHPhone::Get(this) && ANHPhone::Get(this)->RideMarker(RideAt, RideLabel) && ToMap(FVector(RideAt, 0.f), true, M))
	{
		DrawRect(FLinearColor(0.95f, 0.35f, 0.3f), M.X - 5.f * S, M.Y - 5.f * S, 10.f * S, 10.f * S); // the hailed ride, on its way
	}
	if (bHasPin)
	{
		DrawPath(PinRoute, X, Y, Size, Corner, Span, FLinearColor(0.2f, 0.75f, 1.f), 3.f); // the way to the pin
	}
	if (FVector2D M; bHasPin && ToMap(FVector(Pin, 0.f), true, M))
	{
		DrawRect(FLinearColor(0.2f, 0.75f, 1.f), M.X - 6.f * S, M.Y - 6.f * S, 12.f * S, 12.f * S);
	}
	// the player: a dot with a heading tick
	const FVector2D C(X + Size * 0.5f, Y + Size * 0.5f);
	const float A = FMath::DegreesToRadians(Yaw);
	DrawLine(C.X, C.Y, C.X + FMath::Cos(A) * 14.f * S, C.Y + FMath::Sin(A) * 14.f * S, FLinearColor::White, 3.f * S);
	DrawRect(FLinearColor::White, C.X - 5.f * S, C.Y - 5.f * S, 10.f * S, 10.f * S);
}

void ANHHUD::DrawHUD()
{
	Super::DrawHUD();
	using namespace NHUI;
	if (!Canvas || !GEngine)
	{
		return;
	}
	const float Now = GetWorld()->GetRealTimeSeconds(), Dt = FMath::Clamp(Now - LastTime, 0.f, 0.25f);
	LastTime = Now;
	S = Canvas->ClipY / 1080.f;
	const float VW = Canvas->ClipX, VH = Canvas->ClipY, Pad = 28.f * S;
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();

	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const UNHGameData* Data = UNHGameData::Get(this);
	ANHGameDirector* Dir = ANHGameDirector::Get(this);
	APawn* Pawn = GetOwningPawn();
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetOwningPlayerController());

	// ---- money and wanted level (top left)
	if (Hustle)
	{
		Text(UNHHustleSubsystem::Naira(Hustle->Cash), Pad, Pad, Ink, Large, 2.2f);
		if (CashDeltaT > 0.f)
		{
			CashDeltaT -= Dt;
			Text((CashDelta > 0 ? TEXT("+") : TEXT("")) + UNHHustleSubsystem::Naira(CashDelta), Pad, Pad + 62.f * S, CashDelta > 0 ? Good : Bad, Large, 1.2f);
		}
		DrawStars(Pad, Pad + 100.f * S, Hustle->Stars(), 26.f * S);
	}

	// ---- job card (left, under the stars)
	if (Dir && !Dir->ObjTitle.IsEmpty())
	{
		const float X = Pad, Y = Pad + 150.f * S, W = 470.f * S;
		const float H = (Dir->ObjSub.IsEmpty() ? 84.f : 112.f) * S + (Dir->ShiftLine.IsEmpty() ? 0.f : 56.f * S);
		Panel(X, Y, W, H);
		DrawRect(Yellow, X, Y, 6.f * S, H);
		Text(Dir->ObjTitle, X + 20.f * S, Y + 10.f * S, Yellow, Medium, 1.f);
		Text(Dir->ObjText, X + 20.f * S, Y + 38.f * S, Ink, Medium, 1.25f);
		float Yy = Y + 74.f * S;
		if (!Dir->ObjSub.IsEmpty())
		{
			Text(Dir->ObjSub, X + 20.f * S, Yy, Muted, Medium, 1.f);
			Yy += 28.f * S;
		}
		if (!Dir->ShiftLine.IsEmpty())
		{
			Text(Dir->ShiftLine, X + 20.f * S, Yy + 4.f * S, Ink, Medium, 1.f);
			Text(Dir->NextStopLine, X + 20.f * S, Yy + 30.f * S, Muted, Medium, 1.f);
		}
		if (Dir->bMarker && Pawn) // distance to the objective, the same number as the marker
		{
			const int32 Metres = FMath::RoundToInt(FVector::Dist2D(Pawn->GetActorLocation(), Dir->Marker) / 100.f);
			// how far, and what street it is on: "26 m, Agege Motor Road", against the card's right edge
			const ANHStreets* Streets = ANHStreets::Get(this);
			const FString On = Streets ? Streets->StreetAt(FVector2D(Dir->Marker), 4000.f) : FString();
			const FString Far = FString::Printf(TEXT("%d m"), Metres) + (On.IsEmpty() ? FString() : TEXT(", ") + On);
			float FW = 0.f, FH = 0.f;
			GetTextSize(Far, FW, FH, Medium, 0.9f * S);
			Text(Far, X + W - 14.f * S - FW, Y + H - FH - 8.f * S, Muted, Medium, 0.9f);
		}
		if (Dir->DeadlineMinutesLeft >= 0.f)
		{
			const int32 M = FMath::CeilToInt(Dir->DeadlineMinutesLeft);
			Text(FString::Printf(TEXT("%d:%02d left"), M / 60, M % 60), X + W - 120.f * S, Y + 38.f * S, M < 20 ? Bad : Ink, Medium, 1.1f);
		}
	}

	// ---- clock, minimap, district (top right)
	const float MapSize = 300.f * S, MX = VW - Pad - MapSize, MY = Pad + 46.f * S;
	if (Hustle)
	{
		Text(Hustle->ClockText(), VW - Pad - 230.f * S, Pad, Ink, Large, 1.5f);
	}
	if (!bSettingsApplied && PC)
	{
		ApplySettings(); // once the controller and the traffic exist
	}
	if (Pawn && bHasPin)
	{
		// the pin: gone once you are there; until then a marker in the world and how far it is
		const float Dist = FVector2D::Distance(FVector2D(Pawn->GetActorLocation()), Pin);
		if (Dist < 2500.f)
		{
			bHasPin = false;
			Toast(this, FString::Printf(TEXT("You have reached %s"), *PinLabel), 1);
		}
		else
		{
			const FVector At = Project(FVector(Pin.X, Pin.Y, Pawn->GetActorLocation().Z + 250.f));
			const FString Far = Dist >= 100000.f ? FString::Printf(TEXT("%.1f km"), Dist / 100000.f) : FString::Printf(TEXT("%d m"), FMath::RoundToInt(Dist / 100.f));
			if (At.Z > 0.f && Screen == EScreen::None)
			{
				DrawRect(FLinearColor(0.2f, 0.75f, 1.f), At.X - 7.f * S, At.Y - 7.f * S, 14.f * S, 14.f * S);
				Text(Far, At.X, At.Y + 12.f * S, FLinearColor(0.2f, 0.75f, 1.f), Medium, 1.f, true);
			}
			Text(FString::Printf(TEXT("PIN  %s  %s"), *PinLabel.ToUpper(), *Far), MX + MapSize * 0.5f, MY + MapSize + 56.f * S, FLinearColor(0.2f, 0.75f, 1.f), Medium, 1.f, true);
			UpdatePinRoute(Pawn->GetActorLocation());
			Text(PinTurn, MX + MapSize * 0.5f, MY + MapSize + 82.f * S, Ink, Medium, 1.f, true);
		}
	}
	if (Pawn && bShowMinimap)
	{
		DrawMinimap(MX, MY, MapSize, Pawn->GetActorLocation(), Pawn->GetActorRotation().Yaw);
		if (Data)
		{
			const FString District = Data->DistrictAt(Pawn->GetActorLocation()).ToUpper();
			Panel(MX - 4.f * S, MY + MapSize + 10.f * S, MapSize + 8.f * S, 36.f * S);
			Text(District, MX + MapSize * 0.5f, MY + MapSize + 16.f * S, Yellow, Medium, 1.1f, true);
		}
	}

	// ---- floating text in the world
	for (int32 I = Floats.Num() - 1; I >= 0; --I)
	{
		FFloat& F = Floats[I];
		F.T -= Dt;
		if (F.T <= 0.f)
		{
			Floats.RemoveAt(I);
			continue;
		}
		const FVector P = Project(F.World + FVector(0, 0, (2.f - F.T) * 60.f));
		if (P.Z > 0.f)
		{
			Text(F.Text, P.X, P.Y, FLinearColor(1.f, 1.f, 1.f, FMath::Min(1.f, F.T)), Medium, 1.3f, true);
		}
	}

	// ---- toasts (top centre)
	float TY = Pad;
	for (int32 I = Toasts.Num() - 1; I >= 0; --I)
	{
		FToast& T = Toasts[I];
		T.T -= Dt;
		if (T.T <= 0.f)
		{
			Toasts.RemoveAt(I);
			continue;
		}
		float TW = 0.f, TH = 0.f;
		GetTextSize(T.Text, TW, TH, Medium, 1.1f * S);
		Panel(VW * 0.5f - TW * 0.5f - 18.f * S, TY, TW + 36.f * S, TH + 16.f * S, FLinearColor(0.f, 0.f, 0.f, 0.7f));
		DrawRect(T.Kind == 1 ? Good : T.Kind == 2 ? Bad : Yellow, VW * 0.5f - TW * 0.5f - 18.f * S, TY, 5.f * S, TH + 16.f * S);
		Text(T.Text, VW * 0.5f, TY + 8.f * S, Ink, Medium, 1.1f, true);
		TY += TH + 26.f * S;
	}

	// ---- prompt (bottom centre)
	if (PC && Dir && !Dir->Panel.bOpen && !Dir->Dialogue.bOpen)
	{
		const FString Prompt = PC->Prompt();
		if (!Prompt.IsEmpty())
		{
			float TW = 0.f, TH = 0.f;
			GetTextSize(Prompt, TW, TH, Medium, 1.3f * S);
			Panel(VW * 0.5f - TW * 0.5f - 24.f * S, VH - 140.f * S, TW + 48.f * S, TH + 20.f * S, FLinearColor(0.f, 0.f, 0.f, 0.7f));
			Text(Prompt, VW * 0.5f, VH - 130.f * S, Yellow, Medium, 1.3f, true);
		}
	}

	// ---- vehicle health (bottom left)
	if (const ANHVehicle* V = Cast<ANHVehicle>(Pawn))
	{
		const float X = Pad, Y = VH - Pad - 60.f * S, W = 300.f * S;
		Text(V->DisplayName(), X, Y - 30.f * S, Ink, Medium, 1.1f);
		Panel(X, Y, W, 14.f * S, FLinearColor(1.f, 1.f, 1.f, 0.15f));
		const float K = V->MaxHealth > 0.f ? V->Health / V->MaxHealth : 0.f;
		DrawRect(K > 0.35f ? Good : Bad, X, Y, W * K, 14.f * S);
		Text(FString::Printf(TEXT("%d km/h"), FMath::RoundToInt(FMath::Abs(V->Speed) * 0.036f)), X, Y + 22.f * S, Muted, Medium, 1.f);
	}

	// ---- the player's health, on foot, once it is not full
	if (const ANHCharacter* Me = Cast<ANHCharacter>(Pawn); Me && Me->Health < 99.5f)
	{
		const float W = 200.f * S, X = Pad, Y = VH - Pad - 46.f * S;
		Text(TEXT("Health"), X, Y - 26.f * S, Ink, Medium, 1.f);
		Panel(X, Y, W, 14.f * S, FLinearColor(1.f, 1.f, 1.f, 0.15f));
		DrawRect(Me->Health > 35.f ? Good : Bad, X, Y, W * Me->Health / 100.f, 14.f * S);
	}

	// ---- dialogue (bottom)
	const UNHAudioSubsystem* AudioSet = UNHAudioSubsystem::Get(this);
	const float SubK = AudioSet ? AudioSet->SubtitleScale() : 1.f;
	if (Dir && Dir->Dialogue.bOpen && Dir->Dialogue.Lines.IsValidIndex(Dir->Dialogue.Index))
	{
		// the subtitle size setting scales the speaker and the line; the panel grows up from the same bottom edge
		const float W = FMath::Min(1100.f * S, VW - 2.f * Pad), X = (VW - W) * 0.5f;
		const TArray<FString> Lines = Wrap(Dir->Dialogue.Lines[Dir->Dialogue.Index], W - 56.f * S, Medium, 1.25f * SubK);
		const float H = (116.f + 34.f * SubK * Lines.Num()) * S, Y = VH - 110.f * S - H;
		Panel(X, Y, W, H, FLinearColor(0.f, 0.f, 0.f, 0.78f));
		Text(Dir->Dialogue.Speaker, X + 28.f * S, Y + 16.f * S, Yellow, Medium, 1.2f * SubK);
		for (int32 I = 0; I < Lines.Num(); ++I)
		{
			Text(Lines[I], X + 28.f * S, Y + (22.f + 34.f * SubK * (I + 1)) * S, Ink, Medium, 1.25f * SubK);
		}
		Text(TEXT("E  next"), X + W - 120.f * S, Y + H - 38.f * S, Muted, Medium, 1.f);
	}

	// ---- choice panel (centre)
	if (Dir && Dir->Panel.bOpen)
	{
		const ANHGameDirector::FPanel& P = Dir->Panel;
		const float W = FMath::Min(760.f * S, VW - 2.f * Pad), LineH = 34.f * S;
		const float H = (90.f + 36.f * P.Lines.Num() + 52.f * P.Options.Num()) * S;
		const float X = (VW - W) * 0.5f, Y = FMath::Max(Pad, (VH - H) * 0.5f);
		Panel(X, Y, W, H, FLinearColor(0.05f, 0.05f, 0.05f, 0.9f));
		DrawRect(Yellow, X, Y, W, 5.f * S);
		Text(P.Title, X + 28.f * S, Y + 20.f * S, Yellow, Large, 1.1f);
		float Yy = Y + 70.f * S;
		for (const FString& L : P.Lines)
		{
			Text(L, X + 28.f * S, Yy, Ink, Medium, 1.1f);
			Yy += LineH;
		}
		Yy += 10.f * S;
		for (int32 I = 0; I < P.Options.Num(); ++I)
		{
			Panel(X + 22.f * S, Yy, W - 44.f * S, 44.f * S, FLinearColor(1.f, 1.f, 1.f, 0.08f));
			Text(FString::Printf(TEXT("%d   %s"), I + 1, *P.Options[I]), X + 38.f * S, Yy + 8.f * S, I == 0 ? Yellow : Ink, Medium, 1.15f);
			Yy += 52.f * S;
		}
	}

	// ---- the streaming overlay (F2): what World Partition has loaded round the player, under the engine's own cell map
	if (PC && PC->bStreamingOverlay)
	{
		int32 Loaded = 0, Loading = 0, Total = 0;
		for (const ULevelStreaming* Cell : GetWorld()->GetStreamingLevels())
		{
			++Total;
			Loaded += Cell && Cell->IsLevelLoaded() && Cell->IsLevelVisible() ? 1 : 0;
			Loading += Cell && Cell->HasLoadRequestPending() ? 1 : 0;
		}
		const float X = Pad, Y = VH - 230.f * S;
		Panel(X, Y, 560.f * S, 96.f * S, FLinearColor(0.f, 0.f, 0.f, 0.75f));
		Text(TEXT("STREAMING (F2)"), X + 16.f * S, Y + 8.f * S, Yellow, Medium, 1.f);
		Text(FString::Printf(TEXT("Loaded round you: %.0f m    Cells shown: %d of %d    Loading: %d"), PC->StreamingRadius / 100.f, Loaded, Total, Loading), X + 16.f * S, Y + 36.f * S, Ink, Medium, 0.95f);
		Text(TEXT("Roads, bridges and terrain are always loaded; buildings and trees by cell"), X + 16.f * S, Y + 62.f * S, Muted, Medium, 0.85f);
	}

	// ---- hotwiring: a marker sweeping a bar, to stop in the green three times
	if (const ANHCarTheft* Theft = ANHCarTheft::Get(this); Theft && Theft->bHotwiring)
	{
		const float W = 620.f * S, X = (VW - W) * 0.5f, Y = VH * 0.5f + 120.f * S, BarX = X + 24.f * S, BarW = W - 48.f * S;
		Panel(X, Y, W, 124.f * S, FLinearColor(0.f, 0.f, 0.f, 0.8f));
		Text(FString::Printf(TEXT("HOTWIRE   wire %d of 3   %.0f s"), FMath::Min(Theft->Hits + 1, 3), FMath::Max(0.f, Theft->TimeLeft)), X + 24.f * S, Y + 10.f * S, Theft->TimeLeft < 4.f ? Bad : Yellow, Medium, 1.1f);
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.14f), BarX, Y + 52.f * S, BarW, 26.f * S);
		DrawRect(Good, BarX + BarW * Theft->ZoneLo, Y + 52.f * S, BarW * (Theft->ZoneHi - Theft->ZoneLo), 26.f * S);
		DrawRect(FLinearColor::White, BarX + BarW * Theft->Marker - 3.f * S, Y + 46.f * S, 6.f * S, 38.f * S);
		Text(TEXT("E when the marker is in the green     F: give up"), X + W * 0.5f, Y + 90.f * S, Muted, Medium, 0.95f, true);
	}

	// ---- the phone: its icon by the clock when something is unseen, who is ringing, what is being said, and the handset
	if (ANHPhone* Phone = ANHPhone::Get(this))
	{
		if (const int32 Unseen = Phone->Unseen(); Unseen > 0 && !Phone->IsOpen())
		{
			const float IX = VW - Pad - 330.f * S, IY = Pad + 2.f * S;
			DrawRect(FLinearColor(0.05f, 0.05f, 0.06f, 0.9f), IX, IY, 22.f * S, 34.f * S);
			DrawRect(Yellow, IX + 3.f * S, IY + 4.f * S, 16.f * S, 22.f * S);
			Text(FString::FromInt(Unseen), IX + 30.f * S, IY + 6.f * S, Yellow, Medium, 1.f);
		}
		if (const FString Who = Phone->Ringing(); !Who.IsEmpty())
		{
			const float W = 620.f * S, X = (VW - W) * 0.5f, Y = 150.f * S;
			Panel(X, Y, W, 84.f * S, FLinearColor(0.f, 0.f, 0.f, 0.82f));
			DrawRect(Good, X, Y, 6.f * S, 84.f * S);
			Text(Who + TEXT(" is calling"), X + 26.f * S, Y + 12.f * S, Ink, Medium, 1.3f);
			Text(TEXT("Enter: answer     Backspace: decline"), X + 26.f * S, Y + 48.f * S, Muted, Medium, 1.f);
		}
		// calls have no recorded voices yet, so the line is shown whatever the Subtitles setting says (ShowSubtitle(false))
		if (!Phone->Subtitle.IsEmpty() && !(Dir && Dir->Dialogue.bOpen) && (!AudioSet || AudioSet->ShowSubtitle(false)))
		{
			const float W = FMath::Min(1000.f * S, VW - 2.f * Pad), X = (VW - W) * 0.5f;
			const TArray<FString> Lines = Wrap(Phone->Subtitle, W - 48.f * S, Medium, 1.2f * SubK);
			const float H = (16.f + 32.f * SubK * (Lines.Num() + 1)) * S, Y = VH - 170.f * S - H;
			Panel(X, Y, W, H, FLinearColor(0.f, 0.f, 0.f, 0.72f));
			Text(Phone->SubtitleSpeaker, X + 24.f * S, Y + 8.f * S, Yellow, Medium, SubK);
			for (int32 I = 0; I < Lines.Num(); ++I)
			{
				Text(Lines[I], X + 24.f * S, Y + (8.f + 32.f * SubK * (I + 1) - 4.f) * S, Ink, Medium, 1.2f * SubK);
			}
		}
		if (Screen == EScreen::None)
		{
			DrawPhone(VW, VH);
		}
	}

	// ---- the map, the pause menu, the inventory wheel: over everything else
	switch (Screen)
	{
	case EScreen::Map: DrawMapScreen(VW, VH); break;
	case EScreen::Menu: DrawMenu(VW, VH); break;
	case EScreen::Wheel: DrawWheel(VW, VH); break;
	default: break;
	}

	if (ShotAt > 0.f && PC)
	{
		if (ShotStage == 0 && Now >= ShotAt - ShotLead)
		{
			ShotStage = 1;
			if (ANHPhone* Phone = ANHPhone::Get(this); Phone && (ShotOpen == TEXT("phone") || ShotOpen == TEXT("contacts") || ShotOpen == TEXT("call") || ShotOpen == TEXT("dropam") || ShotOpen == TEXT("ride") || ShotOpen == TEXT("driver")))
			{
				Phone->DebugOpen(ShotOpen);
			}
			if (ANHCarTheft* Theft = ANHCarTheft::Get(this); Theft && (ShotOpen == TEXT("steal") || ShotOpen == TEXT("hotwin") || ShotOpen == TEXT("carjack") || ShotOpen == TEXT("sell") || ShotOpen == TEXT("roll") || ShotOpen == TEXT("climb")))
			{
				Theft->Debug(ShotOpen);
			}
			if (ShotOpen == TEXT("route"))
			{
				// a pin a few kilometres off, to see the directions; "routemap" the same with the map open
				if (const FNHBusStop* Yaba = Data ? Data->Stops.Find(TEXT("balogate")) : nullptr)
				{
					SetPin(Yaba->Kerb, Yaba->Name);
				}
				if (Dir)
				{
					Dir->Dialogue = ANHGameDirector::FDialogue();
				}
			}
			if (ShotOpen == TEXT("streaming")) // the F2 overlay, and its numbers in the log
			{
				PC->bStreamingOverlay = true;
				int32 Loaded = 0, Total = 0;
				for (const ULevelStreaming* Cell : GetWorld()->GetStreamingLevels())
				{
					++Total;
					Loaded += Cell && Cell->IsLevelLoaded() && Cell->IsLevelVisible() ? 1 : 0;
				}
				UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: test streaming: %d of %d cells loaded, radius %.0f m, player at %s"), Loaded, Total, PC->StreamingRadius / 100.f, Pawn ? *Pawn->GetActorLocation().ToCompactString() : TEXT("?"));
			}
			if (ShotOpen == TEXT("streets"))
			{
				if (ANHStreets* Streets = ANHStreets::Get(this))
				{
					Streets->DebugRepeatBanner();
					UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: test streets: here is '%s'; %d signs standing near the player"), Pawn ? *Streets->PlaceName(FVector2D(Pawn->GetActorLocation())) : TEXT(""), Streets->NumSigns());
				}
				if (Dir)
				{
					Dir->Dialogue = ANHGameDirector::FDialogue();
				}
			}
			if (ShotOpen == TEXT("streetmap"))
			{
				ToggleMap(); // close in, to see the side streets and their names
				MapZoom = 22.f;
			}
			if (ShotOpen == TEXT("credits"))
			{
				ToggleMenu();
				bMenuCredits = true;
			}
			if (ShotOpen == TEXT("baba") && Pawn)
			{
				// three metres in front of the old man, looking at him
				for (TActorIterator<ANHPerson> It(GetWorld()); It; ++It)
				{
					if (It->GetCast() == ENHCast::ElderMan)
					{
						const FVector Stand = It->GetActorLocation() + It->GetActorForwardVector() * 300.f + FVector(0.f, 0.f, 100.f);
						Pawn->SetActorLocation(Stand, false, nullptr, ETeleportType::TeleportPhysics);
						PC->SetControlRotation(FRotator(-5.f, (It->GetActorLocation() - Stand).Rotation().Yaw, 0.f));
						break;
					}
				}
			}
			if (ShotOpen == TEXT("audio"))
			{
				ToggleMenu();
				bMenuAudio = true;
			}
			if (ShotOpen == TEXT("controls"))
			{
				ToggleMenu();
				bMenuControls = true;
			}
			if (ShotOpen == TEXT("night"))
			{
				// the night-rain lighting, held, to see the traffic's lights
				PC->ConsoleCommand(TEXT("NHLighting NightRain"));
				if (Pawn)
				{
					PC->SetControlRotation(FRotator(-4.f, Pawn->GetActorRotation().Yaw + 150.f, 0.f)); // level, looking back along the road at the traffic
				}
				if (Dir)
				{
					Dir->SetManualLighting();
					Dir->Dialogue = ANHGameDirector::FDialogue();
				}
			}
			if (ShotOpen == TEXT("carshow") && Pawn)
			{
				// one of every vehicle type lined up ahead, photographed into the scratch folder given by -NHLookDir
				FString Folder;
				FParse::Value(FCommandLine::Get(), TEXT("NHLookDir="), Folder);
				PC->ConsoleCommand(FString::Printf(TEXT("NHCarShow %.0f %.0f %s"), Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y, *Folder));
			}
			if (ShotOpen == TEXT("map"))
			{
				ToggleMap();
				if (const FNHBusStop* Yaba = Data ? Data->Stops.Find(TEXT("balogate")) : nullptr)
				{
					SetPin(Yaba->Kerb, Yaba->Name);
				}
			}
			else if (ShotOpen == TEXT("mapzoom"))
			{
				ToggleMap();
				MapZoom = 8.f;
			}
			else if (ShotOpen == TEXT("menu"))
			{
				ToggleMenu();
			}
			else if (ShotOpen == TEXT("wheel"))
			{
				SetWheel(true);
				WheelSlot = 2;
			}
			else if (ShotOpen == TEXT("radiowheel") || ShotOpen == TEXT("music") || ShotOpen == TEXT("weapons"))
			{
				// the radio wheel in the nearest car; the phone's Music app with a station on; the three weapons in turn, the last left in hand
				ANHPlayerController* NHPC = Cast<ANHPlayerController>(PC);
				UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this);
				if (ShotOpen == TEXT("radiowheel") && NHPC)
				{
					ANHVehicle* Car = nullptr;
					for (TActorIterator<ANHVehicle> It(GetWorld()); It; ++It)
					{
						if (!It->GetController() && !It->GetSpec().bBike && Pawn && (!Car || FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation()) < FVector::DistSquared(Car->GetActorLocation(), Pawn->GetActorLocation())))
						{
							Car = *It;
						}
					}
					if (NHPC->EnterVehicle(Car) && Audio)
					{
						Audio->RadioPlay(0, Car);
						SetRadioWheel(true);
						WheelSlot = 0;
					}
				}
				else if (ShotOpen == TEXT("music"))
				{
					if (Audio)
					{
						Audio->RadioPlay(0, nullptr);
					}
					if (ANHPhone* Phone = ANHPhone::Get(this))
					{
						Phone->DebugOpen(TEXT("music"));
					}
				}
				else if (ANHCharacter* Char = Cast<ANHCharacter>(Pawn))
				{
					Char->Equip(TEXT("machete"));
					Char->Equip(TEXT("pistol"));
					Char->Equip(TEXT("ak47"));
					UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: in hand: %s"), *ANHCharacter::WeaponName(Char->Equipped()));
				}
			}
		}
		else if (ShotStage == 1 && Now >= ShotAt)
		{
			ShotStage = 2;
			PC->ConsoleCommand(TEXT("shot showui"));
		}
		else if (ShotStage == 2 && Now >= ShotAt + 2.5f)
		{
			ShotStage = 3;
			PC->ConsoleCommand(TEXT("quit"));
		}
	}
}
