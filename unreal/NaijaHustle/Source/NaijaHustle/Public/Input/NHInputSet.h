#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NHInputSet.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * Every Enhanced Input action and mapping context in the game, built in code.
 * Keeping bindings in source means they diff, review and merge like any other code, and nobody has to
 * hunt through .uasset files to find out what a key does. (Step 6 can bake these into assets if we
 * add player key remapping through Enhanced Input User Settings.)
 *
 * Contexts are stacked by ANHPlayerController: Global is always on; OnFoot / Vehicle / Menu swap.
 */
UCLASS()
class NAIJAHUSTLE_API UNHInputSet : public UObject
{
	GENERATED_BODY()

public:
	/** Creates all actions and contexts. Call once, right after NewObject. */
	void Build();

	// ---- contexts
	UPROPERTY() TObjectPtr<UInputMappingContext> Global;
	UPROPERTY() TObjectPtr<UInputMappingContext> OnFoot;
	UPROPERTY() TObjectPtr<UInputMappingContext> Vehicle;
	UPROPERTY() TObjectPtr<UInputMappingContext> Menu;

	// ---- on foot
	UPROPERTY() TObjectPtr<UInputAction> Move;        // Axis2D: WASD / arrows / left stick
	UPROPERTY() TObjectPtr<UInputAction> Look;        // Axis2D: mouse delta (already per-frame)
	UPROPERTY() TObjectPtr<UInputAction> LookStick;   // Axis2D: right stick (a rate, scaled by delta time)
	UPROPERTY() TObjectPtr<UInputAction> Jump;        // Space / A
	UPROPERTY() TObjectPtr<UInputAction> Sprint;      // Left Shift / L3
	UPROPERTY() TObjectPtr<UInputAction> Interact;    // F / Y: enter or steal a vehicle, talk
	UPROPERTY() TObjectPtr<UInputAction> Action;      // E / X: context action (call passengers, buy, pick up)

	// ---- vehicle
	UPROPERTY() TObjectPtr<UInputAction> Throttle;    // Axis1D: W / right trigger
	UPROPERTY() TObjectPtr<UInputAction> Brake;       // Axis1D: S / left trigger (reverses when stopped)
	UPROPERTY() TObjectPtr<UInputAction> Steer;       // Axis1D: A-D / left stick X
	UPROPERTY() TObjectPtr<UInputAction> Handbrake;   // Space / RB
	UPROPERTY() TObjectPtr<UInputAction> Horn;        // H / L3
	UPROPERTY() TObjectPtr<UInputAction> ExitVehicle; // F / Y
	UPROPERTY() TObjectPtr<UInputAction> Radio;       // R / D-pad right
	UPROPERTY() TObjectPtr<UInputAction> LookBehind;  // C / R3

	// ---- always on
	UPROPERTY() TObjectPtr<UInputAction> Phone;       // P or Up arrow / D-pad up
	UPROPERTY() TObjectPtr<UInputAction> Inventory;   // I or Tab / View (Back)
	UPROPERTY() TObjectPtr<UInputAction> QuickWheel;  // hold Q / hold LB
	UPROPERTY() TObjectPtr<UInputAction> Map;         // M / D-pad down
	UPROPERTY() TObjectPtr<UInputAction> Pause;       // Esc / Menu (Start)

	// ---- menus (phone, inventory, pause)
	UPROPERTY() TObjectPtr<UInputAction> UIBack;      // Esc or Backspace / B
};
