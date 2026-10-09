#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "NHHUD.generated.h"

class UTexture2D;
class UFont;

/**
 * The game's HUD, drawn on the canvas (no widget assets needed): naira and wanted stars, clock and day,
 * a minimap of the shared city with the route's stops and the objective marker, the job card, the
 * conductor's shift status, prompts, toasts, floating text (fares, horns), Baba Driver's dialogue and
 * numbered choice panels (change, agbero, routes, summary). Step 6 rebuilds it in UMG.
 *
 * It also draws three screens of its own, opened by ANHPlayerController's keys:
 *   the map (M): the whole city, zoomed with the mouse wheel and moved with the arrow keys. Click to pin a place; the
 *     pin shows on the minimap, in the world and as a distance, until you get there or right-click it away.
 *   the pause menu (Esc): character, lighting, traffic, look speed, resolution, minimap, quit. Up and down pick a
 *     line, left and right change it. The settings are remembered.
 *   the inventory wheel (hold Tab, point with the mouse, let go): phone (the map), wardrobe (next character), car
 *     keys (pins the vehicle you last drove), torch (headlights when driving), wallet, and hail (the nearest passing
 *     vehicle pulls up for you).
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Short message at the top of the screen. Kind: 0 normal, 1 good, 2 bad. */
	static void Toast(const UObject* WorldContext, const FString& Text, int32 Kind = 0);
	/** Text that floats up from a point in the world */
	static void Floater(const UObject* WorldContext, const FVector& World, const FString& Text);
	static ANHHUD* Get(const UObject* WorldContext);

	enum class EScreen : uint8 { None, Map, Menu, Wheel };
	void ToggleMap();
	void ToggleMenu();
	/** P: the phone (ANHPhone), drawn beside the minimap; it does not stop the game */
	void TogglePhone();
	/** Backspace: back a page on the phone, or declines a call */
	void Back();
	void SetWheel(bool bOpen);
	/** Arrow keys: a line up or down and a value left or right in the menu; moving the map; turning the wheel */
	void Nav(int32 DX, int32 DY);
	void Accept();
	/** A mouse click: on the map the left button pins the place under the cursor and the right clears the pin */
	void Click(bool bRight);
	void Zoom(int32 Dir);
	EScreen GetScreen() const { return Screen; }

	/** Marks a place for the player to head for */
	void SetPin(const FVector2D& World, const FString& Label);
	void ClearPin() { bHasPin = false; }
	bool bHasPin = false;
	FVector2D Pin = FVector2D::ZeroVector;

protected:
	virtual void BeginPlay() override;

private:
	struct FToast { FString Text; int32 Kind = 0; float T = 0.f; };
	struct FFloat { FVector World; FString Text; float T = 0.f; };
	TArray<FToast> Toasts;
	TArray<FFloat> Floats;
	int32 CashDelta = 0;
	float CashDeltaT = 0.f;
	float LastTime = 0.f;
	FDelegateHandle EarnHandle;

	UPROPERTY(Transient) TObjectPtr<UTexture2D> MapTex;

	// ---- the map, the pause menu and the inventory wheel
	EScreen Screen = EScreen::None;
	void Open(EScreen NewScreen);
	void DrawMapScreen(float VW, float VH);
	void DrawMenu(float VW, float VH);
	void DrawWheel(float VW, float VH);
	void DrawPhone(float VW, float VH);
	/** Splits text into lines no wider than MaxWidth at that font and scale */
	TArray<FString> Wrap(const FString& Str, float MaxWidth, UFont* Font, float Scale);
	/** The roads of the real city inside a square of the screen showing Span cm of the world from Corner */
	void DrawRoads(float X, float Y, float Size, const FVector2D& Corner, float Span, float Thick);
	void BuildCityMap();
	/** The whole real city's roads as a picture, for the map zoomed out; its world corner and size */
	UPROPERTY(Transient) TObjectPtr<UTexture2D> CityTex;
	FVector2D CityCorner = FVector2D::ZeroVector;
	float CitySpan = 1.f;
	FVector2D MapCentre = FVector2D::ZeroVector;
	float MapZoom = 1.f;
	/** Where on the screen the map's square is, as drawn last */
	FVector2D MapAt = FVector2D::ZeroVector;
	float MapSide = 1.f, MapSpan = 1.f;
	FString PinLabel;
	int32 MenuLine = 0;
	int32 WheelSlot = -1;
	FString MenuValue(int32 Line) const;
	void MenuChange(int32 Line, int32 Dir);
	void UseWheel(int32 Slot);
	// settings, kept in GameUserSettings.ini under [NaijaHustle]
	int32 TrafficLevel = 2;
	int32 ScreenPercent = 100;
	bool bShowMinimap = true;
	void LoadSettings();
	void SaveSettings() const;
	void ApplySettings();
	bool bSettingsApplied = false;
	/** -NHHudShot=Seconds [-NHHudOpen=map|menu|wheel|phone|contacts|call|dropam|ride|driver]: opens that screen, saves a screenshot with the HUD on after that long, and quits */
	float ShotAt = 0.f;
	/** How long before the screenshot the screen is opened (-NHHudLead=Seconds), for things that take time, like a ride arriving */
	float ShotLead = 2.f;
	FString ShotOpen;
	int32 ShotStage = 0;

	void BuildMinimap();
	void DrawMinimap(float X, float Y, float Size, const FVector& Player, float Yaw);
	void DrawStars(float X, float Y, int32 Stars, float S);
	void Panel(float X, float Y, float W, float H, const FLinearColor& Color = FLinearColor(0.f, 0.f, 0.f, 0.62f));
	float Text(const FString& Str, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale, bool bCentre = false, bool bShadow = true);
	float S = 1.f; // UI scale (1 at 1080p)
};
