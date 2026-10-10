#include "Vehicles/NHVehicle.h"

#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Audio/NHAudioSubsystem.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/NHInputSet.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NaijaHustleGame.h"
#include "Player/NHCharacter.h"
#include "Player/NHPlayerController.h"
#include "UI/NHHUD.h"
#include "Vehicles/NHVehicleDynamicsComponent.h"
#include "Vehicles/NHVehicleMaterialComponent.h"
#include "World/NHShapes.h"

namespace NHCar
{
	const FNHSurface Glass(FLinearColor(0.02f, 0.025f, 0.03f), 0.06f, 0.4f);
	const FNHSurface Rubber(FLinearColor(0.02f, 0.02f, 0.02f), 0.8f, 0.6f);
	const FNHSurface Black(FLinearColor(0.015f, 0.015f, 0.015f), 0.5f, 0.6f, 0.2f);
	const FNHSurface Chrome(FLinearColor(0.6f, 0.6f, 0.62f), 0.25f, 0.4f, 0.9f);
	const FNHSurface HeadLamp(FLinearColor(1.f, 0.95f, 0.85f), 0.2f, 0.f, 0.f, 200.f, 1.f);
	const FNHSurface TailLamp(FLinearColor(0.8f, 0.02f, 0.01f), 0.3f, 0.f, 0.f, 40.f, 0.f);
}

ANHVehicle::ANHVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::Disabled;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetCollisionProfileName(UCollisionProfile::Vehicle_ProfileName);
	Box->SetBoxExtent(FVector(250.f, 110.f, 60.f));
	RootComponent = Box;

	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(Box);

	Arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Arm"));
	Arm->SetupAttachment(Box);
	Arm->TargetArmLength = 900.f;
	Arm->SocketOffset = FVector(0.f, 0.f, 160.f);
	Arm->SetRelativeRotation(FRotator(-12.f, 0.f, 0.f));
	Arm->bUsePawnControlRotation = false;
	Arm->bEnableCameraLag = true;
	Arm->bEnableCameraRotationLag = true;
	Arm->CameraLagSpeed = 8.f;
	Arm->CameraRotationLagSpeed = 5.f;
	Arm->bDoCollisionTest = true;

	PaintFx = CreateDefaultSubobject<UNHVehicleMaterialComponent>(TEXT("PaintFx"));
	Dynamics = CreateDefaultSubobject<UNHVehicleDynamicsComponent>(TEXT("Dynamics"));
	// while the player drives it, the vehicle itself asks World Partition for the city around it (see ANHPlayerController::UpdateStreaming)
	Streaming = CreateDefaultSubobject<UWorldPartitionStreamingSourceComponent>(TEXT("Streaming"));
	Streaming->DisableStreamingSource();
	Streaming->Priority = EStreamingSourcePriority::High;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Arm, USpringArmComponent::SocketName);
	Camera->FieldOfView = 75.f;
}

void ANHVehicle::BeginPlay()
{
	Super::BeginPlay();
	if (const UNHGameData* Data = UNHGameData::Get(this))
	{
		Spec = Data->Spec(VehicleType);
	}
	MaxHealth = Health = Spec.Hp;
	BuildBody();
	PaintFx->InitializeEffects(); // the body exists now: pick up any paint and glass materials on it
	// settle onto the ground
	const FVector P = GetActorLocation();
	SetActorLocation(FVector(P.X, P.Y, GroundZ(P) + Clearance + HalfHeight));

	TArray<USceneComponent*> Hubs;
	for (USceneComponent* Hub : Wheels)
	{
		Hubs.Add(Hub);
	}
	Dynamics->Setup(Body, Hubs, Clearance + HalfHeight, Spec.bBike);
	if (static TSet<FName> Said; FParse::Param(FCommandLine::Get(), TEXT("NHVehicleSit")) && !Said.Contains(VehicleType))
	{
		// how far the lowest thing drawn (the tyres) is off the plane the vehicle is driven on: above it floats, below it is sunk
		Said.Add(VehicleType);
		TArray<USceneComponent*> Parts;
		Body->GetChildrenComponents(true, Parts);
		float Lowest = BIG_NUMBER;
		for (const USceneComponent* Part : Parts)
		{
			if (const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Part); Mesh && Mesh->GetStaticMesh())
			{
				Lowest = FMath::Min(Lowest, Mesh->Bounds.GetBox().Min.Z);
			}
		}
		UE_LOG(LogNHGame, Log, TEXT("[vehiclesit] %s: lowest point %.1f cm off the driving plane"), *VehicleType.ToString(), Lowest - (GetActorLocation().Z - Clearance - HalfHeight));
	}
}

float ANHVehicle::GroundZ(const FVector& At) const
{
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(NHVehicleGround), false, this);
	// As far down as a tall flyover: with only 8 m of reach, a vehicle that left a bridge's edge found no ground,
	// was told the ground was wherever it already was, and drove on through the air at deck height.
	if (GetWorld()->LineTraceSingleByObjectType(Hit, FVector(At.X, At.Y, At.Z + 300.f), FVector(At.X, At.Y, At.Z - 15000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Q))
	{
		return Hit.ImpactPoint.Z;
	}
	return At.Z - Clearance - HalfHeight;
}

void ANHVehicle::BuildBody()
{
	using namespace NHCar;
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	const float L = Spec.Length, W = Spec.Width;
	const FNHSurface PaintS(Paint, 0.35f, 0.5f, 0.3f);
	auto Piece = [this](ENHShape S, const FVector& At, const FVector& Size, const FNHSurface& Surf, const FRotator& R = FRotator::ZeroRotator)
	{
		return NHShapes::AddPiece(this, Body, S, At, Size, Surf, R);
	};
	auto Wheel = [this](float X, float Y, float Radius, float Width)
	{
		USceneComponent* Hub = NHShapes::AddPivot(this, Body, FVector(X, Y, Radius));
		NHShapes::AddPiece(this, Hub, ENHShape::Cylinder, FVector::ZeroVector, FVector(Radius * 2.f, Radius * 2.f, Width), NHCar::Rubber, FRotator(0.f, 0.f, 90.f));
		NHShapes::AddPiece(this, Hub, ENHShape::Box, FVector::ZeroVector, FVector(Radius * 1.1f, Width + 2.f, 6.f), NHCar::Chrome); // a spoke, so you can see it turn
		Wheels.Add(Hub);
	};
	auto Seated = [this](const FVector& At, float Scale)
	{
		const FNHSurface Skin(FLinearColor(0.08f, 0.04f, 0.025f), 0.55f), Shirt(FLinearColor(0.85f, 0.83f, 0.78f), 0.85f);
		SeatAt = At;
		DriverPieces.Add(NHShapes::AddPiece(this, Body, ENHShape::Box, At + FVector(0, 0, 30.f * Scale), FVector(24.f, 34.f, 52.f) * Scale, Shirt));
		DriverPieces.Add(NHShapes::AddPiece(this, Body, ENHShape::Sphere, At + FVector(2.f, 0, 72.f * Scale), FVector(20.f, 18.f, 24.f) * Scale, Skin));
	};

	float Height = 150.f;
	if (AddModel(Height))
	{
		// A real model: no blockout pieces. Its wheels are part of the one mesh, so unseen hubs stand where they
		// would be, to keep the suspension and the feel for the road surface working.
		const float Radius = Spec.bBike ? 30.f : 34.f;
		// the Pathmaster's model comes with one wheel lying at its middle and none at the corners: it gets the game's own
		const bool bNoWheels = VehicleType == TEXT("suv");
		for (int32 i = 0; i < (Spec.bBike ? 2 : 4); ++i)
		{
			if (bNoWheels)
			{
				Wheel(i < 2 ? 0.305f * L : -0.3f * L, (i % 2 ? 0.5f : -0.5f) * (W - 52.f), 41.f, 26.f);
				continue;
			}
			Wheels.Add(NHShapes::AddPivot(this, Body, FVector((Spec.bBike ? (i == 0) : (i < 2)) ? 0.31f * L : -0.31f * L, Spec.bBike ? 0.f : (i % 2 ? 0.5f : -0.5f) * (W - 50.f), Radius)));
		}
		// the driving seat is a guess from the size: left-hand drive, a little ahead of the middle, a third of the way up (over half in a van or truck cab)
		const bool bCabOver = VehicleType == TEXT("danfo") || VehicleType == TEXT("truck");
		SeatAt = Spec.bBike ? FVector(-L * 0.08f, 0.f, Height * 0.5f) : VehicleType == TEXT("keke") ? FVector(L * 0.12f, 0.f, Height * 0.36f) : FVector(bCabOver ? L * 0.5f - 90.f : L * 0.04f, -W * 0.2f, Height * (bCabOver ? 0.6f : 0.36f));
	}
	else if (VehicleType == TEXT("danfo"))
	{
		Height = 230.f;
		Piece(ENHShape::Box, FVector(0, 0, 115.f), FVector(L - 20.f, W - 12.f, 150.f), PaintS);                   // body
		Piece(ENHShape::Box, FVector(-10.f, 0, 205.f), FVector(L - 70.f, W - 24.f, 36.f), PaintS);              // roof
		Piece(ENHShape::Box, FVector(-10.f, 0, 160.f), FVector(L - 60.f, W - 8.f, 52.f), Glass);                // window band
		Piece(ENHShape::Box, FVector(L * 0.5f - 14.f, 0, 160.f), FVector(10.f, W - 30.f, 64.f), Glass);         // windscreen
		Piece(ENHShape::Box, FVector(0, 0, 98.f), FVector(L - 16.f, W - 8.f, 9.f), Black);                      // stripe
		Piece(ENHShape::Box, FVector(0, 0, 80.f), FVector(L - 16.f, W - 8.f, 6.f), Black);
		Piece(ENHShape::Box, FVector(L * 0.5f - 2.f, 0, 50.f), FVector(18.f, W - 4.f, 26.f), Black);            // bumpers
		Piece(ENHShape::Box, FVector(-L * 0.5f + 2.f, 0, 50.f), FVector(18.f, W - 4.f, 26.f), Black);
		Piece(ENHShape::Box, FVector(-12.f, W * 0.5f - 4.f, 125.f), FVector(110.f, 4.f, 140.f), Black);        // the sliding door, open
		for (float X : { -L * 0.3f, -L * 0.05f, L * 0.2f }) // roof rack with luggage
		{
			Piece(ENHShape::Box, FVector(X, 0, 232.f), FVector(8.f, W - 40.f, 6.f), Chrome);
		}
		Piece(ENHShape::Box, FVector(-L * 0.1f, 0, 250.f), FVector(L * 0.35f, W * 0.45f, 32.f), FNHSurface(FLinearColor(0.15f, 0.08f, 0.04f), 0.9f, 0.8f));
		for (float Y : { -W * 0.32f, W * 0.32f })
		{
			Piece(ENHShape::Box, FVector(L * 0.5f + 2.f, Y, 82.f), FVector(4.f, 34.f, 18.f), HeadLamp);
			Piece(ENHShape::Box, FVector(-L * 0.5f - 2.f, Y, 92.f), FVector(4.f, 22.f, 30.f), TailLamp);
		}
		// destination board above the windscreen
		Piece(ENHShape::Box, FVector(L * 0.5f - 8.f, 0, 205.f), FVector(4.f, W * 0.55f, 30.f), FNHSurface(FLinearColor(0.95f, 0.93f, 0.85f), 0.6f, 0.2f, 0.f, 6.f, 1.f));
		BoardText = NewObject<UTextRenderComponent>(this);
		BoardText->SetupAttachment(Body);
		BoardText->SetText(FText::FromString(Board));
		BoardText->SetHorizontalAlignment(EHTA_Center);
		BoardText->SetVerticalAlignment(EVRTA_TextCenter);
		BoardText->SetWorldSize(26.f);
		BoardText->SetTextRenderColor(FColor(0xb7, 0x1c, 0x1c));
		BoardText->SetRelativeLocation(FVector(L * 0.5f - 5.f, 0, 205.f));
		BoardText->RegisterComponent();
		const float R = 40.f;
		for (float X : { L * 0.5f - 95.f, -L * 0.5f + 100.f })
		{
			Wheel(X, -W * 0.5f + 24.f, R, 26.f);
			Wheel(X, W * 0.5f - 24.f, R, 26.f);
		}
		Seated(FVector(L * 0.5f - 130.f, -W * 0.25f, 75.f), 1.f);
	}
	else if (VehicleType == TEXT("keke"))
	{
		Height = 200.f;
		Piece(ENHShape::Box, FVector(L * 0.25f, 0, 80.f), FVector(L * 0.42f, W * 0.55f, 100.f), PaintS);       // nose
		Piece(ENHShape::Box, FVector(-L * 0.12f, 0, 48.f), FVector(L * 0.62f, W, 22.f), PaintS);              // floor
		Piece(ENHShape::Box, FVector(-L * 0.38f, 0, 95.f), FVector(18.f, W - 10.f, 70.f), Black);              // back bench
		Piece(ENHShape::Box, FVector(L * 0.42f, 0, 150.f), FVector(6.f, W * 0.5f, 60.f), Glass);               // screen
		Piece(ENHShape::Box, FVector(-L * 0.05f, 0, 195.f), FVector(L * 0.9f, W + 6.f, 8.f), FNHSurface(FLinearColor(0.02f, 0.1f, 0.03f), 0.7f, 1.f)); // canopy
		for (float X : { -L * 0.42f, L * 0.36f })
		{
			for (float Y : { -W * 0.47f, W * 0.47f })
			{
				Piece(ENHShape::Cylinder, FVector(X, Y, 130.f), FVector(4.f, 4.f, 130.f), Black);
			}
		}
		Wheel(L * 0.5f - 30.f, 0, 26.f, 14.f);
		Wheel(-L * 0.5f + 42.f, -W * 0.5f + 14.f, 26.f, 16.f);
		Wheel(-L * 0.5f + 42.f, W * 0.5f - 14.f, 26.f, 16.f);
		Seated(FVector(L * 0.1f, 0, 60.f), 0.95f);
	}
	else if (Spec.bBike)
	{
		Height = 130.f;
		Piece(ENHShape::Box, FVector(0, 0, 58.f), FVector(L * 0.55f, 18.f, 30.f), PaintS);                   // frame
		Piece(ENHShape::Box, FVector(L * 0.12f, 0, 82.f), FVector(48.f, 28.f, 24.f), PaintS);                // tank
		Piece(ENHShape::Box, FVector(-L * 0.16f, 0, 84.f), FVector(70.f, 26.f, 10.f), Black);                // seat
		Piece(ENHShape::Box, FVector(L * 0.36f, 0, 104.f), FVector(6.f, 70.f, 5.f), Chrome);                 // handlebar
		Piece(ENHShape::Box, FVector(L * 0.5f - 18.f, 0, 80.f), FVector(10.f, 16.f, 14.f), HeadLamp);
		Wheel(L * 0.5f - 34.f, 0, 32.f, 10.f);
		Wheel(-L * 0.5f + 34.f, 0, 32.f, 12.f);
		Seated(FVector(-L * 0.12f, 0, 68.f), 1.f);
	}
	else // cars, SUVs, trucks
	{
		// the browser game's types keep their shapes; the Unreal-only luxury types pick theirs with "body"
		const bool bLuxSuv = Spec.Body == TEXT("suv"), bLow = Spec.Body == TEXT("sports");
		const bool bTall = VehicleType == TEXT("truck") || VehicleType == TEXT("suv") || bLuxSuv;
		Height = bTall ? 230.f : bLow ? 122.f : 150.f;
		if (bLuxSuv) // one long glasshouse rather than a cab, and a chrome grille
		{
			Piece(ENHShape::Box, FVector(0, 0, 90.f), FVector(L - 10.f, W - 8.f, 100.f), PaintS);
			Piece(ENHShape::Box, FVector(-L * 0.06f, 0, 178.f), FVector(L * 0.66f, W - 22.f, 76.f), Glass);
			Piece(ENHShape::Box, FVector(-L * 0.06f, 0, 219.f), FVector(L * 0.64f, W - 26.f, 6.f), PaintS);
			Piece(ENHShape::Box, FVector(L * 0.5f - 3.f, 0, 96.f), FVector(4.f, W * 0.5f, 44.f), Chrome);
		}
		else if (bLow) // a sports car: long nose, a small cabin set back, a wing
		{
			Piece(ENHShape::Box, FVector(0, 0, 50.f), FVector(L - 10.f, W - 8.f, 44.f), PaintS);
			Piece(ENHShape::Box, FVector(-L * 0.1f, 0, 92.f), FVector(L * 0.36f, W - 34.f, 40.f), Glass);
			Piece(ENHShape::Box, FVector(-L * 0.1f, 0, 114.f), FVector(L * 0.3f, W - 40.f, 5.f), PaintS);
			Piece(ENHShape::Box, FVector(-L * 0.5f + 16.f, 0, 96.f), FVector(22.f, W - 16.f, 4.f), Black);
			for (float Y : { -W * 0.36f, W * 0.36f })
			{
				Piece(ENHShape::Box, FVector(-L * 0.5f + 16.f, Y, 84.f), FVector(6.f, 6.f, 22.f), Black);
			}
		}
		else
		{
			Piece(ENHShape::Box, FVector(0, 0, bTall ? 90.f : 62.f), FVector(L - 10.f, W - 8.f, bTall ? 100.f : 62.f), PaintS);
			Piece(ENHShape::Box, FVector(bTall ? L * 0.28f : -L * 0.05f, 0, bTall ? 185.f : 118.f), FVector(bTall ? L * 0.36f : L * 0.52f, W - 22.f, bTall ? 90.f : 52.f), Glass);
			Piece(ENHShape::Box, FVector(bTall ? L * 0.28f : -L * 0.05f, 0, bTall ? 232.f : 146.f), FVector(bTall ? L * 0.34f : L * 0.48f, W - 26.f, 6.f), PaintS);
		}
		if (VehicleType == TEXT("truck")) // an overloaded tipper: sand piled over the sides
		{
			Piece(ENHShape::Box, FVector(-L * 0.18f, 0, 175.f), FVector(L * 0.6f, W - 6.f, 70.f), FNHSurface(FLinearColor(0.25f, 0.13f, 0.06f), 0.9f, 0.6f));
			Piece(ENHShape::Cone, FVector(-L * 0.18f, 0, 245.f), FVector(L * 0.55f, W - 20.f, 70.f), FNHSurface(FLinearColor(0.45f, 0.3f, 0.15f), 0.95f, 0.8f));
		}
		for (float Y : { -W * 0.32f, W * 0.32f })
		{
			Piece(ENHShape::Box, FVector(L * 0.5f - 2.f, Y, bTall ? 90.f : bLow ? 56.f : 66.f), FVector(4.f, 30.f, bLow ? 8.f : 14.f), HeadLamp);
			Piece(ENHShape::Box, FVector(-L * 0.5f + 2.f, Y, bTall ? 90.f : bLow ? 58.f : 70.f), FVector(4.f, bLow ? 40.f : 24.f, bLow ? 8.f : 14.f), TailLamp);
		}
		const float R = bTall ? 42.f : 33.f;
		for (float X : { L * 0.5f - R * 2.2f, -L * 0.5f + R * 2.4f })
		{
			Wheel(X, -W * 0.5f + 18.f, R, 22.f);
			Wheel(X, W * 0.5f - 18.f, R, 22.f);
		}
		Seated(bLuxSuv ? FVector(L * 0.1f, -W * 0.25f, 110.f) : bLow ? FVector(-L * 0.06f, -W * 0.22f, 40.f) : FVector(bTall ? L * 0.3f : L * 0.05f, -W * 0.25f, bTall ? 120.f : 55.f), bLow ? 0.85f : 1.f);
	}

	// collision: the box from the ground clearance up, so kerbs pass underneath and the vehicle rides up them
	Clearance = Spec.bBike ? 22.f : 28.f;
	HalfHeight = (Height - Clearance) * 0.5f;
	BodyHeight = Height;
	Box->SetBoxExtent(FVector(L * 0.5f, FMath::Max(W * 0.5f, 25.f), HalfHeight));
	Body->SetRelativeLocation(FVector(0, 0, -(Clearance + HalfHeight)));
	Arm->TargetArmLength = L * 1.2f + 420.f;
	SetOccupied(false);
}

bool ANHVehicle::AddModel(float& OutHeight)
{
	const UNHGameData* Data = UNHGameData::Get(this);
	const FNHVehicleMesh* Model = Data ? Data->VehicleMeshes.Find(VehicleType) : nullptr;
	if (!Model)
	{
		return false;
	}
	int32 Added = 0;
	for (const FString& Path : Model->Meshes)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
		if (!Mesh)
		{
			UE_LOG(LogNHGame, Warning, TEXT("NHVehicle: %s's model %s is missing; run Scripts/assign_vehicle_meshes.py again"), *VehicleType.ToString(), *Path);
			continue;
		}
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(Mesh);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision); // the vehicle's box does the colliding
		C->SetCanEverAffectNavigation(false);
		C->SetupAttachment(Body);
		C->SetRelativeTransform(FTransform(FRotator(0.f, Model->Yaw, 0.f), Model->Offset, FVector(Model->Scale)));
		C->RegisterComponent();
		++Added;
	}
	if (Added == 0)
	{
		return false; // nothing loaded: keep the blockout
	}
	OutHeight = FMath::Clamp(Model->Height, 60.f, 400.f);
	return true;
}

int32 ANHVehicle::ValueOf(FName Type)
{
	static const TMap<FName, int32> Worth = { { TEXT("okada"), 350000 }, { TEXT("keke"), 900000 }, { TEXT("danfo"), 2500000 }, { TEXT("sedan"), 4500000 }, { TEXT("suv"), 9000000 },
		{ TEXT("truck"), 12000000 }, { TEXT("tfpick"), 15000000 }, { TEXT("tfbike"), 1200000 }, { TEXT("luxsedan"), 45000000 }, { TEXT("luxsuv"), 70000000 }, { TEXT("coupesuv"), 85000000 },
		{ TEXT("royalsuv"), 250000000 }, { TEXT("supersuv"), 180000000 }, { TEXT("sports"), 120000000 }, { TEXT("luxcoupe"), 160000000 }, { TEXT("hypercar"), 900000000 } };
	const int32* V = Worth.Find(Type);
	return V ? *V : 4000000;
}

void ANHVehicle::SetOccupied(bool bOn)
{
	bNpcDriver = false; // whoever was at the wheel is not any more
	// the player's own body if there is one to show, the blockout driver otherwise
	const bool bBody = bOn && SeatDriver();
	if (DriverBody)
	{
		DriverBody->SetVisibility(bBody && !bCabinView);
	}
	if (!bOn && bCabinView)
	{
		SetCabinView(false); // the next driver starts from the chase camera
	}
	for (UStaticMeshComponent* P : DriverPieces)
	{
		if (P)
		{
			P->SetVisibility(bOn && !bBody);
		}
	}
}

bool ANHVehicle::SeatDriver()
{
	const ANHCharacter* Player = Cast<ANHCharacter>(UGameplayStatics::GetActorOfClass(this, ANHCharacter::StaticClass()));
	USkeletalMesh* Mesh = Player && Player->HasBody() ? Player->GetMesh()->GetSkeletalMeshAsset() : nullptr;
	if (!Mesh)
	{
		return false;
	}
	// the body's own scale, and the way it faces in its own space
	return SeatBody(Mesh, Player->GetMesh()->GetRelativeScale3D().X, -Player->GetMesh()->GetRelativeRotation().Yaw);
}

void ANHVehicle::SetNpcDriver(USkeletalMesh* Mesh)
{
	if (Mesh && SeatBody(Mesh, 1.f, ANHCharacter::FacingYawOf(Mesh)))
	{
		DriverBody->SetVisibility(true);
		bNpcDriver = true;
	}
}

void ANHVehicle::SetupDriverAnim(USkeletalMesh* Mesh)
{
	DriveClips.Reset();
	for (const TCHAR* Name : { TEXT("Drive_Idle"), TEXT("Drive_Left"), TEXT("Drive_Right"), TEXT("Drive_Reverse") })
	{
		DriveClips.Add(ANHCharacter::ActionClip(Mesh, Name));
	}
	const bool bClips = DriveClips[0] != nullptr && !Spec.bBike; // the clips sit him at a car's wheel, not astride a bike
	DriverBody->SetHiddenInGame(bClips);
	if (!bClips)
	{
		DriveClips.Reset();
		if (DriverAnim)
		{
			DriverAnim->SetVisibility(false);
		}
		return;
	}
	if (!DriverAnim)
	{
		DriverAnim = NewObject<USkeletalMeshComponent>(this);
		DriverAnim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DriverAnim->SetCanEverAffectNavigation(false);
		DriverAnim->SetupAttachment(Body);
		DriverAnim->RegisterComponent();
	}
	DriverAnim->SetSkeletalMeshAsset(Mesh);
	DriverAnim->SetRelativeTransform(DriverBody->GetRelativeTransform());
	DriverAnim->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	DriverAnim->PlayAnimation(DriveClips[0], true);
	DriveClipShown = 0;
	SteerShown = ReverseShown = 0.f;
}

void ANHVehicle::DriverAnimTick(float DeltaSeconds)
{
	if (!DriverAnim || DriveClips.Num() < 4 || !DriverBody)
	{
		return;
	}
	const bool bShow = DriverBody->GetVisibleFlag();
	DriverAnim->SetVisibility(bShow);
	if (!bShow)
	{
		return;
	}
	SteerShown = FMath::FInterpTo(SteerShown, Controller ? Steer : 0.f, DeltaSeconds, 7.f);
	ReverseShown = FMath::FInterpTo(ReverseShown, Controller && Speed < -40.f ? 1.f : 0.f, DeltaSeconds, 4.f);
	// reversing: turned to look behind. Otherwise the hands go round with the wheel, as far through the turning clip as the wheel is turned.
	const int32 Want = ReverseShown > 0.04f && DriveClips[3] ? 3 : SteerShown < -0.05f && DriveClips[1] ? 1 : SteerShown > 0.05f && DriveClips[2] ? 2 : 0;
	if (Want != DriveClipShown)
	{
		DriveClipShown = Want;
		DriverAnim->SetAnimation(DriveClips[Want]);
		if (Want == 0)
		{
			DriverAnim->Play(true);
		}
		else
		{
			DriverAnim->Stop();
		}
	}
	if (Want != 0)
	{
		DriverAnim->SetPosition((Want == 3 ? ReverseShown : FMath::Abs(SteerShown)) * DriveClips[Want]->GetPlayLength() * 0.98f, false);
	}
}

bool ANHVehicle::SeatBody(USkeletalMesh* Mesh, float Scale, float Facing)
{
	if (!DriverBody)
	{
		DriverBody = NewObject<UPoseableMeshComponent>(this);
		DriverBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DriverBody->SetCanEverAffectNavigation(false);
		DriverBody->SetupAttachment(Body);
		DriverBody->RegisterComponent();
	}
	if (DriverBody->GetSkinnedAsset() == Mesh)
	{
		return true; // already seated and posed
	}
	DriverBody->SetSkinnedAssetAndUpdate(Mesh);
	DriverBody->ResetBoneTransformByName(NAME_None);

	// Posed by pointing each limb where a driver's would be, in the body's own space, so it works for any skeleton
	// that uses the mannequin's bone names, whichever pose it was made in.
	const FVector Fwd = FRotator(0.f, Facing, 0.f).Vector(), Up = FVector::UpVector, Right = FVector::CrossProduct(Up, Fwd);
	const auto At = [this](const TCHAR* Bone) { return DriverBody->GetBoneLocationByName(FName(Bone), EBoneSpaces::ComponentSpace); };
	const auto Aim = [this, &At](const FString& Bone, const FString& Child, const FVector& Toward)
	{
		if (DriverBody->GetBoneIndex(FName(*Bone)) == INDEX_NONE || DriverBody->GetBoneIndex(FName(*Child)) == INDEX_NONE)
		{
			return;
		}
		const FVector Now = (At(*Child) - At(*Bone)).GetSafeNormal();
		const FQuat Turn = FQuat::FindBetweenNormals(Now, Toward.GetSafeNormal());
		const FQuat Was = DriverBody->GetBoneRotationByName(FName(*Bone), EBoneSpaces::ComponentSpace).Quaternion();
		DriverBody->SetBoneRotationByName(FName(*Bone), (Turn * Was).Rotator(), EBoneSpaces::ComponentSpace);
	};
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FString S = Side == 0 ? TEXT("_l") : TEXT("_r");
		const FVector Out = Side == 0 ? -Right : Right;
		if (Spec.bBike)
		{
			Aim(TEXT("thigh") + S, TEXT("calf") + S, Fwd * 0.75f - Up * 0.45f + Out * 0.45f);
			Aim(TEXT("calf") + S, TEXT("foot") + S, -Up * 0.9f - Fwd * 0.3f);
		}
		else if (VehicleType == TEXT("danfo") || VehicleType == TEXT("truck"))
		{
			// sitting upright over the front axle: knees lower, shins straight down
			Aim(TEXT("thigh") + S, TEXT("calf") + S, Fwd * 0.8f - Up * 0.5f + Out * 0.15f);
			Aim(TEXT("calf") + S, TEXT("foot") + S, -Up);
		}
		else
		{
			Aim(TEXT("thigh") + S, TEXT("calf") + S, Fwd * 0.95f - Up * 0.12f + Out * 0.18f);
			Aim(TEXT("calf") + S, TEXT("foot") + S, -Up * 0.8f + Fwd * 0.55f);
		}
		Aim(TEXT("upperarm") + S, TEXT("lowerarm") + S, Fwd * 0.75f - Up * 0.6f + Out * 0.12f);
		Aim(TEXT("lowerarm") + S, TEXT("hand") + S, Fwd * 0.9f + Up * 0.25f - Out * 0.15f);
	}
	const float Hips = DriverBody->GetBoneIndex(TEXT("pelvis")) != INDEX_NONE ? At(TEXT("pelvis")).Z : 95.f;
	DriverBody->SetRelativeScale3D(FVector(Scale));
	DriverBody->SetRelativeRotation(FRotator(0.f, -Facing, 0.f));
	DriverBody->SetRelativeLocation(SeatAt - FVector(0.f, 0.f, Hips * Scale));
	SetupDriverAnim(Mesh);
	UE_LOG(LogNHGame, Verbose, TEXT("NAIJA HUSTLE: driver seated in the %s at %s (hips %.0f cm up the body, scale %.2f, vehicle %.0f x %.0f x %.0f)"), *VehicleType.ToString(), *SeatAt.ToCompactString(), Hips, Scale, Spec.Length, Spec.Width, BodyHeight);
	return true;
}

void ANHVehicle::SetCabinView(bool bOn)
{
	bCabinView = bOn;
	if (bOn)
	{
		// at the driver's eyes: the seat is where the hips are, the eyes about 65 cm above, a little forward
		Arm->TargetArmLength = 0.f;
		Arm->SocketOffset = FVector::ZeroVector;
		const bool bCabOver = VehicleType == TEXT("danfo") || VehicleType == TEXT("truck"); // sits close to the windscreen, high up
		Arm->SetRelativeLocation(Body->GetRelativeLocation() + SeatAt + (bCabOver ? FVector(-20.f, 0.f, 48.f) : FVector(Spec.bBike ? 20.f : 10.f, 0.f, 65.f)));
		Arm->bDoCollisionTest = false;
		Arm->bEnableCameraLag = false;
		Arm->bEnableCameraRotationLag = false;
		Camera->SetFieldOfView(88.f);
	}
	else
	{
		Arm->TargetArmLength = Spec.Length * 1.2f + 420.f;
		Arm->SocketOffset = FVector(0.f, 0.f, 160.f);
		Arm->SetRelativeLocation(FVector::ZeroVector);
		Arm->bDoCollisionTest = true;
		Arm->bEnableCameraLag = true;
		Arm->bEnableCameraRotationLag = true;
		Camera->SetFieldOfView(75.f);
	}
	LookOffset = FVector2D::ZeroVector;
	if (DriverBody)
	{
		DriverBody->SetVisibility(!bOn && IsPlayerControlled()); // his own head would fill the view
	}
}

void ANHVehicle::SetHeadlights(bool bOn)
{
	bHeadlights = bOn;
	if (bOn && Lamps.Num() == 0)
	{
		const float L = Spec.Length, W = Spec.Width;
		for (int32 Side = 0; Side < (Spec.bBike ? 1 : 2); ++Side)
		{
			USpotLightComponent* Beam = NewObject<USpotLightComponent>(this);
			Beam->SetupAttachment(Body);
			Beam->SetRelativeLocationAndRotation(FVector(L * 0.5f - 8.f, Spec.bBike ? 0.f : (Side ? 0.3f : -0.3f) * W, BodyHeight * (Spec.bBike ? 0.6f : 0.42f)), FRotator(-6.f, 0.f, 0.f));
			Beam->SetIntensityUnits(ELightUnits::Candelas);
			Beam->SetIntensity(9000.f);
			Beam->SetLightColor(FLinearColor(1.f, 0.93f, 0.8f));
			Beam->SetInnerConeAngle(16.f);
			Beam->SetOuterConeAngle(34.f);
			Beam->SetAttenuationRadius(4500.f);
			Beam->SetCastShadows(false); // two shadowed lights per car is too much for the 8 GB Mac
			Beam->RegisterComponent();
			Lamps.Add(Beam);
			// the lamp itself, lit: a real model's lamp is only painted on
			LampGlow.Add(NHShapes::AddPiece(this, Body, ENHShape::Sphere, Beam->GetRelativeLocation() + FVector(2.f, 0.f, 0.f), FVector(10.f, Spec.bBike ? 16.f : 26.f, 13.f), NHCar::HeadLamp));
		}
		for (int32 Side = 0; Side < (Spec.bBike ? 1 : 2); ++Side)
		{
			LampGlow.Add(NHShapes::AddPiece(this, Body, ENHShape::Sphere, FVector(-L * 0.5f - 1.f, Spec.bBike ? 0.f : (Side ? 0.32f : -0.32f) * W, BodyHeight * (Spec.bBike ? 0.55f : 0.45f)), FVector(8.f, Spec.bBike ? 12.f : 24.f, 11.f), NHCar::TailLamp));
		}
		UPointLightComponent* Tail = NewObject<UPointLightComponent>(this);
		Tail->SetupAttachment(Body);
		Tail->SetRelativeLocation(FVector(-L * 0.5f - 12.f, 0.f, BodyHeight * 0.45f));
		Tail->SetIntensityUnits(ELightUnits::Candelas);
		Tail->SetIntensity(18.f);
		Tail->SetLightColor(FLinearColor(1.f, 0.05f, 0.03f));
		Tail->SetAttenuationRadius(350.f);
		Tail->SetCastShadows(false);
		Tail->RegisterComponent();
		Lamps.Add(Tail);
	}
	for (ULocalLightComponent* Lamp : Lamps)
	{
		if (Lamp)
		{
			Lamp->SetVisibility(bOn);
		}
	}
	for (UStaticMeshComponent* Glow : LampGlow)
	{
		if (Glow)
		{
			Glow->SetVisibility(bOn);
		}
	}
	if (IsPlayerControlled())
	{
		ANHHUD::Toast(this, bOn ? TEXT("Headlights on") : TEXT("Headlights off"), 0);
	}
}

void ANHVehicle::Repair()
{
	Health = MaxHealth;
	Speed = 0.f;
	PaintFx->ClearDamage();
	Dynamics->ResetDynamics();
}

FVector ANHVehicle::ExitPoint() const
{
	const FVector C = GetActorLocation();
	const FVector Right = GetActorRightVector(), Fwd = GetActorForwardVector();
	const float Side = Spec.Width * 0.5f + 80.f;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(NHVehicleExit), false, this);
	for (const FVector& Dir : { -Right, Right, -Fwd, Fwd })
	{
		const float Reach = (Dir | Fwd) != 0.f && FMath::Abs(Dir | Fwd) > 0.5f ? Spec.Length * 0.5f + 80.f : Side;
		const FVector P = C + Dir * Reach;
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, C, P, FCollisionObjectQueryParams(ECC_WorldStatic), Q))
		{
			return FVector(P.X, P.Y, GroundZ(P) + 100.f);
		}
	}
	return C + FVector(0, 0, HalfHeight + 120.f); // boxed in: out through the roof
}

void ANHVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Drive(DeltaSeconds);
	DriverAnimTick(DeltaSeconds);
	if (AlarmLeft > 0.f)
	{
		// the alarm: lights flashing (when nobody is driving) and the noise written in the air; there is no sound yet
		AlarmLeft -= DeltaSeconds;
		AlarmBeat -= DeltaSeconds;
		if (AlarmBeat <= 0.f || AlarmLeft <= 0.f)
		{
			AlarmBeat = 0.4f;
			if (!IsPlayerControlled())
			{
				SetHeadlights(AlarmLeft > 0.f && !bHeadlights);
			}
			if (AlarmLeft > 0.f && FMath::Fmod(AlarmLeft, 2.f) < 0.45f)
			{
				ANHHUD::Floater(this, GetActorLocation() + FVector(0, 0, 220.f), TEXT("WEEOO! WEEOO!"));
			}
		}
	}

	// wheels turn with the speed, bikes lean into corners
	const float R = Spec.bBike ? 32.f : 38.f;
	WheelSpin = FMath::Fmod(WheelSpin - FMath::RadiansToDegrees(Speed * DeltaSeconds / R), 360.f);
	for (USceneComponent* Hub : Wheels)
	{
		if (Hub)
		{
			Hub->SetRelativeRotation(FRotator(WheelSpin, 0.f, 0.f));
		}
	}
	if (Spec.bBike)
	{
		const float Want = -Steer * FMath::Clamp(FMath::Abs(Speed) / Spec.MaxSpeed, 0.f, 1.f) * 25.f;
		Lean = FMath::FInterpTo(Lean, Want, DeltaSeconds, 5.f);
	}
	// the sprung body: dive, squat, roll, and the suspension over kerbs (a bike's lean is added on top)
	Dynamics->StepChassis(DeltaSeconds, Spec.bBike ? Lean : 0.f);

	// free look while driving, easing back behind the vehicle after two seconds
	LookIdle += DeltaSeconds;
	if (LookIdle > 2.f)
	{
		LookOffset = FMath::Vector2DInterpTo(LookOffset, FVector2D::ZeroVector, DeltaSeconds, 2.f);
	}
	Arm->SetRelativeRotation(FRotator((bCabinView ? 0.f : -12.f) + LookOffset.Y, LookOffset.X, 0.f));
}

void ANHVehicle::Readings(int32& OutGear, float& OutRpm) const
{
	// Five gears (four on a bike), each taking a wider band of the speed than the one below; the revs climb through a
	// gear and drop back at the change, as a rev counter's needle does.
	static const float Tops[] = { 0.14f, 0.3f, 0.5f, 0.74f, 1.02f };
	const int32 Gears = Spec.bBike ? 4 : 5;
	const float K = FMath::Abs(Speed) / FMath::Max(Spec.MaxSpeed, 1.f) * (Spec.bBike ? Tops[4] / Tops[3] : 1.f);
	if (Speed < -20.f)
	{
		OutGear = -1;
		OutRpm = 900.f + FMath::Clamp(K / 0.3f, 0.f, 1.f) * 4200.f;
		return;
	}
	if (FMath::Abs(Speed) < 20.f && Throttle <= 0.f)
	{
		OutGear = 0;
		OutRpm = Fuel > 0.f && Controller ? 850.f : 0.f;
		return;
	}
	int32 G = 0;
	while (G < Gears - 1 && K > Tops[G])
	{
		++G;
	}
	const float Low = G == 0 ? 0.f : Tops[G - 1];
	OutGear = G + 1;
	OutRpm = (G == 0 ? 1000.f : 2600.f) + FMath::Clamp((K - Low) / (Tops[G] - Low), 0.f, 1.f) * (G == 0 ? 5400.f : 3800.f);
}

FString ANHVehicle::DescribeMotion() const
{
	static const TCHAR* Clips[] = { TEXT("Drive_Idle"), TEXT("Drive_Left"), TEXT("Drive_Right"), TEXT("Drive_Reverse") };
	return FString::Printf(TEXT("%s: %d wheels, turned %.0f deg, lean %.1f deg, steering %.2f, speed %.0f km/h, rider %s, rider's clip %s, fuel %.0f%%"), *VehicleType.ToString(), Wheels.Num(), WheelSpin, Lean, SteerEased,
		Speed * 0.036f, DriverAnim && DriverAnim->IsVisible() ? TEXT("animated") : DriverBody && DriverBody->IsVisible() ? TEXT("posed, no clips") : TEXT("not shown"),
		DriveClipShown >= 0 && DriveClipShown < 4 ? Clips[DriveClipShown] : TEXT("none"), Fuel * 100.f);
}

void ANHVehicle::TrafficMove(const FVector2D& At, float Yaw, float InSpeed, float DeltaSeconds)
{
	Speed = InSpeed;
	const FVector From = GetActorLocation();
	FVector To(At.X, At.Y, From.Z);
	const float Axle = Spec.Length * 0.31f;
	const FVector Fwd = FRotator(0.f, Yaw, 0.f).Vector();
	const float FrontZ = GroundZ(To + Fwd * Axle), RearZ = GroundZ(To - Fwd * Axle);
	const float Pitch = FMath::FInterpTo(GetActorRotation().Pitch, FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(FrontZ - RearZ, 2.f * Axle)), -30.f, 30.f), DeltaSeconds, 12.f);
	const float WantZ = (FrontZ + RearZ) * 0.5f + Clearance + HalfHeight;
	const float Slope = FVector::Dist2D(From, To) * 0.3f + 1.f; // down a slope with the road; only a sharp drop is eased
	To.Z = WantZ > From.Z - Slope ? WantZ : FMath::Max(WantZ, FMath::Min(From.Z - Slope, FMath::FInterpTo(From.Z, WantZ, DeltaSeconds, 10.f)));
	SetActorLocationAndRotation(To, FRotator(Pitch, Yaw, 0.f), false);
}

void ANHVehicle::Drive(float DeltaSeconds)
{
	if (bTraffic && !Controller)
	{
		return; // carried along the road by ANHTraffic
	}
	const float A = Spec.Accel, V = Spec.MaxSpeed;
	const bool bDriven = Controller != nullptr && !IsWrecked() && !bHeld;
	const float T = bDriven && Fuel > 0.f ? Throttle : 0.f, B = bDriven ? BrakeIn : 0.f, S = bDriven ? Steer : 0.f;
	if (bDriven && Throttle > 0.f && Fuel <= 0.f && IsPlayerControlled() && GetWorld()->GetTimeSeconds() - DrySaid > 6.f)
	{
		DrySaid = GetWorld()->GetTimeSeconds();
		ANHHUD::Toast(this, TEXT("Fuel don finish. Find petrol, or a jerry can."), 2);
	}

	// The engine pulls hardest from rest and less and less as the speed climbs, so a car works up through its speed
	// to the top over a quarter of a minute. (Used whole, the data's figure put every car at top speed inside two seconds.)
	const float Pull = A * 0.2f * FMath::Lerp(1.f, 0.12f, FMath::Clamp(Speed / V, 0.f, 1.f));
	if (T > 0.f)
	{
		Speed += (Speed < 0.f ? A * 0.3f : Pull) * T * DeltaSeconds;
	}
	if (B > 0.f)
	{
		Speed -= A * B * (Speed > 50.f ? 0.3f : 0.1f) * DeltaSeconds; // brakes at about a g, then reverses slowly
	}
	if (T <= 0.f && B <= 0.f)
	{
		Speed -= FMath::Sign(Speed) * FMath::Min(FMath::Abs(Speed), (70.f + FMath::Abs(Speed) * 0.06f) * DeltaSeconds); // rolls on, slowing
	}
	if (bHandbrake && bDriven)
	{
		Speed *= FMath::Max(0.f, 1.f - 3.f * DeltaSeconds);
	}
	Speed = FMath::Clamp(Speed, -V * 0.3f, V);
	if (FMath::Abs(Speed) < 1.f && T <= 0.f && B <= 0.f && FMath::Abs(Dynamics->GetSideSpeed()) < 1.f)
	{
		Speed = 0.f;
		return;
	}

	// The wheel takes a moment to go over and comes back quicker; and the faster the car, the less it turns, held to
	// what the tyres can do (about 1.2 g sideways), so a touch of the key at speed is a lane change, not a swerve.
	SteerEased = FMath::FInterpConstantTo(SteerEased, S, DeltaSeconds, FMath::Abs(S) > FMath::Abs(SteerEased) ? 2.4f : 4.5f);
	const float MostTurn = FMath::Min(Spec.Turn * 0.55f, 1200.f / FMath::Max(FMath::Abs(Speed), 1.f));
	const float TurnRate = FMath::RadiansToDegrees(MostTurn) * SteerEased * FMath::Clamp(FMath::Abs(Speed) / (V * 0.1f), 0.f, 1.f) * (Speed >= 0.f ? 1.f : -1.f);
	FRotator Rot(0.f, GetActorRotation().Yaw + TurnRate * DeltaSeconds, 0.f);
	// grip: past the limit (speed, handbrake, dirt, rain) the car slides sideways as well as going where it points
	const float Slid = Dynamics->StepTraction(DeltaSeconds, Speed, FMath::DegreesToRadians(TurnRate), bHandbrake && bDriven);
	const FVector From = GetActorLocation(), Delta = Rot.Vector() * Speed * DeltaSeconds + FRotationMatrix(Rot).GetUnitAxis(EAxis::Y) * Slid;
	FVector To = From + Delta;
	// The body follows the ground under its axles: nose up on a ramp, down on the far side. Kept level, the front
	// of the collision box dug into any slope steeper than about one in ten and the vehicle battered itself to a stop.
	const float Axle = Spec.Length * 0.31f;
	const FVector Fwd = Rot.Vector();
	const float FrontZ = GroundZ(To + Fwd * Axle), RearZ = GroundZ(To - Fwd * Axle);
	Rot.Pitch = FMath::FInterpTo(GetActorRotation().Pitch, FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(FrontZ - RearZ, 2.f * Axle)), -30.f, 30.f), DeltaSeconds, 12.f);
	const float WantZ = (FrontZ + RearZ) * 0.5f + Clearance + HalfHeight;
	if (WantZ > From.Z - 30.f)
	{
		// Up at once; down a slope with the road, the wheels on it (eased, the body hung a foot in the air all the way
		// down a ramp); only a sharp drop, off a kerb, is let down gently.
		const float Slope = Delta.Size2D() * 0.3f + 1.f;
		To.Z = WantZ > From.Z - Slope ? WantZ : FMath::Max(WantZ, FMath::Min(From.Z - Slope, FMath::FInterpTo(From.Z, WantZ, DeltaSeconds, 10.f)));
		FallSpeed = 0.f;
	}
	else
	{
		// the ground has gone from under it (over a crest at speed, off an edge): it falls as things fall, and lands hard
		FallSpeed = FMath::Min(FallSpeed + 980.f * DeltaSeconds, 6000.f);
		To.Z = FMath::Max(WantZ, From.Z - FallSpeed * DeltaSeconds);
		if (To.Z <= WantZ + 1.f)
		{
			if (FallSpeed > 900.f)
			{
				Health = FMath::Max(0.f, Health - (FallSpeed - 900.f) / 30.f);
				Speed *= 0.6f;
				ANHHUD::Floater(this, To + FVector(0.f, 0.f, 150.f), TEXT("CRASH!"));
			}
			FallSpeed = 0.f;
		}
	}

	// ten litres a hundred kilometres for a car, three for a bike, out of the tank
	Odometer += Delta.Size2D();
	Fuel = FMath::Max(0.f, Fuel - Delta.Size2D() / 1.e7f * (Spec.bBike ? 3.f : 10.f) / TankLitres());
	FHitResult Hit;
	SetActorLocationAndRotation(To, Rot, true, &Hit);
	if (Hit.bBlockingHit)
	{
		const FVector N = Hit.ImpactNormal.GetSafeNormal2D();
		const float Impact = FMath::Abs(Speed * (Rot.Vector() | N));
		if (Impact > 350.f) // the browser game's crash damage, scaled to cm/s
		{
			Health = FMath::Max(0.f, Health - (Impact - 350.f) / 45.f);
			ANHHUD::Floater(this, Hit.ImpactPoint + FVector(0, 0, 150.f), TEXT("CRASH!"));
			PaintFx->ApplyImpact(Hit.ImpactPoint, FMath::GetMappedRangeValueClamped(FVector2D(350.f, 1800.f), FVector2D(0.25f, 1.f), Impact));
		}
		Speed *= Impact > 600.f ? -0.25f : 0.6f;
		Dynamics->DampSlide(0.3f);
		const FVector Slide = FVector::VectorPlaneProject(Delta, N) * (1.f - Hit.Time);
		AddActorWorldOffset(FVector(Slide.X, Slide.Y, 0.f), true);
	}
}

void ANHVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetController());
	if (!Input || !PC)
	{
		return;
	}
	const UNHInputSet* Set = PC->GetInputSet();
	Input->BindAction(Set->Throttle, ETriggerEvent::Triggered, this, &ANHVehicle::OnThrottle);
	Input->BindAction(Set->Throttle, ETriggerEvent::Completed, this, &ANHVehicle::OnThrottleEnd);
	Input->BindAction(Set->Brake, ETriggerEvent::Triggered, this, &ANHVehicle::OnBrake);
	Input->BindAction(Set->Brake, ETriggerEvent::Completed, this, &ANHVehicle::OnBrakeEnd);
	Input->BindAction(Set->Steer, ETriggerEvent::Triggered, this, &ANHVehicle::OnSteer);
	Input->BindAction(Set->Steer, ETriggerEvent::Completed, this, &ANHVehicle::OnSteerEnd);
	Input->BindAction(Set->Handbrake, ETriggerEvent::Started, this, &ANHVehicle::OnHandbrake);
	Input->BindAction(Set->Handbrake, ETriggerEvent::Completed, this, &ANHVehicle::OnHandbrakeEnd);
	Input->BindAction(Set->Horn, ETriggerEvent::Started, this, &ANHVehicle::OnHorn);
	Input->BindAction(Set->Headlights, ETriggerEvent::Started, this, &ANHVehicle::OnHeadlights);
	Input->BindAction(Set->Radio, ETriggerEvent::Started, this, &ANHVehicle::OnRadio);
	Input->BindAction(Set->RadioTrack, ETriggerEvent::Started, this, &ANHVehicle::OnRadioTrack);
	Input->BindAction(Set->CabinView, ETriggerEvent::Started, this, &ANHVehicle::OnCabinView);
	Input->BindAction(Set->Look, ETriggerEvent::Triggered, this, &ANHVehicle::OnLook);
	Input->BindAction(Set->LookStick, ETriggerEvent::Triggered, this, &ANHVehicle::OnLook);
}

void ANHVehicle::OnRadio()
{
	if (UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this))
	{
		Audio->RadioNextStation(this);
	}
}

void ANHVehicle::OnRadioTrack()
{
	if (UNHAudioSubsystem* Audio = UNHAudioSubsystem::Get(this))
	{
		Audio->RadioNextTrack();
	}
}

void ANHVehicle::OnHorn()
{
	ANHHUD::Floater(this, GetActorLocation() + FVector(0, 0, 250.f), VehicleType == TEXT("danfo") ? TEXT("PAAAN! PAAAN!") : TEXT("PIM PIM!"));
}

void ANHVehicle::OnLook(const FInputActionValue& V)
{
	const FVector2D D = V.Get<FVector2D>();
	LookOffset.X = FMath::Clamp(LookOffset.X + D.X, -170.f, 170.f);
	LookOffset.Y = FMath::Clamp(LookOffset.Y + D.Y, -30.f, 25.f);
	LookIdle = 0.f;
}

void ANHVehicle::SetStreamingRadius(bool bOn, float Radius)
{
	if (!bOn)
	{
		Streaming->DisableStreamingSource();
		return;
	}
	FStreamingSourceShape Shape;
	Shape.bUseGridLoadingRange = false;
	Shape.Radius = Radius;
	Streaming->Shapes.SetNum(1);
	Streaming->Shapes[0] = Shape;
	Streaming->EnableStreamingSource();
}
