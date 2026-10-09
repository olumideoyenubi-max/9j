#pragma once

#include "CoreMinimal.h"
#include "Audio/NHAudioTypes.h"
#include "GameFramework/Actor.h"
#include "NHAudioZone.generated.h"

class UAttenuationVolumeComponent;
class UAudioGameplayVolumeComponent;
class UBoxComponent;
class UFilterVolumeComponent;

/**
 * A box of the city that sounds like somewhere: a market, a motor park, a shop, a tunnel. It is an Audio Gameplay
 * Volume: while the listener is inside, UNHAudioSubsystem gives the World submix this zone's space (reverb and EQ).
 * An indoor zone also dulls and quietens what is outside it while you are in, and what is inside while you are out.
 *
 * Place one in a level and set Space, or let the game make the city's own (SpawnCityZones: the motor park from its
 * bays, and the market by the Oshodi stop). Where zones overlap the higher Priority wins.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHAudioZone : public AActor
{
	GENERATED_BODY()

public:
	ANHAudioZone();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio Zone") ENHAudioSpace Space = ENHAudioSpace::Market;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio Zone") int32 Priority = 0;
	/** Walls and a roof: the street is muffled from in here, and this place from the street */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio Zone") bool bIndoor = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio Zone") FString Label;

	/** Sets the box's half-size (cm) and the rest, after spawning */
	void Setup(ENHAudioSpace InSpace, const FVector& HalfSize, int32 InPriority, bool bInIndoor, const FString& InLabel);
	/** The zones the city's data gives: call once when play starts */
	static void SpawnCityZones(UWorld* World);
	bool ListenerInside() const { return bInside; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Audio Zone") TObjectPtr<UBoxComponent> Box;
	UPROPERTY(VisibleAnywhere, Category = "Audio Zone") TObjectPtr<UAudioGameplayVolumeComponent> Volume;
	UPROPERTY(VisibleAnywhere, Category = "Audio Zone") TObjectPtr<UFilterVolumeComponent> Filter;
	UPROPERTY(VisibleAnywhere, Category = "Audio Zone") TObjectPtr<UAttenuationVolumeComponent> Level;
	bool bInside = false;
	void ApplyIndoor();
	UFUNCTION() void OnListenerEnter();
	UFUNCTION() void OnListenerExit();
};
