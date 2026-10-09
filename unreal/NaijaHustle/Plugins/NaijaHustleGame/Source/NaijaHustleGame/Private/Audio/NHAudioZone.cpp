#include "Audio/NHAudioZone.h"

#include "AttenuationVolumeComponent.h"
#include "Audio/NHAudioSubsystem.h"
#include "AudioGameplayVolumeComponent.h"
#include "AudioGameplayVolumeProxy.h"
#include "Components/BoxComponent.h"
#include "Core/NHGameData.h"
#include "Engine/World.h"
#include "FilterVolumeComponent.h"
#include "NaijaHustleGame.h"

ANHAudioZone::ANHAudioZone()
{
	PrimaryActorTick.bCanEverTick = false;
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(1000.f, 1000.f, 600.f));
	// the listener is looked for inside the box, so it has to answer queries; it stops nothing and is seen by no ray
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Overlap);
	Box->SetGenerateOverlapEvents(false);
	Box->SetCanEverAffectNavigation(false);

	Volume = CreateDefaultSubobject<UAudioGameplayVolumeComponent>(TEXT("Volume"));
	Volume->SetProxy(CreateDefaultSubobject<UAGVPrimitiveComponentProxy>(TEXT("ListenerInBox")));
	Filter = CreateDefaultSubobject<UFilterVolumeComponent>(TEXT("Filter"));
	Level = CreateDefaultSubobject<UAttenuationVolumeComponent>(TEXT("Level"));
	// these two are what make the engine look for the listener in the box at all, and they only count once active
	Filter->SetAutoActivate(true);
	Level->SetAutoActivate(true);
}

void ANHAudioZone::Setup(ENHAudioSpace InSpace, const FVector& HalfSize, int32 InPriority, bool bInIndoor, const FString& InLabel)
{
	Space = InSpace;
	Priority = InPriority;
	bIndoor = bInIndoor;
	Label = InLabel;
	Box->SetBoxExtent(HalfSize);
	ApplyIndoor();
	Volume->OnComponentDataChanged();
}

void ANHAudioZone::ApplyIndoor()
{
	// "exterior" is the sound outside the box heard from inside it, "interior" the sound inside heard from inside
	Filter->SetExteriorLPF(bIndoor ? 1800.f : MAX_FILTER_FREQUENCY, 0.5f);
	Filter->SetInteriorLPF(MAX_FILTER_FREQUENCY, 0.5f);
	Level->SetExteriorVolume(bIndoor ? 0.5f : 1.f, 0.5f);
	Level->SetInteriorVolume(1.f, 0.5f);
}

void ANHAudioZone::BeginPlay()
{
	Super::BeginPlay();
	ApplyIndoor();
	Volume->OnProxyEnter.AddDynamic(this, &ANHAudioZone::OnListenerEnter);
	Volume->OnProxyExit.AddDynamic(this, &ANHAudioZone::OnListenerExit);
}

void ANHAudioZone::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bInside)
	{
		OnListenerExit();
	}
	Super::EndPlay(Reason);
}

void ANHAudioZone::OnListenerEnter()
{
	bInside = true;
	if (UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this))
	{
		Audio->ZoneChanged(this, true);
	}
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: audio zone: in %s (%s)"), *Label, UNHAudioSubsystem::SpaceName(Space));
}

void ANHAudioZone::OnListenerExit()
{
	bInside = false;
	if (UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this))
	{
		Audio->ZoneChanged(this, false);
	}
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: audio zone: out of %s"), *Label);
}

void ANHAudioZone::SpawnCityZones(UWorld* World)
{
	const UNHGameData* Data = UNHGameData::Get(World);
	if (!World || !Data)
	{
		return;
	}
	const auto Spawn = [World](ENHAudioSpace InSpace, const FVector2D& Centre, const FVector2D& Half, float Yaw, int32 InPriority, const FString& InLabel)
	{
		if (ANHAudioZone* Zone = World->SpawnActor<ANHAudioZone>(ANHAudioZone::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), FVector(Centre, 1500.f))))
		{
			Zone->Setup(InSpace, FVector(Half, 2500.f), InPriority, false, InLabel);
			UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: audio zone: %s (%s) at %.0f, %.0f, %.0f x %.0f m"), *InLabel, UNHAudioSubsystem::SpaceName(InSpace), Centre.X, Centre.Y, Half.X / 50.f, Half.Y / 50.f);
		}
	};
	// the motor park: the box round its bays, and 25 m of yard past them
	if (Data->ParkBays.Num() > 0)
	{
		FBox2D Bays(ForceInit);
		for (const FNHParkBay& Bay : Data->ParkBays)
		{
			Bays += Bay.Pos;
		}
		Bays += Data->Park;
		Spawn(ENHAudioSpace::MotorPark, Bays.GetCenter(), Bays.GetExtent() + FVector2D(2500.f), 0.f, 10, TEXT("the motor park"));
	}
	// the market along the road at the Oshodi stop, in the real city. A stand-in box until the market itself is placed (audio brief, step 2).
	if (Data->bRealCity)
	{
		if (const FNHBusStop* Stop = Data->Stops.Find(TEXT("oshoja")))
		{
			const FVector2D Along = (Stop->Wait - Stop->Kerb).GetSafeNormal();
			// from the far kerb of the road to 55 m back from it, 100 m along it: the stop itself is inside
			Spawn(ENHAudioSpace::Market, Stop->Wait + Along * 1500.f, FVector2D(4000.f, 5000.f), FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), 20, TEXT("Oshodi market"));
		}
	}
}
