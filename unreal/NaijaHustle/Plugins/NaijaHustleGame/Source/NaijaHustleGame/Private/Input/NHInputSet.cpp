#include "Input/NHInputSet.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"

namespace NHInput
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type = EInputActionValueType::Boolean)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, FName(Name));
		Action->ValueType = Type;
		return Action;
	}

	// Modifiers are owned by the context they're used in.
	UInputModifier* Swizzle(UObject* Ctx) // (x,0,0) -> (0,x,0): turns a single key into the Y axis
	{
		UInputModifierSwizzleAxis* M = NewObject<UInputModifierSwizzleAxis>(Ctx);
		M->Order = EInputAxisSwizzle::YXZ;
		return M;
	}
	UInputModifier* Negate(UObject* Ctx, bool bX, bool bY)
	{
		UInputModifierNegate* M = NewObject<UInputModifierNegate>(Ctx);
		M->bX = bX; M->bY = bY; M->bZ = false;
		return M;
	}
	UInputModifier* DeadZone(UObject* Ctx, float Lower = 0.2f)
	{
		UInputModifierDeadZone* M = NewObject<UInputModifierDeadZone>(Ctx);
		M->LowerThreshold = Lower;
		return M;
	}

	/** WASD + arrows + left stick onto one Axis2D action (X = right, Y = forward). */
	void MapWASD(UInputMappingContext* Ctx, UInputAction* Action)
	{
		for (const auto& Keys : { TArray<FKey>{ EKeys::W, EKeys::S, EKeys::A, EKeys::D }, TArray<FKey>{ EKeys::Up, EKeys::Down, EKeys::Left, EKeys::Right } })
		{
			Ctx->MapKey(Action, Keys[0]).Modifiers.Add(Swizzle(Ctx));
			{ FEnhancedActionKeyMapping& M = Ctx->MapKey(Action, Keys[1]); M.Modifiers.Add(Swizzle(Ctx)); M.Modifiers.Add(Negate(Ctx, true, true)); }
			Ctx->MapKey(Action, Keys[2]).Modifiers.Add(Negate(Ctx, true, false));
			Ctx->MapKey(Action, Keys[3]);
		}
		Ctx->MapKey(Action, EKeys::Gamepad_Left2D).Modifiers.Add(DeadZone(Ctx));
	}
}

void UNHInputSet::Build()
{
	using namespace NHInput;
	using EV = EInputActionValueType;

	Global = NewObject<UInputMappingContext>(this, TEXT("IMC_Global"));
	OnFoot = NewObject<UInputMappingContext>(this, TEXT("IMC_OnFoot"));
	Vehicle = NewObject<UInputMappingContext>(this, TEXT("IMC_Vehicle"));
	Menu = NewObject<UInputMappingContext>(this, TEXT("IMC_Menu"));

	// ---------------------------------------------------------------- on foot
	Move = MakeAction(this, TEXT("IA_Move"), EV::Axis2D);
	Look = MakeAction(this, TEXT("IA_Look"), EV::Axis2D);
	LookStick = MakeAction(this, TEXT("IA_LookStick"), EV::Axis2D);
	Jump = MakeAction(this, TEXT("IA_Jump"));
	Sprint = MakeAction(this, TEXT("IA_Sprint"));
	Interact = MakeAction(this, TEXT("IA_Interact"));
	Action = MakeAction(this, TEXT("IA_Action"));

	MapWASD(OnFoot, Move);
	// Mouse up is +Y; negating Y gives the usual "mouse up looks up" with the engine's pitch convention.
	OnFoot->MapKey(Look, EKeys::Mouse2D).Modifiers.Add(Negate(OnFoot, false, true));
	{ FEnhancedActionKeyMapping& M = OnFoot->MapKey(LookStick, EKeys::Gamepad_Right2D); M.Modifiers.Add(DeadZone(OnFoot)); M.Modifiers.Add(Negate(OnFoot, false, true)); }
	OnFoot->MapKey(Jump, EKeys::SpaceBar);
	OnFoot->MapKey(Jump, EKeys::Gamepad_FaceButton_Bottom);
	OnFoot->MapKey(Sprint, EKeys::LeftShift);
	OnFoot->MapKey(Sprint, EKeys::Gamepad_LeftThumbstick);
	OnFoot->MapKey(Interact, EKeys::F);
	OnFoot->MapKey(Interact, EKeys::Gamepad_FaceButton_Top);
	OnFoot->MapKey(Action, EKeys::E);
	OnFoot->MapKey(Action, EKeys::Gamepad_FaceButton_Left);

	// ---------------------------------------------------------------- vehicle
	Throttle = MakeAction(this, TEXT("IA_Throttle"), EV::Axis1D);
	Brake = MakeAction(this, TEXT("IA_Brake"), EV::Axis1D);
	Steer = MakeAction(this, TEXT("IA_Steer"), EV::Axis1D);
	Handbrake = MakeAction(this, TEXT("IA_Handbrake"));
	Horn = MakeAction(this, TEXT("IA_Horn"));
	ExitVehicle = MakeAction(this, TEXT("IA_ExitVehicle"));
	Radio = MakeAction(this, TEXT("IA_Radio"));
	LookBehind = MakeAction(this, TEXT("IA_LookBehind"));
	Headlights = MakeAction(this, TEXT("IA_Headlights"));
	CabinView = MakeAction(this, TEXT("IA_CabinView"));

	Vehicle->MapKey(Throttle, EKeys::W);
	Vehicle->MapKey(Throttle, EKeys::Up);
	Vehicle->MapKey(Throttle, EKeys::Gamepad_RightTriggerAxis);
	Vehicle->MapKey(Brake, EKeys::S);
	Vehicle->MapKey(Brake, EKeys::Down);
	Vehicle->MapKey(Brake, EKeys::Gamepad_LeftTriggerAxis);
	Vehicle->MapKey(Steer, EKeys::D);
	Vehicle->MapKey(Steer, EKeys::Right);
	Vehicle->MapKey(Steer, EKeys::A).Modifiers.Add(Negate(Vehicle, true, false));
	Vehicle->MapKey(Steer, EKeys::Left).Modifiers.Add(Negate(Vehicle, true, false));
	Vehicle->MapKey(Steer, EKeys::Gamepad_LeftX).Modifiers.Add(DeadZone(Vehicle, 0.12f));
	Vehicle->MapKey(Handbrake, EKeys::SpaceBar);
	Vehicle->MapKey(Handbrake, EKeys::Gamepad_RightShoulder);
	Vehicle->MapKey(Horn, EKeys::H);
	Vehicle->MapKey(Horn, EKeys::Gamepad_LeftThumbstick);
	Vehicle->MapKey(ExitVehicle, EKeys::F);
	Vehicle->MapKey(ExitVehicle, EKeys::Gamepad_FaceButton_Top);
	Vehicle->MapKey(Radio, EKeys::R);
	Vehicle->MapKey(Radio, EKeys::Gamepad_DPad_Right);
	Vehicle->MapKey(LookBehind, EKeys::C);
	Vehicle->MapKey(CabinView, EKeys::V);
	Vehicle->MapKey(Headlights, EKeys::K); // every gamepad button is taken
	Vehicle->MapKey(LookBehind, EKeys::Gamepad_RightThumbstick);
	// Free look while driving uses the same actions as on foot
	Vehicle->MapKey(Look, EKeys::Mouse2D).Modifiers.Add(Negate(Vehicle, false, true));
	{ FEnhancedActionKeyMapping& M = Vehicle->MapKey(LookStick, EKeys::Gamepad_Right2D); M.Modifiers.Add(DeadZone(Vehicle)); M.Modifiers.Add(Negate(Vehicle, false, true)); }
	// E stays useful in a vehicle: the conductor's "call passengers" at a bus stop
	Vehicle->MapKey(Action, EKeys::E);
	Vehicle->MapKey(Action, EKeys::Gamepad_FaceButton_Left);

	// ---------------------------------------------------------------- always on
	Phone = MakeAction(this, TEXT("IA_Phone"));
	Inventory = MakeAction(this, TEXT("IA_Inventory"));
	QuickWheel = MakeAction(this, TEXT("IA_QuickWheel")); // bind Started (open) / Completed (pick + close)
	Map = MakeAction(this, TEXT("IA_Map"));
	Pause = MakeAction(this, TEXT("IA_Pause"));
	CycleLighting = MakeAction(this, TEXT("IA_CycleLighting"));
	LightingMenu = MakeAction(this, TEXT("IA_LightingMenu"));
	Choice1 = MakeAction(this, TEXT("IA_Choice1"));
	Choice2 = MakeAction(this, TEXT("IA_Choice2"));
	Choice3 = MakeAction(this, TEXT("IA_Choice3"));
	Choice4 = MakeAction(this, TEXT("IA_Choice4"));

	Global->MapKey(Phone, EKeys::P);
	Global->MapKey(Phone, EKeys::Gamepad_DPad_Up);
	Global->MapKey(Inventory, EKeys::I);
	Global->MapKey(Inventory, EKeys::Tab);
	Global->MapKey(Inventory, EKeys::Gamepad_Special_Left);
	Global->MapKey(QuickWheel, EKeys::Q);
	Global->MapKey(QuickWheel, EKeys::Gamepad_LeftShoulder);
	Global->MapKey(Map, EKeys::M);
	Global->MapKey(Map, EKeys::Gamepad_DPad_Down);
	Global->MapKey(Pause, EKeys::Escape);
	Global->MapKey(Pause, EKeys::Gamepad_Special_Right);
	Global->MapKey(CycleLighting, EKeys::L);
	Global->MapKey(LightingMenu, EKeys::F1);
	// choices: number keys, or the gamepad face buttons are taken, so the d-pad left/right and shoulders
	Global->MapKey(Choice1, EKeys::One);
	Global->MapKey(Choice2, EKeys::Two);
	Global->MapKey(Choice3, EKeys::Three);
	Global->MapKey(Choice4, EKeys::Four);
	Global->MapKey(Choice1, EKeys::Gamepad_DPad_Left);
	Global->MapKey(Choice2, EKeys::Gamepad_DPad_Right);

	// ---------------------------------------------------------------- menus
	UIBack = MakeAction(this, TEXT("IA_UIBack"));
	Menu->MapKey(UIBack, EKeys::Escape);
	Menu->MapKey(UIBack, EKeys::BackSpace);
	Menu->MapKey(UIBack, EKeys::Gamepad_FaceButton_Right);
}
