#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NHBridgeTest.generated.h"

class ANHVehicle;

/**
 * The bridge test: each vehicle type in turn is driven, through the same pedals and wheel a player uses, from the
 * road before a bridge, over the whole of it, and back, and must not stop, fall or get stuck.
 *
 *   Scripts/mac.sh city -NHBridgeTest [-NHBridgeTypes=danfo,sedan,keke] [-NHBridgeIndex=0]
 *                       [-NHBridgeName="Eko Bridge"] [-NHBridgeSpeed=2600] [-NHBridgeLead=2000]
 *
 * It picks the bridge nearest the player (or the Nth nearest) that the road graph can route over in both directions,
 * logs one "[bridgetest]" line a leg with PASS or FAIL and why (where it stuck, the slope there, how fast it was
 * going), then a RESULT line, and quits. Run it after any change to the map or the vehicles.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHBridgeTest : public AActor
{
	GENERATED_BODY()

public:
	ANHBridgeTest();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	TArray<FName> Types;
	TArray<FVector2D> Out, Back;
	FString BridgeName;
	int32 TypeIndex = -1, Leg = 0, Passed = 0, Failed = 0;
	TWeakObjectPtr<ANHVehicle> Car;
	float Along = 0.f, LegTime = 0.f, StuckTime = 0.f, Top = 0.f, Slowest = 0.f, StartDelay = 6.f, SteepestSeen = 0.f;
	FVector LastAt = FVector::ZeroVector;
	bool bStarted = false;
	/** The most its middle was above what is under it, cm (about 100 on the ground), where, and seconds spent well clear of it */
	float HighestOff = 0.f, HighestOffAlong = 0.f, AirTime = 0.f;
	int32 PutBack = 0;
	bool FindBridge(int32 Skip);
	void NextLeg();
	void EndLeg(bool bPass, const FString& Why);
	const TArray<FVector2D>& Line() const { return Leg == 0 ? Out : Back; }
};
