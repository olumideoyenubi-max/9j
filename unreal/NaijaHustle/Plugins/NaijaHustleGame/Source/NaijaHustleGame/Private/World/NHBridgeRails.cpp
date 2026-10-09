#include "World/NHBridgeRails.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Core/NHGameData.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "NaijaHustleGame.h"
#include "TimerManager.h"

void UNHBridgeRails::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.IsGameWorld())
	{
		// once the level's own pieces are in and can be felt for
		FTimerHandle Later;
		InWorld.GetTimerManager().SetTimer(Later, FTimerDelegate::CreateUObject(this, &UNHBridgeRails::Build), 3.f, false);
	}
}

void UNHBridgeRails::Build()
{
	UWorld* World = GetWorld();
	const UNHGameData* Data = UNHGameData::Get(World);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!World || !Data || !Data->bRealCity || !Cube || Holder)
	{
		return;
	}
	const double Started = FPlatformTime::Seconds();
	const auto TopAt = [World](const FVector2D& At, float& OutZ)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByObjectType(Hit, FVector(At, 12000.f), FVector(At, -3000.f), FCollisionObjectQueryParams(ECC_WorldStatic)))
		{
			return false;
		}
		OutZ = Hit.ImpactPoint.Z;
		return true;
	};
	const float Piece = 1000.f; // a length of wall, cm
	TArray<FTransform> Walls;
	for (const FNHRoadWay& Way : Data->RoadWays)
	{
		if (!Way.bBridge || Way.Class > 4)
		{
			continue;
		}
		float Run = 0.f, Next = Piece * 0.5f;
		for (int32 I = 1; I < Way.Nodes.Num(); ++I)
		{
			const FVector2D P0 = Data->RoadNodes[Way.Nodes[I - 1]], P1 = Data->RoadNodes[Way.Nodes[I]];
			const float Seg = FVector2D::Distance(P0, P1);
			const FVector2D Dir = (P1 - P0).GetSafeNormal(), Side(-Dir.Y, Dir.X);
			for (; Next <= Run + Seg; Next += Piece)
			{
				const FVector2D At = P0 + Dir * (Next - Run);
				float Deck = 0.f, Ahead = 0.f, Behind = 0.f;
				if (!TopAt(At, Deck))
				{
					continue;
				}
				// the wall leans with the deck on a ramp
				const float Slope = TopAt(At + Dir * Piece * 0.5f, Ahead) && TopAt(At - Dir * Piece * 0.5f, Behind) && FMath::Abs(Ahead - Behind) < 300.f ? Ahead - Behind : 0.f;
				for (const float Sign : { -1.f, 1.f })
				{
					// outward from the middle of the carriageway until the surface drops away; a surface that rises instead
					// (a kerb, another deck) or carries on level for 22 m is not an open edge
					float Edge = 0.f, Z = 0.f;
					for (float Off = 200.f; Off <= 2200.f; Off += 100.f)
					{
						if (!TopAt(At + Side * Sign * Off, Z) || Deck - Z > 120.f)
						{
							Edge = Off - 100.f;
							break;
						}
						if (Z - Deck > 60.f)
						{
							break;
						}
					}
					if (Edge > 0.f)
					{
						const FRotator Turn(FMath::RadiansToDegrees(FMath::Atan2(Slope, Piece)), FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.f);
						Walls.Add(FTransform(Turn, FVector(At + Side * Sign * (Edge - 25.f), Deck + 42.f), FVector((Piece + 30.f) / 100.f, 0.28f, 0.9f)));
					}
				}
			}
			Run += Seg;
		}
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Holder = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!Holder || Walls.Num() == 0)
	{
		UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: bridge parapets: none stood (%d open edges found)"), Walls.Num());
		return;
	}
	UInstancedStaticMeshComponent* Rails = NewObject<UInstancedStaticMeshComponent>(Holder);
	Rails->SetStaticMesh(Cube);
	Rails->SetMobility(EComponentMobility::Static);
	Rails->SetCollisionProfileName(TEXT("BlockAll"));
	Rails->SetCanEverAffectNavigation(false);
	Holder->SetRootComponent(Rails);
	Rails->RegisterComponent();
	Rails->AddInstances(Walls, false, true);
	Made = Walls.Num();
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: bridge parapets: %d lengths of %.0f m stood along open deck edges, in %.2f s"), Made, Piece / 100.f, FPlatformTime::Seconds() - Started);
}
