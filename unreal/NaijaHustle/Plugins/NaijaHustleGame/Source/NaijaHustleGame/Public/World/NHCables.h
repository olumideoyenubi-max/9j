#pragma once

#include "CoreMinimal.h"
#include "CableComponent.h"
#include "GameFramework/Actor.h"
#include "NHCables.generated.h"

/** One overhead cable between two points, world cm */
USTRUCT(BlueprintType)
struct FNHCableSpan
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cable") FVector Start = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cable") FVector End = FVector::ZeroVector;
	/** Extra length over the straight distance (0.05 = 5 % longer), which is what makes it sag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cable", meta = (ClampMin = "0", ClampMax = "0.5")) float Slack = 0.05f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cable", meta = (ClampMin = "0.5")) float Width = 3.f;
};

/** A cable that sags into place and then stops simulating, so hundreds of them cost nothing per frame */
UCLASS()
class NAIJAHUSTLEGAME_API UNHCableComponent : public UCableComponent
{
	GENERATED_BODY()

public:
	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "Cable") float SettleSeconds = 3.f;

private:
	float Settled = 0.f;
};

/**
 * The overhead cables of one city tile: power lines strung pole to pole and service drops from poles to
 * house walls. The spans come from the city data (Scripts/build_street_block.py); each one is an engine
 * cable component with some slack.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHCables : public AActor
{
	GENERATED_BODY()

public:
	ANHCables();

	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cables") TArray<FNHCableSpan> Spans;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cables") FLinearColor Color = FLinearColor(0.012f, 0.012f, 0.014f);
	/** Cables further away than this are not drawn (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cables") float DrawDistance = 14000.f;

	/** Removes every cable and strings them again from Spans. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Cables")
	void Rebuild();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Cables") TObjectPtr<USceneComponent> Root;
};
