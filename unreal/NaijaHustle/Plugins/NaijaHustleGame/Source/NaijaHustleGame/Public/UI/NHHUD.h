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

	void BuildMinimap();
	void DrawMinimap(float X, float Y, float Size, const FVector& Player, float Yaw);
	void DrawStars(float X, float Y, int32 Stars, float S);
	void Panel(float X, float Y, float W, float H, const FLinearColor& Color = FLinearColor(0.f, 0.f, 0.f, 0.62f));
	float Text(const FString& Str, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale, bool bCentre = false, bool bShadow = true);
	float S = 1.f; // UI scale (1 at 1080p)
};
