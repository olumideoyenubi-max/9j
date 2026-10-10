#include "Gameplay/NHEstate.h"

#include "Animation/AnimSequence.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Core/NHGameData.h"
#include "Core/NHHustleSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Gameplay/NHInventory.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "NaijaHustleGame.h"
#include "Player/NHBodyAnimInstance.h"
#include "Player/NHCharacter.h"
#include "Player/NHPlayerController.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UI/NHHUD.h"
#include "Vehicles/NHVehicle.h"
#include "World/NHShapes.h"

namespace NHEstateLook
{
	// what a kind of place is called on its board, and the colour of its board and lights
	struct FKind { const TCHAR* Id; const TCHAR* Board; FLinearColor Colour; };
	const FKind Kinds[] = {
		{ TEXT("land"), TEXT("LAND FOR SALE"), FLinearColor(0.75f, 0.55f, 0.05f) }, { TEXT("house"), TEXT("HOUSE FOR SALE"), FLinearColor(0.05f, 0.35f, 0.6f) },
		{ TEXT("apartment"), TEXT("FLAT FOR SALE"), FLinearColor(0.05f, 0.45f, 0.4f) }, { TEXT("bar"), TEXT("BAR"), FLinearColor(1.f, 0.55f, 0.1f) },
		{ TEXT("club"), TEXT("NIGHT CLUB"), FLinearColor(0.2f, 0.35f, 1.f) }, { TEXT("strip"), TEXT("GENTLEMEN'S CLUB"), FLinearColor(1.f, 0.05f, 0.25f) },
		{ TEXT("petrol"), TEXT("PETROL"), FLinearColor(0.1f, 0.6f, 0.15f) } };
	const FKind& Of(const FString& Kind)
	{
		for (const FKind& K : Kinds)
		{
			if (Kind == K.Id)
			{
				return K;
			}
		}
		return Kinds[0];
	}
}

ANHEstate::ANHEstate()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

ANHEstate* ANHEstate::Get(const UObject* WorldContext)
{
	return WorldContext ? Cast<ANHEstate>(UGameplayStatics::GetActorOfClass(WorldContext, ANHEstate::StaticClass())) : nullptr;
}

bool ANHEstate::Load()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("NaijaHustleGame"));
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!Plugin || !FFileHelper::LoadFileToString(Text, *(Plugin->GetBaseDir() / TEXT("Data/estate.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
	{
		return false;
	}
	Pocket = static_cast<int32>(Root->GetNumberField(TEXT("pocket")));
	PetrolPerLitre = static_cast<int32>(Root->GetNumberField(TEXT("petrolPerLitre")));
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (Root->TryGetArrayField(TEXT("places"), List))
	{
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FPlace P;
			P.Id = FName(*O->GetStringField(TEXT("id")));
			P.Kind = O->GetStringField(TEXT("kind"));
			P.Name = O->GetStringField(TEXT("name"));
			P.Area = O->GetStringField(TEXT("area"));
			O->TryGetStringField(TEXT("about"), P.About);
			double Number = 0.0;
			P.Price = O->TryGetNumberField(TEXT("price"), Number) ? static_cast<int64>(Number) : 0;
			P.Fee = O->TryGetNumberField(TEXT("fee"), Number) ? static_cast<int32>(Number) : 0;
			// the model's own flat projection (build_lagos_real.py): origin 3.40 E, 6.47 N, Unreal cm, Y south
			P.Want = FVector2D((O->GetNumberField(TEXT("lon")) - 3.40) * 11061100.0, (6.47 - O->GetNumberField(TEXT("lat"))) * 11057400.0);
			Places.Add(MoveTemp(P));
		}
	}
	if (Root->TryGetArrayField(TEXT("people"), List))
	{
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FPerson Who;
			Who.Id = FName(*O->GetStringField(TEXT("id")));
			Who.Name = O->GetStringField(TEXT("name"));
			Who.About = O->GetStringField(TEXT("about"));
			Who.Skin = FName(*O->GetStringField(TEXT("skin")));
			Who.Home = FName(*O->GetStringField(TEXT("home")));
			Who.Bank = static_cast<int64>(O->GetNumberField(TEXT("bank")));
			const TArray<TSharedPtr<FJsonValue>>* Cars = nullptr;
			if (O->TryGetArrayField(TEXT("cars"), Cars))
			{
				for (const TSharedPtr<FJsonValue>& Car : *Cars)
				{
					Who.Cars.Add(FName(*Car->AsString()));
				}
			}
			People.Add(MoveTemp(Who));
		}
	}
	const TSharedPtr<FJsonObject>* Cards = nullptr;
	if (Root->TryGetObjectField(TEXT("menus"), Cards))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Card : (*Cards)->Values)
		{
			TArray<FDrink>& Out = Menus.Add(Card.Key);
			for (const TSharedPtr<FJsonValue>& Row : Card.Value->AsArray())
			{
				const TArray<TSharedPtr<FJsonValue>>& Cells = Row->AsArray();
				if (Cells.Num() >= 3)
				{
					Out.Add({ Cells[0]->AsString(), static_cast<int32>(Cells[1]->AsNumber()), static_cast<float>(Cells[2]->AsNumber()) });
				}
			}
		}
	}
	return Places.Num() > 0;
}

void ANHEstate::BeginPlay()
{
	Super::BeginPlay();
	const UNHGameData* Data = UNHGameData::Get(this);
	if (!Data || !Data->bRealCity || !Load())
	{
		SetActorTickEnabled(false); // the street level has no Lekki to put anything in
		return;
	}
	FString As;
	const UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	StartAs = FParse::Value(FCommandLine::Get(), TEXT("NHPlayAs="), As) ? FName(*As) : Hustle ? Hustle->Persona : NAME_None;
	StartHome = StartAs.IsNone() && Hustle ? Hustle->Home : NAME_None; // a saved game starts at home
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: estate: %d places, %d people to play%s"), Places.Num(), People.Num(), StartAs.IsNone() ? TEXT("") : *FString::Printf(TEXT("; starting as %s"), *StartAs.ToString()));
}

bool ANHEstate::Place(FPlace& P) const
{
	// On the verge of the nearest road, on whichever side has open ground: first the side the data's point is on.
	const UNHGameData* Data = UNHGameData::Get(this);
	FNHRoadSeg Seg;
	FVector2D OnRoad;
	if (!Data || !Data->NearestRoad(P.Want, Seg, OnRoad))
	{
		return false;
	}
	const FNHRoadWay& Way = Data->RoadWays[Seg.Way];
	const FVector2D Along = (Data->RoadNodes[Way.Nodes[Seg.Index + 1]] - Data->RoadNodes[Way.Nodes[Seg.Index]]).GetSafeNormal();
	const FVector2D Right(-Along.Y, Along.X);
	const float First = FVector2D::DotProduct(P.Want - OnRoad, Right) < 0.f ? -1.f : 1.f;
	// open ground: what is under a spot is the land (within a metre and a half of the roads' height), not a roof or a deck
	const auto Open = [this](const FVector2D& Spot, FVector& OutGround)
	{
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByObjectType(Hit, FVector(Spot, 30000.f), FVector(Spot, -2000.f), FCollisionObjectQueryParams(ECC_WorldStatic)) && FMath::Abs(Hit.ImpactPoint.Z) < 150.f)
		{
			OutGround = Hit.ImpactPoint;
			return true;
		}
		return false;
	};
	// A house or a block of flats wants room behind its board for the building: the road is walked along, up to 800 m
	// either way, for a gap in what the city already has beside it. Anything else, and a home with no gap found, only
	// needs its own spot.
	struct FStop { FVector2D On, Dir; float Half; };
	TArray<FStop> Stops;
	const float OwnHalf = Data->HalfWidth(Way);
	Stops.Add({ OnRoad, Along, OwnHalf });
	for (const int32 Step : { 1, -1 })
	{
		FVector2D From = OnRoad;
		float Gone = 0.f, Next = 4000.f;
		for (int32 I = Step > 0 ? Seg.Index + 1 : Seg.Index; Way.Nodes.IsValidIndex(I) && Gone < 80000.f; I += Step)
		{
			const FVector2D To = Data->RoadNodes[Way.Nodes[I]];
			const float Length = FVector2D::Distance(From, To);
			for (; Next <= Gone + Length; Next += 4000.f)
			{
				Stops.Add({ From + (To - From) * ((Next - Gone) / FMath::Max(Length, 1.f)), (To - From).GetSafeNormal() * static_cast<float>(Step), OwnHalf });
			}
			Gone += Length;
			From = To;
		}
	}
	if (IsHome(P))
	{
		// and the other roads round about, out to a kilometre and a half: a big house is worth a longer walk from where the data put it
		for (float X = -150000.f; X <= 150000.f; X += 15000.f)
		{
			for (float Y = -150000.f; Y <= 150000.f; Y += 15000.f)
			{
				FNHRoadSeg Other;
				FVector2D OnOther;
				if (Data->NearestRoad(P.Want + FVector2D(X, Y), Other, OnOther) && Other.Way != Seg.Way && !Data->RoadWays[Other.Way].bBridge)
				{
					const FNHRoadWay& OtherWay = Data->RoadWays[Other.Way];
					Stops.Add({ OnOther, (Data->RoadNodes[OtherWay.Nodes[Other.Index + 1]] - Data->RoadNodes[OtherWay.Nodes[Other.Index]]).GetSafeNormal(), Data->HalfWidth(OtherWay) });
				}
			}
		}
	}
	Stops.StableSort([&OnRoad](const FStop& A, const FStop& B) { return FVector2D::DistSquared(A.On, OnRoad) < FVector2D::DistSquared(B.On, OnRoad); });
	for (const bool bRoomy : { IsHome(P), false })
	{
		for (const FStop& Stop : Stops)
		{
			const FVector2D Across(-Stop.Dir.Y, Stop.Dir.X);
			for (const float Out : { 900.f, 1500.f, 2400.f })
			{
				for (const float Side : { First, -First })
				{
					const FVector2D Back = Across * Side, Spot = Stop.On + Back * (Stop.Half + Out);
					FVector Ground;
					bool bFits = Open(Spot, Ground);
					for (const FVector2D& Corner : { FVector2D(700.f, -2000.f), FVector2D(700.f, 2000.f), FVector2D(4600.f, -2000.f), FVector2D(4600.f, 2000.f), FVector2D(2600.f, 0.f), FVector2D(1500.f, -900.f), FVector2D(1500.f, 900.f), FVector2D(3800.f, 0.f) })
					{
						FVector Unused;
						bFits = bFits && (!bRoomy || Open(Spot + Back * Corner.X + Stop.Dir * Corner.Y, Unused));
					}
					if (bFits)
					{
						P.At = Ground;
						P.Yaw = (FVector(-Back, 0.f)).Rotation().Yaw;
						P.bPlaced = true;
						return true;
					}
				}
			}
			if (!bRoomy)
			{
				break; // no room needed: only the data's own point is tried
			}
		}
	}
	return false;
}

bool ANHEstate::Owns(const FPlace& P) const
{
	const UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	return Hustle && Hustle->Owned.Contains(P.Id);
}

void ANHEstate::Build(FPlace& P)
{
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	AActor* Holder = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FRotator(0.f, P.Yaw, 0.f), P.At), Params);
	if (!Holder)
	{
		return;
	}
	USceneComponent* Base = NewObject<USceneComponent>(Holder);
	Holder->SetRootComponent(Base);
	Base->SetWorldLocationAndRotation(P.At, FRotator(0.f, P.Yaw, 0.f));
	Base->RegisterComponent();
	const NHEstateLook::FKind& Kind = NHEstateLook::Of(P.Kind);
	const FNHSurface Paint(Kind.Colour, 0.6f), White(FLinearColor(0.85f, 0.85f, 0.82f), 0.7f), Dark(FLinearColor(0.03f, 0.03f, 0.035f), 0.5f), Concrete(FLinearColor(0.35f, 0.35f, 0.33f), 0.9f);
	const FNHSurface Neon(Kind.Colour, 0.4f, 0.f, 0.f, 6.f);
	// X is toward the road, Y along it
	const auto Piece = [Holder, Base](ENHShape Shape, const FVector& Where, const FVector& Size, const FNHSurface& Surface, bool bSolid = true)
	{
		if (UStaticMeshComponent* Part = NHShapes::AddPiece(Holder, Base, Shape, Where, Size, Surface))
		{
			Part->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
			Part->SetCanEverAffectNavigation(false);
		}
	};
	const auto Words = [Holder, Base](const FString& Text, const FVector& Where, float Size, const FColor& Ink, float Turn = 0.f)
	{
		UTextRenderComponent* Line = NewObject<UTextRenderComponent>(Holder);
		Line->SetupAttachment(Base);
		Line->SetRelativeLocationAndRotation(Where, FRotator(0.f, Turn, 0.f));
		Line->SetText(FText::FromString(Text));
		Line->SetWorldSize(Size);
		Line->SetHorizontalAlignment(EHTA_Center);
		Line->SetVerticalAlignment(EVRTA_TextCenter);
		Line->SetTextRenderColor(Ink);
		Line->RegisterComponent();
	};
	const auto Lamp = [Holder, Base](const FVector& Where, const FLinearColor& Colour, float Candelas, float Reach)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(Holder);
		Light->SetupAttachment(Base);
		Light->SetRelativeLocation(Where);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(Candelas);
		Light->SetLightColor(Colour);
		Light->SetAttenuationRadius(Reach);
		Light->SetCastShadows(false);
		Light->RegisterComponent();
	};
	const bool bMine = Owns(P);
	const FString Price = UNHHustleSubsystem::Naira(P.Price);
	if (IsVenue(P))
	{
		// a doorway with the name in lights over it, a strip of carpet, and the colour of the place thrown on the ground
		Piece(ENHShape::Box, FVector(-60.f, 0.f, 190.f), FVector(60.f, 700.f, 380.f), Dark);
		Piece(ENHShape::Box, FVector(-25.f, 0.f, 120.f), FVector(12.f, 130.f, 240.f), FNHSurface(FLinearColor(0.12f, 0.05f, 0.03f), 0.5f)); // the door
		Piece(ENHShape::Box, FVector(-22.f, 0.f, 250.f), FVector(16.f, 170.f, 14.f), Neon, false);
		for (const float Y : { -85.f, 85.f })
		{
			Piece(ENHShape::Box, FVector(-22.f, Y, 125.f), FVector(16.f, 10.f, 250.f), Neon, false);
		}
		Piece(ENHShape::Box, FVector(150.f, 0.f, 2.f), FVector(360.f, 150.f, 3.f), FNHSurface(FLinearColor(0.45f, 0.02f, 0.04f), 0.9f), false);
		Words(P.Name.ToUpper(), FVector(-20.f, 0.f, 325.f), 46.f, Kind.Colour.ToFColor(true));
		Words(Kind.Board, FVector(-20.f, 0.f, 282.f), 22.f, FColor::White);
		Lamp(FVector(160.f, 0.f, 260.f), Kind.Colour, P.Kind == TEXT("bar") ? 60.f : 140.f, 1100.f);
		if (P.Kind == TEXT("strip"))
		{
			// the red-light side of town: red lamps along the verge either side of the door
			for (const float Y : { -1400.f, -700.f, 700.f, 1400.f })
			{
				Piece(ENHShape::Cylinder, FVector(120.f, Y, 190.f), FVector(10.f, 10.f, 380.f), Dark);
				Piece(ENHShape::Sphere, FVector(120.f, Y, 390.f), FVector(34.f), FNHSurface(FLinearColor(1.f, 0.02f, 0.05f), 0.4f, 0.f, 0.f, 8.f), false);
				Lamp(FVector(120.f, Y, 360.f), FLinearColor(1.f, 0.03f, 0.08f), 70.f, 900.f);
			}
		}
	}
	else if (P.Kind == TEXT("petrol"))
	{
		Piece(ENHShape::Box, FVector(-200.f, 0.f, 520.f), FVector(700.f, 1300.f, 40.f), White);       // the canopy
		Piece(ENHShape::Box, FVector(-200.f, 0.f, 495.f), FVector(720.f, 1320.f, 30.f), Paint);
		for (const float Y : { -520.f, 520.f })
		{
			Piece(ENHShape::Cylinder, FVector(-200.f, Y, 250.f), FVector(36.f, 36.f, 500.f), White);
		}
		for (const float Y : { -220.f, 220.f })
		{
			Piece(ENHShape::Box, FVector(-200.f, Y, 12.f), FVector(160.f, 260.f, 24.f), Concrete);      // the island and its pump
			Piece(ENHShape::Box, FVector(-200.f, Y, 100.f), FVector(60.f, 90.f, 170.f), Paint);
			Piece(ENHShape::Box, FVector(-168.f, Y, 140.f), FVector(4.f, 60.f, 40.f), Dark, false);
		}
		Words(P.Name.ToUpper(), FVector(152.f, 0.f, 495.f), 44.f, FColor::White);
		Words(FString::Printf(TEXT("PMS  N%d / LITRE"), PetrolPerLitre), FVector(152.f, 0.f, 440.f), 26.f, FColor::Yellow);
		Lamp(FVector(-200.f, 0.f, 460.f), FLinearColor(1.f, 0.96f, 0.85f), 220.f, 1600.f);
	}
	else
	{
		// an estate agent's board on two posts, and what is being sold behind it
		for (const float Y : { -130.f, 130.f })
		{
			Piece(ENHShape::Box, FVector(0.f, Y, 150.f), FVector(10.f, 10.f, 300.f), Dark);
		}
		Piece(ENHShape::Box, FVector(0.f, 0.f, 235.f), FVector(8.f, 320.f, 170.f), bMine ? FNHSurface(FLinearColor(0.05f, 0.4f, 0.12f), 0.6f) : Paint);
		Words(bMine ? TEXT("PRIVATE PROPERTY") : Kind.Board, FVector(6.f, 0.f, 290.f), 30.f, FColor::White);
		Words(P.Name, FVector(6.f, 0.f, 245.f), 22.f, FColor::White);
		Words(bMine ? TEXT("KEEP OFF") : *Price, FVector(6.f, 0.f, 195.f), 34.f, FColor::Yellow);
		// The buildings made in Blender (Scripts/build_estate_homes.py), where they have been brought in; a fenced plot otherwise
		const FString Model = P.Kind == TEXT("house") ? TEXT("/Game/Estate/SM_Mansion") : P.Kind == TEXT("apartment") ? TEXT("/Game/Estate/SM_Tower") : FString();
		UStaticMesh* Mesh = !Model.IsEmpty() && FPackageName::DoesPackageExist(Model) ? LoadObject<UStaticMesh>(nullptr, *Model) : nullptr;
		const float Deep = Mesh ? Mesh->GetBounds().BoxExtent.X + 500.f : 1500.f, Wide = Mesh ? Mesh->GetBounds().BoxExtent.Y + 300.f : 1100.f;
		bool bClear = true; // nothing of the city's already standing where it would go
		for (const FVector2D& Corner : { FVector2D(-600.f, -Wide), FVector2D(-600.f, Wide), FVector2D(-600.f - 2.f * Deep, -Wide), FVector2D(-600.f - 2.f * Deep, Wide), FVector2D(-600.f - Deep, 0.f) })
		{
			const FVector Over = Base->GetComponentTransform().TransformPosition(FVector(Corner, 0.f));
			FHitResult Hit;
			bClear &= GetWorld()->LineTraceSingleByObjectType(Hit, Over + FVector(0.f, 0.f, 30000.f), Over - FVector(0.f, 0.f, 2000.f), FCollisionObjectQueryParams(ECC_WorldStatic)) && FMath::Abs(Hit.ImpactPoint.Z - P.At.Z) < 150.f;
		}
		if (bClear)
		{
			if (Mesh)
			{
				UStaticMeshComponent* House = NewObject<UStaticMeshComponent>(Holder);
				House->SetStaticMesh(Mesh);
				House->SetupAttachment(Base);
				House->SetRelativeLocation(FVector(-600.f - Deep, 0.f, 0.f));
				House->SetCanEverAffectNavigation(false);
				House->RegisterComponent();
			}
			// the fence round the plot, open at the front
			const FNHSurface Fence = P.Kind == TEXT("land") ? Concrete : White;
			for (const float Y : { -Wide, Wide })
			{
				Piece(ENHShape::Box, FVector(-600.f - Deep, Y, 110.f), FVector(2.f * Deep, 20.f, 220.f), Fence);
			}
			Piece(ENHShape::Box, FVector(-600.f - 2.f * Deep, 0.f, 110.f), FVector(20.f, 2.f * Wide, 220.f), Fence);
			for (const float Y : { -(Wide + 250.f) * 0.5f, (Wide + 250.f) * 0.5f })
			{
				Piece(ENHShape::Box, FVector(-600.f, Y, 110.f), FVector(20.f, Wide - 250.f, 220.f), Fence);
			}
		}
	}
	P.Built = Holder;
}

void ANHEstate::BuildRoom(const FPlace& P)
{
	// One plan for bars and clubs: a room 18 m by 12, the door in the middle of one long wall, the bar along the far
	// one; a club has a stage at one end, with lights on it, and a strip club has poles on the stage. The furniture is
	// shuffled by the place's name, so no two are quite alike.
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Room = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(RoomAt(P)), Params);
	if (!Room)
	{
		return;
	}
	AActor* Holder = Room;
	USceneComponent* Base = NewObject<USceneComponent>(Holder);
	Holder->SetRootComponent(Base);
	Base->SetWorldLocation(RoomAt(P));
	Base->RegisterComponent();
	if (IsHome(P))
	{
		BuildHome(P, Holder, Base);
		return;
	}
	FRandomStream Dice(GetTypeHash(P.Id));
	const NHEstateLook::FKind& Kind = NHEstateLook::Of(P.Kind);
	const bool bBar = P.Kind == TEXT("bar"), bStrip = P.Kind == TEXT("strip");
	const FNHSurface Floor(bBar ? FLinearColor(0.2f, 0.13f, 0.08f) : FLinearColor(0.03f, 0.03f, 0.04f), bBar ? 0.8f : 0.25f), Wall(bBar ? FLinearColor(0.45f, 0.38f, 0.25f) : FLinearColor(0.07f, 0.05f, 0.09f), 0.8f);
	const FNHSurface Wood(FLinearColor(0.25f, 0.12f, 0.05f), 0.5f), Steel(FLinearColor(0.6f, 0.6f, 0.62f), 0.25f, 0.f, 1.f), Seat(bBar ? FLinearColor(0.1f, 0.3f, 0.6f) : FLinearColor(0.35f, 0.02f, 0.06f), 0.6f);
	const FNHSurface Neon(Kind.Colour, 0.4f, 0.f, 0.f, 6.f);
	const auto Piece = [Holder, Base](ENHShape Shape, const FVector& Where, const FVector& Size, const FNHSurface& Surface, bool bSolid = true)
	{
		if (UStaticMeshComponent* Part = NHShapes::AddPiece(Holder, Base, Shape, Where, Size, Surface))
		{
			Part->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
			Part->SetCanEverAffectNavigation(false);
		}
	};
	const auto Lamp = [Holder, Base](const FVector& Where, const FLinearColor& Colour, float Candelas, float Reach)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(Holder);
		Light->SetupAttachment(Base);
		Light->SetRelativeLocation(Where);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(Candelas);
		Light->SetLightColor(Colour);
		Light->SetAttenuationRadius(Reach);
		Light->SetCastShadows(false);
		Light->RegisterComponent();
	};
	// somebody in the room: one of the bodies the project has, standing, or dancing where there is a clip for it
	const auto Body = [Holder, Base, &Dice](const TCHAR* Who, const FVector& Where, float Facing, bool bDance)
	{
		const FString Mesh = FString::Printf(TEXT("/Game/Characters/Player/%s/%s"), Who, Who), Anim = FString::Printf(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed_%s"), Who);
		USkeletalMesh* Skin = FPackageName::DoesPackageExist(Mesh) ? LoadObject<USkeletalMesh>(nullptr, *Mesh) : nullptr;
		UClass* Class = Skin && FPackageName::DoesPackageExist(Anim) ? LoadObject<UClass>(nullptr, *(Anim + TEXT(".ABP_Unarmed_") + Who + TEXT("_C"))) : nullptr;
		if (!Skin || !Class)
		{
			return;
		}
		USkeletalMeshComponent* Person = NewObject<USkeletalMeshComponent>(Holder);
		Person->SetupAttachment(Base);
		Person->SetRelativeLocationAndRotation(Where, FRotator(0.f, Facing - 90.f, 0.f)); // the bodies face along Y
		Person->SetSkeletalMesh(Skin);
		Person->SetAnimInstanceClass(Class);
		Person->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Person->RegisterComponent();
		UNHBodyAnimInstance* Layer = Cast<UNHBodyAnimInstance>(Person->GetAnimInstance());
		if (UAnimSequence* Dance = bDance && Layer ? ANHCharacter::ActionClip(Skin, TEXT("Dance")) : nullptr)
		{
			Layer->SetReach(1.f, 1.f);
			Layer->ShowClip(Dance, Dice.FRandRange(0.85f, 1.15f), true, 0.2f);
		}
	};
	const float L = 1800.f, W = 1200.f, H = 420.f;
	Piece(ENHShape::Box, FVector(0.f, 0.f, -10.f), FVector(L, W, 20.f), Floor);
	Piece(ENHShape::Box, FVector(0.f, 0.f, H + 10.f), FVector(L, W, 20.f), FNHSurface(FLinearColor(0.02f, 0.02f, 0.02f), 0.9f));
	for (const float Y : { -W * 0.5f, W * 0.5f })
	{
		Piece(ENHShape::Box, FVector(0.f, Y, H * 0.5f), FVector(L, 20.f, H), Wall);
	}
	for (const float X : { -L * 0.5f, L * 0.5f })
	{
		Piece(ENHShape::Box, FVector(X, 0.f, H * 0.5f), FVector(20.f, W, H), Wall);
	}
	Piece(ENHShape::Box, FVector(0.f, W * 0.5f - 12.f, 120.f), FVector(130.f, 8.f, 240.f), Wood, false);           // the door, in the near wall
	Piece(ENHShape::Box, FVector(0.f, W * 0.5f - 14.f, 262.f), FVector(90.f, 6.f, 24.f), FNHSurface(FLinearColor(0.1f, 1.f, 0.2f), 0.4f, 0.f, 0.f, 5.f), false); // EXIT
	// the bar along the far wall: counter, shelf, bottles, stools, and somebody behind it
	Piece(ENHShape::Box, FVector(-250.f, -W * 0.5f + 150.f, 55.f), FVector(900.f, 70.f, 110.f), Wood);
	Piece(ENHShape::Box, FVector(-250.f, -W * 0.5f + 150.f, 113.f), FVector(930.f, 90.f, 6.f), bBar ? Wood : Steel);
	Piece(ENHShape::Box, FVector(-250.f, -W * 0.5f + 26.f, 190.f), FVector(900.f, 30.f, 8.f), Wood);
	Piece(ENHShape::Box, FVector(-250.f, -W * 0.5f + 14.f, 250.f), FVector(900.f, 6.f, 110.f), Neon, false);
	for (int32 I = 0; I < 22; ++I)
	{
		const FLinearColor Glass = FLinearColor::MakeFromHSV8(static_cast<uint8>(Dice.RandRange(0, 255)), 200, 150);
		Piece(ENHShape::Cylinder, FVector(-680.f + I * 41.f, -W * 0.5f + 28.f, 208.f), FVector(9.f, 9.f, Dice.FRandRange(24.f, 36.f)), FNHSurface(Glass, 0.15f), false);
	}
	for (int32 I = 0; I < 7; ++I)
	{
		Piece(ENHShape::Cylinder, FVector(-640.f + I * 130.f, -W * 0.5f + 235.f, 38.f), FVector(8.f, 8.f, 76.f), Steel);
		Piece(ENHShape::Cylinder, FVector(-640.f + I * 130.f, -W * 0.5f + 235.f, 80.f), FVector(38.f, 38.f, 8.f), Seat);
	}
	static const TCHAR* Women[] = { TEXT("Amaka"), TEXT("Zainab"), TEXT("Ngozi"), TEXT("Kate"), TEXT("Mei"), TEXT("Priya") };
	static const TCHAR* Men[] = { TEXT("Tunde"), TEXT("Emeka"), TEXT("Dayo"), TEXT("Mark"), TEXT("Chen") };
	Body(Men[Dice.RandRange(0, 4)], FVector(-250.f, -W * 0.5f + 80.f, 0.f), 90.f, false);                           // behind the counter
	// tables and chairs, or booths, about the floor
	for (int32 I = 0; I < (bBar ? 6 : 4); ++I)
	{
		const FVector Table(-600.f + (I % 3) * 330.f + Dice.FRandRange(-40.f, 40.f), 60.f + (I / 3) * 300.f + Dice.FRandRange(-30.f, 30.f), 0.f);
		Piece(ENHShape::Cylinder, Table + FVector(0.f, 0.f, 36.f), FVector(10.f, 10.f, 72.f), Steel);
		Piece(ENHShape::Cylinder, Table + FVector(0.f, 0.f, 74.f), FVector(90.f, 90.f, 5.f), bBar ? FNHSurface(FLinearColor(0.8f, 0.8f, 0.78f), 0.5f) : Wood);
		for (int32 Chair = 0; Chair < 3; ++Chair)
		{
			const FVector Out = FRotator(0.f, Chair * 120.f + Dice.FRandRange(0.f, 60.f), 0.f).Vector() * 85.f;
			Piece(ENHShape::Box, Table + Out + FVector(0.f, 0.f, 24.f), FVector(42.f, 42.f, 48.f), Seat);
		}
		if (Dice.FRand() < 0.7f)
		{
			Body(Dice.FRand() < 0.5f ? Men[Dice.RandRange(0, 4)] : Women[Dice.RandRange(0, 5)], Table + FVector(Dice.FRandRange(-30.f, 30.f), 120.f, 0.f), -90.f + Dice.FRandRange(-40.f, 40.f), !bBar);
		}
	}
	Lamp(FVector(-250.f, -W * 0.5f + 200.f, 330.f), bBar ? FLinearColor(1.f, 0.8f, 0.5f) : Kind.Colour, bBar ? 160.f : 90.f, 1400.f);
	Lamp(FVector(-300.f, 250.f, 380.f), bBar ? FLinearColor(1.f, 0.85f, 0.6f) : FLinearColor(0.5f, 0.2f, 1.f), bBar ? 220.f : 50.f, 1600.f);
	if (!bBar)
	{
		// the stage at one end, a DJ's desk beside it, and coloured lights over both
		Piece(ENHShape::Box, FVector(L * 0.5f - 260.f, 0.f, 30.f), FVector(480.f, 760.f, 60.f), FNHSurface(FLinearColor(0.02f, 0.02f, 0.02f), 0.15f));
		Piece(ENHShape::Box, FVector(L * 0.5f - 505.f, 0.f, 58.f), FVector(10.f, 770.f, 8.f), Neon, false);
		Piece(ENHShape::Box, FVector(L * 0.5f - 120.f, -480.f, 60.f), FVector(160.f, 180.f, 120.f), FNHSurface(FLinearColor(0.05f, 0.05f, 0.06f), 0.4f));
		Body(Men[Dice.RandRange(0, 4)], FVector(L * 0.5f - 60.f, -480.f, 0.f), 180.f, false);
		const int32 Dancers = bStrip ? 3 : 2;
		for (int32 I = 0; I < Dancers; ++I)
		{
			const FVector Spot(L * 0.5f - 300.f + (I % 2) * 110.f, -240.f + I * (480.f / FMath::Max(Dancers - 1, 1)), 60.f);
			if (bStrip)
			{
				Piece(ENHShape::Cylinder, Spot + FVector(40.f, 0.f, (H - 60.f) * 0.5f), FVector(6.f, 6.f, H - 60.f), Steel);
			}
			Body(Women[(Dice.RandRange(0, 5) + I) % 6], Spot, 180.f + Dice.FRandRange(-30.f, 30.f), true);
		}
		Lamp(FVector(L * 0.5f - 300.f, -200.f, 360.f), bStrip ? FLinearColor(1.f, 0.05f, 0.2f) : FLinearColor(0.1f, 0.4f, 1.f), 200.f, 900.f);
		Lamp(FVector(L * 0.5f - 300.f, 200.f, 360.f), bStrip ? FLinearColor(1.f, 0.2f, 0.7f) : FLinearColor(1.f, 0.1f, 0.8f), 200.f, 900.f);
		// and the floor in front of it, with people on it
		for (int32 I = 0; I < 5; ++I)
		{
			Body(I % 2 ? Men[Dice.RandRange(0, 4)] : Women[Dice.RandRange(0, 5)], FVector(120.f + Dice.FRandRange(0.f, 300.f), -300.f + I * 130.f, 0.f), Dice.FRandRange(-40.f, 40.f), true);
		}
	}
}

void ANHEstate::BuildHome(const FPlace& P, AActor* Holder, USceneComponent* Base)
{
	// The inside of a house or a flat: one big room 18 m by 12 with a wall of window, a sitting area round a television,
	// a dining table, a kitchen along one end and a bed behind a half wall at the other. The same plan for every home;
	// a flat looks out on sky, a house on its garden, and the colours come from the place's name.
	const bool bFlat = P.Kind == TEXT("apartment");
	if (UStaticMesh* Model = HomeModel(P))
	{
		// A whole room brought in from a model (Scripts/import_interiors.py): a loft for a flat, a drawing room for a
		// house. Its origin is a clear spot on its floor, where the player comes in; lamps are hung through it here.
		UStaticMeshComponent* Shell = NewObject<UStaticMeshComponent>(Holder);
		Shell->SetStaticMesh(Model);
		Shell->SetupAttachment(Base);
		Shell->SetCanEverAffectNavigation(false);
		Shell->RegisterComponent();
		const FBox Box = Model->GetBoundingBox();
		const FVector Size = Box.GetSize();
		const int32 Across = FMath::Max(1, FMath::RoundToInt(Size.X / 450.f)), Down = FMath::Max(1, FMath::RoundToInt(Size.Y / 450.f));
		for (int32 I = 0; I < Across; ++I)
		{
			for (int32 J = 0; J < Down; ++J)
			{
				UPointLightComponent* Light = NewObject<UPointLightComponent>(Holder);
				Light->SetupAttachment(Base);
				Light->SetRelativeLocation(FVector(Box.Min.X + (I + 0.5f) * Size.X / Across, Box.Min.Y + (J + 0.5f) * Size.Y / Down, Box.Min.Z + FMath::Min(Size.Z * 0.72f, 300.f)));
				Light->SetIntensityUnits(ELightUnits::Candelas);
				Light->SetIntensity(bFlat ? 110.f : 150.f);
				Light->SetLightColor(FLinearColor(1.f, 0.86f, 0.68f));
				Light->SetAttenuationRadius(900.f);
				Light->SetCastShadows(false);
				Light->RegisterComponent();
			}
		}
		return;
	}
	FRandomStream Dice(GetTypeHash(P.Id));
	const FLinearColor Accent = FLinearColor::MakeFromHSV8(static_cast<uint8>(Dice.RandRange(0, 255)), 150, 120);
	const FNHSurface Floor(FLinearColor(0.32f, 0.2f, 0.11f), 0.35f), Wall(FLinearColor(0.82f, 0.8f, 0.76f), 0.85f), Stone(FLinearColor(0.75f, 0.75f, 0.73f), 0.2f), Dark(FLinearColor(0.03f, 0.03f, 0.035f), 0.4f);
	const FNHSurface Fabric(Accent, 0.9f), Cream(FLinearColor(0.85f, 0.82f, 0.74f), 0.9f), Wood(FLinearColor(0.22f, 0.11f, 0.05f), 0.45f), Steel(FLinearColor(0.6f, 0.6f, 0.62f), 0.25f, 0.f, 1.f), Green(FLinearColor(0.06f, 0.22f, 0.06f), 0.9f);
	const FNHSurface Day(bFlat ? FLinearColor(0.45f, 0.7f, 1.f) : FLinearColor(0.55f, 0.8f, 0.6f), 0.3f, 0.f, 0.f, 4.f), Screen(FLinearColor(0.2f, 0.5f, 0.9f), 0.2f, 0.f, 0.f, 3.f), Glow(FLinearColor(1.f, 0.8f, 0.5f), 0.4f, 0.f, 0.f, 6.f);
	const auto Piece = [Holder, Base](ENHShape Shape, const FVector& Where, const FVector& Size, const FNHSurface& Surface, bool bSolid = true)
	{
		if (UStaticMeshComponent* Part = NHShapes::AddPiece(Holder, Base, Shape, Where, Size, Surface))
		{
			Part->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
			Part->SetCanEverAffectNavigation(false);
		}
	};
	const auto Lamp = [Holder, Base](const FVector& Where, const FLinearColor& Colour, float Candelas, float Reach)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(Holder);
		Light->SetupAttachment(Base);
		Light->SetRelativeLocation(Where);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(Candelas);
		Light->SetLightColor(Colour);
		Light->SetAttenuationRadius(Reach);
		Light->SetCastShadows(false);
		Light->RegisterComponent();
	};
	const float L = 1800.f, W = 1200.f, H = 380.f;
	Piece(ENHShape::Box, FVector(0.f, 0.f, -10.f), FVector(L, W, 20.f), Floor);
	Piece(ENHShape::Box, FVector(0.f, 0.f, H + 10.f), FVector(L, W, 20.f), FNHSurface(FLinearColor(0.9f, 0.9f, 0.88f), 0.9f));
	Piece(ENHShape::Box, FVector(0.f, W * 0.5f, H * 0.5f), FVector(L, 20.f, H), Wall);
	for (const float X : { -L * 0.5f, L * 0.5f })
	{
		Piece(ENHShape::Box, FVector(X, 0.f, H * 0.5f), FVector(20.f, W, H), Wall);
	}
	// the far wall is window from knee to ceiling, with the day outside it
	Piece(ENHShape::Box, FVector(0.f, -W * 0.5f, 25.f), FVector(L, 20.f, 50.f), Wall);
	Piece(ENHShape::Box, FVector(0.f, -W * 0.5f - 6.f, H * 0.5f + 25.f), FVector(L, 6.f, H - 50.f), Day, false);
	Piece(ENHShape::Box, FVector(0.f, -W * 0.5f + 4.f, H * 0.5f + 25.f), FVector(L, 8.f, H - 50.f), FNHSurface(FLinearColor(0.4f, 0.55f, 0.6f), 0.05f)); // the glass: it stops you
	for (float X = -L * 0.5f; X <= L * 0.5f; X += 300.f)
	{
		Piece(ENHShape::Box, FVector(X, -W * 0.5f + 10.f, H * 0.5f + 25.f), FVector(8.f, 10.f, H - 50.f), Dark, false);
	}
	Piece(ENHShape::Box, FVector(0.f, W * 0.5f - 12.f, 120.f), FVector(130.f, 8.f, 240.f), Wood, false);                         // the front door
	// sitting: an L of sofa on a rug, a low table, the television on the wall with a unit under it
	Piece(ENHShape::Box, FVector(-150.f, -150.f, 1.f), FVector(520.f, 420.f, 2.f), Cream, false);
	Piece(ENHShape::Box, FVector(-150.f, 60.f, 38.f), FVector(420.f, 110.f, 76.f), Fabric);
	Piece(ENHShape::Box, FVector(-150.f, 105.f, 75.f), FVector(420.f, 30.f, 70.f), Fabric);
	Piece(ENHShape::Box, FVector(-395.f, -110.f, 38.f), FVector(110.f, 260.f, 76.f), Fabric);
	for (const float X : { -280.f, -150.f, -20.f })
	{
		Piece(ENHShape::Box, FVector(X, 60.f, 84.f), FVector(60.f, 60.f, 16.f), Cream, false);
	}
	Piece(ENHShape::Box, FVector(-150.f, -150.f, 22.f), FVector(200.f, 100.f, 44.f), Stone);
	Piece(ENHShape::Box, FVector(-150.f, -W * 0.5f + 60.f, 30.f), FVector(360.f, 60.f, 60.f), Wood);
	Piece(ENHShape::Box, FVector(-150.f, -W * 0.5f + 40.f, 150.f), FVector(300.f, 10.f, 170.f), Dark, false);
	Piece(ENHShape::Box, FVector(-150.f, -W * 0.5f + 46.f, 150.f), FVector(284.f, 4.f, 156.f), Screen, false);
	// eating: a long table and six chairs, a light hung low over it
	Piece(ENHShape::Box, FVector(330.f, 180.f, 74.f), FVector(300.f, 120.f, 8.f), Wood);
	for (const float X : { 200.f, 460.f })
	{
		Piece(ENHShape::Box, FVector(X, 180.f, 36.f), FVector(12.f, 90.f, 72.f), Dark);
	}
	for (int32 I = 0; I < 6; ++I)
	{
		Piece(ENHShape::Box, FVector(230.f + (I % 3) * 100.f, I < 3 ? 95.f : 265.f, 24.f), FVector(48.f, 48.f, 48.f), Cream);
	}
	Piece(ENHShape::Box, FVector(330.f, 180.f, 250.f), FVector(220.f, 12.f, 8.f), Glow, false);
	// the kitchen along the right-hand end: units, an island, a tall fridge
	Piece(ENHShape::Box, FVector(L * 0.5f - 45.f, -60.f, 46.f), FVector(70.f, 700.f, 92.f), FNHSurface(FLinearColor(0.1f, 0.12f, 0.14f), 0.4f));
	Piece(ENHShape::Box, FVector(L * 0.5f - 45.f, -60.f, 95.f), FVector(76.f, 706.f, 6.f), Stone);
	Piece(ENHShape::Box, FVector(L * 0.5f - 40.f, -60.f, 250.f), FVector(60.f, 700.f, 80.f), FNHSurface(FLinearColor(0.1f, 0.12f, 0.14f), 0.4f));
	Piece(ENHShape::Box, FVector(L * 0.5f - 50.f, 340.f, 110.f), FVector(80.f, 90.f, 220.f), Steel);
	Piece(ENHShape::Box, FVector(L * 0.5f - 260.f, -80.f, 46.f), FVector(110.f, 320.f, 92.f), FNHSurface(FLinearColor(0.1f, 0.12f, 0.14f), 0.4f));
	Piece(ENHShape::Box, FVector(L * 0.5f - 260.f, -80.f, 95.f), FVector(120.f, 330.f, 6.f), Stone);
	for (const float Y : { -170.f, -80.f, 10.f })
	{
		Piece(ENHShape::Cylinder, FVector(L * 0.5f - 350.f, Y, 36.f), FVector(34.f, 34.f, 72.f), Steel);
	}
	// sleeping, behind a half wall at the left-hand end: the bed, its tables and lamps, a wardrobe
	Piece(ENHShape::Box, FVector(-L * 0.5f + 420.f, 120.f, 130.f), FVector(16.f, 520.f, 260.f), Wall);
	Piece(ENHShape::Box, FVector(-L * 0.5f + 190.f, -120.f, 28.f), FVector(230.f, 200.f, 56.f), Wood);
	Piece(ENHShape::Box, FVector(-L * 0.5f + 200.f, -120.f, 62.f), FVector(210.f, 190.f, 14.f), Cream);
	Piece(ENHShape::Box, FVector(-L * 0.5f + 150.f, -120.f, 72.f), FVector(110.f, 192.f, 10.f), Fabric, false);
	Piece(ENHShape::Box, FVector(-L * 0.5f + 60.f, -120.f, 80.f), FVector(16.f, 230.f, 160.f), Wood);
	for (const float Y : { -265.f, 25.f })
	{
		Piece(ENHShape::Box, FVector(-L * 0.5f + 90.f, Y, 26.f), FVector(50.f, 50.f, 52.f), Wood);
		Piece(ENHShape::Cylinder, FVector(-L * 0.5f + 90.f, Y, 72.f), FVector(22.f, 22.f, 36.f), Glow, false);
	}
	Piece(ENHShape::Box, FVector(-L * 0.5f + 220.f, W * 0.5f - 45.f, 125.f), FVector(380.f, 70.f, 250.f), Wood);
	// pictures on the walls, plants in the corners
	for (int32 I = 0; I < 3; ++I)
	{
		Piece(ENHShape::Box, FVector(-250.f + I * 330.f, W * 0.5f - 12.f, 200.f), FVector(150.f, 6.f, 110.f), FNHSurface(FLinearColor::MakeFromHSV8(static_cast<uint8>(Dice.RandRange(0, 255)), 170, 150), 0.7f), false);
	}
	for (const FVector2D& Pot : { FVector2D(-470.f, -520.f), FVector2D(560.f, -520.f), FVector2D(120.f, 540.f) })
	{
		Piece(ENHShape::Cylinder, FVector(Pot, 25.f), FVector(50.f, 50.f, 50.f), Stone);
		Piece(ENHShape::Cone, FVector(Pot, 115.f), FVector(90.f, 90.f, 150.f), Green, false);
	}
	Lamp(FVector(-150.f, -100.f, 320.f), FLinearColor(1.f, 0.9f, 0.75f), 260.f, 1500.f);
	Lamp(FVector(330.f, 180.f, 230.f), FLinearColor(1.f, 0.85f, 0.6f), 160.f, 900.f);
	Lamp(FVector(L * 0.5f - 200.f, -80.f, 320.f), FLinearColor(1.f, 0.96f, 0.9f), 220.f, 1100.f);
	Lamp(FVector(-L * 0.5f + 190.f, -120.f, 300.f), FLinearColor(1.f, 0.8f, 0.55f), 120.f, 900.f);
	Lamp(FVector(0.f, -W * 0.5f + 120.f, 250.f), bFlat ? FLinearColor(0.6f, 0.8f, 1.f) : FLinearColor(0.7f, 1.f, 0.75f), 180.f, 1600.f);
}

UStaticMesh* ANHEstate::HomeModel(const FPlace& P) const
{
	const FString Path = P.Kind == TEXT("apartment") ? TEXT("/Game/Interiors/Loft/SM_Loft") : P.Kind == TEXT("house") ? TEXT("/Game/Interiors/Classic/SM_Classic") : FString();
	return !Path.IsEmpty() && FPackageName::DoesPackageExist(Path) ? LoadObject<UStaticMesh>(nullptr, *Path) : nullptr;
}

bool ANHEstate::Travel(FName Id)
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const FPlace* P = Places.FindByPredicate([Id](const FPlace& Have) { return Have.Id == Id; });
	if (!Hustle || !Pawn || !P || !Owns(*P))
	{
		return false;
	}
	if (Hustle->Stars() > 0)
	{
		ANHHUD::Toast(this, TEXT("Not with Task Force on your neck. Lose them first."), 2);
		return false;
	}
	const FVector From = Inside != INDEX_NONE ? Outside : Pawn->GetActorLocation();
	if (!GoTo(Id))
	{
		return false;
	}
	// the drive it would have been: the straight distance and a third again, at 50 km/h
	const float Minutes = FVector::Dist2D(From, P->At) / 100000.f * 1.33f / 50.f * 60.f;
	Hustle->Minutes += Minutes;
	Mode = EMode::None;
	ANHHUD::Toast(this, FString::Printf(TEXT("%s, %s. %.0f minutes on the road."), *P->Name, *P->Area, Minutes), 1);
	return true;
}

void ANHEstate::LeaveRoom(APawn* Pawn)
{
	if (Pawn)
	{
		Pawn->SetActorLocation(Outside, false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (Room)
	{
		Room->Destroy();
		Room = nullptr;
	}
	Inside = INDEX_NONE;
}

ANHVehicle* ANHEstate::CarAt(const FPlace& P, const APawn* Pawn) const
{
	if (ANHVehicle* Driving = Cast<ANHVehicle>(const_cast<APawn*>(Pawn)))
	{
		return Driving;
	}
	ANHVehicle* Best = nullptr;
	float BestD = FMath::Square(1500.f);
	TArray<AActor*> Cars;
	UGameplayStatics::GetAllActorsOfClass(this, ANHVehicle::StaticClass(), Cars);
	for (AActor* Car : Cars)
	{
		ANHVehicle* V = Cast<ANHVehicle>(Car);
		const float D = FVector::DistSquared2D(Car->GetActorLocation(), P.At);
		if (V && (V->bOwned || V->bPlayerOwned) && D < BestD)
		{
			Best = V;
			BestD = D;
		}
	}
	return Best;
}

void ANHEstate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	if (!StartAs.IsNone() && GetWorld()->GetTimeSeconds() > 3.f)
	{
		const FName Who = StartAs;
		StartAs = NAME_None;
		PlayAs(Who);
	}
	if (!StartHome.IsNone() && GetWorld()->GetTimeSeconds() > 3.f)
	{
		GoTo(StartHome);
		StartHome = NAME_None;
	}
	GarageLook -= DeltaSeconds;
	if (UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this); Hustle && GarageLook <= 0.f)
	{
		// what the kept cars have been through since they were last looked at
		GarageLook = 1.f;
		for (FNHGarageCar& Record : Hustle->Cars)
		{
			if (const ANHVehicle* Car = Standing(Record.Serial))
			{
				Record.Fuel = Car->Fuel;
				Record.bWrecked = Car->IsWrecked();
			}
		}
	}
	Look -= DeltaSeconds;
	if (Look > 0.f)
	{
		return;
	}
	Look = 0.25f;
	if (Inside != INDEX_NONE)
	{
		Near = Inside;
		return;
	}
	const FVector Here = Pawn->GetActorLocation();
	const bool bDriving = Cast<ANHVehicle>(Pawn) != nullptr;
	Near = INDEX_NONE;
	float Best = BIG_NUMBER;
	for (int32 I = 0; I < Places.Num(); ++I)
	{
		FPlace& P = Places[I];
		const float Far = P.bPlaced ? FVector::Dist2D(Here, P.At) : FVector2D::Distance(FVector2D(Here), P.Want);
		if (Far < 60000.f && !P.bPlaced && !Place(P))
		{
			continue;
		}
		if (P.bPlaced && Far < 50000.f && !P.Built.IsValid())
		{
			Build(P);
			if (IsHome(P) && Owns(P))
			{
				Park(P);
			}
		}
		else if (Far > 70000.f && P.Built.IsValid())
		{
			P.Built->Destroy();
		}
		// in reach: on foot at the board or the door; at the wheel, only a filling station
		const float Reach = P.Kind == TEXT("petrol") ? 1100.f : 450.f;
		if (P.bPlaced && Far < Reach && Far < Best && (!bDriving || P.Kind == TEXT("petrol")))
		{
			Best = Far;
			Near = I;
		}
	}
	if (Mode == EMode::Place && Near != Shown)
	{
		Mode = EMode::None; // walked off
	}
}

FString ANHEstate::Prompt(const APawn* Pawn) const
{
	if (Mode != EMode::None || !Places.IsValidIndex(Near))
	{
		return FString();
	}
	const FPlace& P = Places[Near];
	return Inside != INDEX_NONE ? (IsHome(P) ? FString::Printf(TEXT("E  %s: save, sleep, or the door"), *P.Name) : FString::Printf(TEXT("E  %s: the bar, or the door"), *P.Name))
		: IsVenue(P) ? FString::Printf(TEXT("E  %s"), *P.Name)
		: P.Kind == TEXT("petrol") ? FString::Printf(TEXT("E  %s: petrol"), *P.Name)
		: Owns(P) ? FString::Printf(TEXT("E  %s (yours)"), *P.Name) : FString::Printf(TEXT("E  %s, %s"), *P.Name, *UNHHustleSubsystem::Naira(P.Price));
}

bool ANHEstate::Interact(APawn* Pawn)
{
	if (Mode != EMode::None)
	{
		Mode = EMode::None;
		return true;
	}
	if (!Places.IsValidIndex(Near))
	{
		return false;
	}
	Shown = Near;
	Mode = EMode::Place;
	return true;
}

void ANHEstate::OpenPeople()
{
	Mode = Mode == EMode::People ? EMode::None : EMode::People;
}

void ANHEstate::Menu(FString& OutTitle, FString& OutHeading, TArray<FNHMenuLine>& OutLines) const
{
	const UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const FString Have = Hustle ? FString::Printf(TEXT("You have %s"), *UNHHustleSubsystem::Naira(Hustle->Worth())) : FString();
	if (Mode == EMode::People)
	{
		OutTitle = TEXT("WHO YOU BE");
		OutHeading = TEXT("CHOOSE A LIFE");
		for (const FPerson& Who : People)
		{
			const FPlace* Home = Places.FindByPredicate([&Who](const FPlace& P) { return P.Id == Who.Home; });
			OutLines.Add({ Who.Name, UNHHustleSubsystem::Naira(Who.Bank), FString::Printf(TEXT("%s  Home: %s, %s. %d cars outside."), *Who.About, Home ? *Home->Name : TEXT("none"), Home ? *Home->Area : TEXT(""), Who.Cars.Num()) });
		}
		OutLines.Add({ TEXT("The conductor"), TEXT("N5,000"), TEXT("Back to Oshodi Motor Park with nothing: the game as it starts.") });
		return;
	}
	if (!Places.IsValidIndex(Shown))
	{
		return;
	}
	const FPlace& P = Places[Shown];
	const bool bMine = Owns(P);
	OutTitle = P.Name.ToUpper();
	OutHeading = FString::Printf(TEXT("%s   %s"), *P.Area.ToUpper(), *Have);
	// the other places that are the player's, to be driven to in a blink
	const auto Journeys = [this, &P, &OutLines]()
	{
		for (const FPlace& Other : Places)
		{
			if (Other.Id != P.Id && Owns(Other))
			{
				OutLines.Add({ FString::Printf(TEXT("Travel to %s"), *Other.Name), Other.Area, TEXT("Your driver takes you. The clock moves on by the drive.") });
			}
		}
	};
	if (Inside == Shown && IsHome(P))
	{
		OutLines.Add({ TEXT("Save the game"), FString(), TEXT("Your money, what you own, the day and the hour. You start from home next time.") });
		OutLines.Add({ TEXT("Sleep till morning"), FString(), TEXT("Wake at seven with your health back. Saves the game.") });
		OutLines.Add({ TEXT("Wardrobe"), FString(), TEXT("Change what you are wearing.") });
		OutLines.Add({ TEXT("Watch television"), FString(), TEXT("Half an hour of the news.") });
		Journeys();
		OutLines.Add({ TEXT("Go out"), FString(), TEXT("Back to the street.") });
		return;
	}
	if (Inside == Shown)
	{
		if (const TArray<FDrink>* Card = Menus.Find(P.Kind))
		{
			for (const FDrink& D : *Card)
			{
				OutLines.Add({ D.Name, bMine ? TEXT("on the house") : *UNHHustleSubsystem::Naira(static_cast<int64>(D.Price)), D.Heal > 0.f ? FString::Printf(TEXT("Good for %.0f health."), D.Heal) : TEXT("For the look of it.") });
			}
		}
		OutLines.Add({ TEXT("Go out"), FString(), TEXT("Back to the street.") });
		return;
	}
	if (IsVenue(P))
	{
		OutLines.Add({ TEXT("Go in"), bMine || P.Fee == 0 ? TEXT("free") : *UNHHustleSubsystem::Naira(static_cast<int64>(P.Fee)), P.About });
	}
	else if (P.Kind == TEXT("petrol"))
	{
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const ANHVehicle* Car = CarAt(P, PC ? PC->GetPawn() : nullptr);
		const float Litres = Car ? (1.f - Car->Fuel) * Car->TankLitres() : 0.f;
		OutLines.Add({ TEXT("Fill the tank"), Car ? *UNHHustleSubsystem::Naira(static_cast<int64>(FMath::CeilToInt(Litres) * PetrolPerLitre)) : TEXT("no car here"),
			Car ? FString::Printf(TEXT("%s: %.0f litres to full."), *Car->DisplayName(), Litres) : TEXT("Drive up to the pumps, or leave your own car by them.") });
		OutLines.Add({ TEXT("A jerry can of petrol"), UNHHustleSubsystem::Naira(static_cast<int64>(20 * PetrolPerLitre + 6000)), TEXT("Twenty litres to carry. Use it from the bag beside a car.") });
	}
	if (P.Price > 0 && !bMine)
	{
		OutLines.Add({ IsVenue(P) ? TEXT("Buy the business") : TEXT("Buy it"), UNHHustleSubsystem::Naira(P.Price), IsVenue(P) ? TEXT("Yours to walk into and drink in free.") : P.About });
	}
	else if (bMine)
	{
		if (IsHome(P))
		{
			OutLines.Add({ TEXT("Go in"), FString(), TEXT("Home. Save the game there, sleep, or change your clothes.") });
			const APlayerController* Who = GetWorld()->GetFirstPlayerController();
			const ANHVehicle* Car = CarAt(P, Who ? Who->GetPawn() : nullptr);
			OutLines.Add({ TEXT("Keep a car here"), FString::Printf(TEXT("%d of %d"), KeptAt(P.Id), Slots(P)), Car && Car->GarageSerial == 0 ? FString::Printf(TEXT("The %s by the board goes into this garage: saved, insured, and the mechanic can bring it to you."), *Car->DisplayName())
				: TEXT("Leave a car of yours by the board first. Kept cars are saved with the game.") });
			OutLines.Add({ TEXT("Rest till morning"), FString(), TEXT("Sleep, and wake at seven with your health back.") });
			OutLines.Add({ Hustle && Hustle->Home == P.Id ? TEXT("This is home") : TEXT("Make this home"), FString(), TEXT("Where you start from.") });
		}
		Journeys();
		OutLines.Add({ TEXT("Sell it"), UNHHustleSubsystem::Naira(P.Price * 8 / 10), TEXT("An agent takes it off you today, for four fifths of its price.") });
	}
	OutLines.Add({ TEXT("Leave"), FString(), FString() });
}

void ANHEstate::Choose(int32 Line)
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	FString Title, Heading;
	TArray<FNHMenuLine> Lines;
	Menu(Title, Heading, Lines);
	if (!Hustle || !Pawn || !Lines.IsValidIndex(Line))
	{
		return;
	}
	const FString What = Lines[Line].Label;
	if (Mode == EMode::People)
	{
		Mode = EMode::None;
		if (People.IsValidIndex(Line))
		{
			PlayAs(People[Line].Id);
		}
		else
		{
			Hustle->ResetProgress();
			PC->ConsoleCommand(TEXT("NHGoto park"));
			ANHHUD::Toast(this, TEXT("Back to Oshodi with N5,000"), 0);
		}
		return;
	}
	if (!Places.IsValidIndex(Shown))
	{
		return;
	}
	FPlace& P = Places[Shown];
	ANHCharacter* Me = Cast<ANHCharacter>(Pawn);
	const auto Rebuild = [&P]()
	{
		if (P.Built.IsValid())
		{
			P.Built->Destroy(); // the board says something else now: it is built again on the next look round
		}
	};
	if (What == TEXT("Leave"))
	{
		Mode = EMode::None;
	}
	else if (What == TEXT("Go out"))
	{
		Mode = EMode::None;
		LeaveRoom(Pawn);
	}
	else if (What.StartsWith(TEXT("Travel to ")))
	{
		const FString Name = What.RightChop(10);
		if (const FPlace* To = Places.FindByPredicate([&Name, this](const FPlace& Have) { return Have.Name == Name && Owns(Have); }))
		{
			Travel(To->Id);
		}
	}
	else if (What == TEXT("Keep a car here"))
	{
		ANHVehicle* Car = CarAt(P, Pawn);
		if (!Car || Car->GarageSerial != 0)
		{
			ANHHUD::Toast(this, Car ? TEXT("That one is already kept") : TEXT("No car of yours by the board"), 0);
		}
		else if (Keep(Car, P.Id))
		{
			ANHHUD::Toast(this, FString::Printf(TEXT("The %s is kept at %s"), *Car->DisplayName(), *P.Name), 1);
		}
		else
		{
			ANHHUD::Toast(this, TEXT("The garage is full"), 2);
		}
	}
	else if (What == TEXT("Wardrobe"))
	{
		Mode = EMode::None;
		if (ANHHUD* H = ANHHUD::Get(this))
		{
			H->OpenWardrobe();
		}
	}
	else if (What == TEXT("Watch television"))
	{
		static const TCHAR* News[] = { TEXT("\"...and the naira closed stronger today...\""), TEXT("\"...Task Force say the city is calm. The city disagrees...\""), TEXT("\"...fuel queues are back in Apapa...\""),
			TEXT("\"...Third Mainland Bridge: repairs to finish 'soon'...\""), TEXT("\"...a new tower is rising in Eko Atlantic...\"") };
		Hustle->Minutes += 30.f;
		ANHHUD::Toast(this, News[FMath::RandRange(0, UE_ARRAY_COUNT(News) - 1)], 0);
	}
	else if (What == TEXT("Save the game"))
	{
		Hustle->Home = P.Id;
		Hustle->Save();
		ANHHUD::Toast(this, TEXT("Game saved. You start from here next time."), 1);
	}
	else if (What == TEXT("Sleep till morning") && Me)
	{
		const float Day = FMath::FloorToFloat(Hustle->Minutes / 1440.f) * 1440.f;
		Hustle->Minutes = Hustle->Minutes < Day + 420.f ? Day + 420.f : Day + 1440.f + 420.f;
		Me->Health = 100.f;
		Hustle->Home = P.Id;
		Hustle->Save();
		ANHHUD::Toast(this, TEXT("Seven o'clock. Game saved."), 1);
	}
	else if (What == TEXT("Go in") && Me)
	{
		if (!Owns(P) && !Hustle->Pay(P.Fee, FString::Printf(TEXT("gate fee, %s"), *P.Name)))
		{
			ANHHUD::Toast(this, TEXT("Bouncer: \"No money, no entry.\""), 2);
			return;
		}
		Outside = Pawn->GetActorLocation();
		BuildRoom(P);
		Inside = Shown;
		// by the door of a room built here; on the clear spot a brought-in room is centred on
		Pawn->SetActorLocation(RoomAt(P) + (IsHome(P) && HomeModel(P) ? FVector(0.f, 0.f, 110.f) : FVector(0.f, 480.f, 100.f)), false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(FRotator(0.f, -90.f, 0.f));
		Mode = EMode::None;
		ANHHUD::Toast(this, FString::Printf(TEXT("%s, %s"), *P.Name, *P.Area), 0);
	}
	else if (What == TEXT("Fill the tank"))
	{
		ANHVehicle* Car = CarAt(P, Pawn);
		const int32 Litres = Car ? FMath::CeilToInt((1.f - Car->Fuel) * Car->TankLitres()) : 0;
		if (Litres <= 0)
		{
			ANHHUD::Toast(this, Car ? TEXT("Tank don full") : TEXT("No car at the pump"), 0);
		}
		else if (Hustle->Pay(static_cast<int64>(Litres) * PetrolPerLitre, TEXT("petrol")))
		{
			Car->Fuel = 1.f;
			ANHHUD::Toast(this, FString::Printf(TEXT("%d litres. Full tank."), Litres), 1);
		}
		else
		{
			ANHHUD::Toast(this, TEXT("Money no reach"), 2);
		}
	}
	else if (What == TEXT("A jerry can of petrol") && Me)
	{
		const int32 Cost = 20 * PetrolPerLitre + 6000;
		if (Hustle->Worth() < Cost)
		{
			ANHHUD::Toast(this, TEXT("Money no reach"), 2);
		}
		else if (Me->GetInventory() && Me->GetInventory()->Add(TEXT("jerry_can"), 1, TEXT("bought")))
		{
			Hustle->Pay(Cost, TEXT("jerry can of petrol"));
		}
		else
		{
			ANHHUD::Toast(this, TEXT("Your bag don full"), 2);
		}
	}
	else if (What == TEXT("Buy it") || What == TEXT("Buy the business"))
	{
		if (Hustle->Pay(P.Price, FString::Printf(TEXT("bought %s"), *P.Name)))
		{
			Hustle->Owned.AddUnique(P.Id);
			if (IsHome(P) && Hustle->Home.IsNone())
			{
				Hustle->Home = P.Id;
			}
			Hustle->Save();
			Rebuild();
			ANHHUD::Toast(this, FString::Printf(TEXT("%s na your own now"), *P.Name), 1);
		}
		else
		{
			ANHHUD::Toast(this, FString::Printf(TEXT("Money no reach: %s"), *UNHHustleSubsystem::Naira(P.Price)), 2);
		}
	}
	else if (What == TEXT("Sell it"))
	{
		Hustle->Owned.Remove(P.Id);
		Hustle->Home = Hustle->Home == P.Id ? NAME_None : Hustle->Home;
		Hustle->Bankroll(P.Price * 8 / 10, FString::Printf(TEXT("sold %s"), *P.Name));
		Hustle->Save();
		Rebuild();
		ANHHUD::Toast(this, FString::Printf(TEXT("Sold. %s in the bank."), *UNHHustleSubsystem::Naira(P.Price * 8 / 10)), 1);
	}
	else if (What == TEXT("Rest till morning") && Me)
	{
		const float Day = FMath::FloorToFloat(Hustle->Minutes / 1440.f) * 1440.f;
		Hustle->Minutes = Hustle->Minutes < Day + 420.f ? Day + 420.f : Day + 1440.f + 420.f;
		Me->Health = 100.f;
		Hustle->Save();
		Mode = EMode::None;
		ANHHUD::Toast(this, TEXT("Seven o'clock. You don rest."), 1);
	}
	else if (What == TEXT("Make this home"))
	{
		Hustle->Home = P.Id;
		Hustle->Save();
	}
	else if (Inside == Shown && Me)
	{
		// something off the card
		const TArray<FDrink>* Card = Menus.Find(P.Kind);
		const FDrink* D = Card ? Card->FindByPredicate([&What](const FDrink& Have) { return Have.Name == What; }) : nullptr;
		if (D && (Owns(P) || Hustle->Pay(D->Price, FString::Printf(TEXT("%s, %s"), *D->Name, *P.Name))))
		{
			Me->Health = FMath::Min(100.f, Me->Health + D->Heal);
			ANHHUD::Toast(this, D->Heal > 0.f ? FString::Printf(TEXT("%s. Health %.0f"), *D->Name, Me->Health) : D->Name + TEXT(". The whole room is looking at you."), 1);
		}
		else if (D)
		{
			ANHHUD::Toast(this, TEXT("Money no reach"), 2);
		}
	}
}

int32 ANHEstate::KeptAt(FName Home) const
{
	const UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	int32 N = 0;
	for (const FNHGarageCar& Car : Hustle ? Hustle->Cars : TArray<FNHGarageCar>())
	{
		N += Car.Home == Home;
	}
	return N;
}

ANHVehicle* ANHEstate::Standing(int32 Serial) const
{
	for (ANHVehicle* Car : Garage)
	{
		if (Car && IsValid(Car) && Car->GarageSerial == Serial)
		{
			return Car;
		}
	}
	return nullptr;
}

ANHVehicle* ANHEstate::Make(FNHGarageCar& Record, const FTransform& Where)
{
	ANHVehicle* Car = GetWorld()->SpawnActorDeferred<ANHVehicle>(ANHVehicle::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Car)
	{
		return nullptr;
	}
	Car->VehicleType = Record.Type;
	Car->Paint = Record.Paint;
	UGameplayStatics::FinishSpawningActor(Car, Where);
	Car->bOwned = Car->bPlayerOwned = true;
	Car->Lock = ENHLock::Open;
	Car->Fuel = Record.Fuel;
	Car->GarageSerial = Record.Serial;
	if (Record.bWrecked)
	{
		Car->Health = 0.f;
	}
	Garage.Add(Car);
	return Car;
}

void ANHEstate::Park(const FPlace& P)
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	if (!Hustle || !P.bPlaced)
	{
		return;
	}
	Garage.RemoveAll([](const TObjectPtr<ANHVehicle>& Car) { return !Car || !IsValid(Car); });
	// nose to tail on the verge between the board and the road, either side of the board
	const FRotator Facing(0.f, P.Yaw + 90.f, 0.f);
	const FVector AlongRoad = Facing.Vector(), ToRoad = FRotator(0.f, P.Yaw, 0.f).Vector();
	int32 Bay = 0;
	for (FNHGarageCar& Record : Hustle->Cars)
	{
		if (Record.Home != P.Id)
		{
			continue;
		}
		const int32 Mine = Bay++;
		if (!Standing(Record.Serial))
		{
			Make(Record, FTransform(Facing, P.At + ToRoad * 380.f + AlongRoad * (500.f + 720.f * (Mine / 2)) * (Mine % 2 ? -1.f : 1.f) + FVector(0.f, 0.f, 160.f)));
		}
	}
}

bool ANHEstate::Keep(ANHVehicle* Car, FName Home)
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	Home = Home.IsNone() && Hustle ? Hustle->Home : Home;
	const FPlace* P = Places.FindByPredicate([Home](const FPlace& Have) { return Have.Id == Home; });
	if (!Hustle || !Car || Car->GarageSerial != 0 || !P || !Owns(*P) || KeptAt(Home) >= Slots(*P))
	{
		return false;
	}
	FNHGarageCar Record;
	Record.Serial = Hustle->NextCar++;
	Record.Type = Car->VehicleType;
	Record.Paint = Car->Paint;
	Record.Home = Home;
	Record.Fuel = Car->Fuel;
	Record.bWrecked = Car->IsWrecked();
	Hustle->Cars.Add(Record);
	Car->GarageSerial = Record.Serial;
	Car->bOwned = Car->bPlayerOwned = true;
	Car->bStolen = false;
	Garage.AddUnique(Car);
	Hustle->Save();
	return true;
}

bool ANHEstate::Kerb(FTransform& Out) const
{
	const UNHGameData* Data = UNHGameData::Get(this);
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	FNHRoadSeg Seg;
	FVector2D OnRoad;
	const FVector Here = Pawn ? (Inside != INDEX_NONE ? Outside : Pawn->GetActorLocation()) : FVector::ZeroVector;
	if (!Pawn || !Data || !Data->NearestRoad(FVector2D(Here), Seg, OnRoad) || FVector2D::Distance(OnRoad, FVector2D(Here)) > 30000.f)
	{
		return false;
	}
	const FNHRoadWay& Way = Data->RoadWays[Seg.Way];
	const FVector2D Along = (Data->RoadNodes[Way.Nodes[Seg.Index + 1]] - Data->RoadNodes[Way.Nodes[Seg.Index]]).GetSafeNormal();
	Out = FTransform(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), 0.f), FVector(OnRoad + FVector2D(-Along.Y, Along.X) * (Data->HalfWidth(Way) - 160.f), Here.Z + 90.f));
	return true;
}

bool ANHEstate::Deliver(int32 Serial)
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	FNHGarageCar* Record = Hustle ? Hustle->Cars.FindByPredicate([Serial](const FNHGarageCar& Have) { return Have.Serial == Serial; }) : nullptr;
	FTransform Where;
	if (!Record)
	{
		return false;
	}
	if (Record->bWrecked)
	{
		ANHHUD::Toast(this, TEXT("Mechanic: \"Oga, that one don spoil. Claim the insurance first.\""), 2);
		return false;
	}
	if (!Kerb(Where))
	{
		ANHHUD::Toast(this, TEXT("Mechanic: \"I no fit reach you there. Stand near road.\""), 2);
		return false;
	}
	ANHVehicle* Car = Standing(Serial);
	if (Car && Car->IsPlayerControlled())
	{
		ANHHUD::Toast(this, TEXT("You are driving it"), 0);
		return false;
	}
	if (!Hustle->Pay(DeliveryFee, TEXT("mechanic brought the car")))
	{
		ANHHUD::Toast(this, TEXT("Mechanic: \"Money for fuel first.\""), 2);
		return false;
	}
	if (Car)
	{
		Car->SetActorLocationAndRotation(Where.GetLocation(), Where.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	}
	else
	{
		Car = Make(*Record, Where);
	}
	if (Car)
	{
		if (ANHHUD* H = ANHHUD::Get(this))
		{
			H->SetPin(FVector2D(Where.GetLocation()), Car->DisplayName());
		}
		ANHHUD::Toast(this, FString::Printf(TEXT("Mechanic: \"Your %s dey the kerb.\""), *Car->DisplayName()), 1);
	}
	return Car != nullptr;
}

bool ANHEstate::Claim(int32 Serial)
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	FNHGarageCar* Record = Hustle ? Hustle->Cars.FindByPredicate([Serial](const FNHGarageCar& Have) { return Have.Serial == Serial; }) : nullptr;
	if (!Record || !Record->bWrecked)
	{
		return false;
	}
	// the premium: a fiftieth of what the car is worth
	const int64 Premium = FMath::Max<int64>(ANHVehicle::ValueOf(Record->Type) / 50, 20000);
	if (!Hustle->Pay(Premium, TEXT("insurance claim")))
	{
		ANHHUD::Toast(this, FString::Printf(TEXT("Shield Mutual: the premium is %s"), *UNHHustleSubsystem::Naira(Premium)), 2);
		return false;
	}
	Record->bWrecked = false;
	Record->Fuel = 1.f;
	if (ANHVehicle* Car = Standing(Serial))
	{
		Garage.Remove(Car);
		Car->Destroy(); // the wreck is towed; the replacement stands at home
	}
	Hustle->Save();
	ANHHUD::Toast(this, TEXT("Shield Mutual: claim paid. Your car is back at home."), 1);
	return true;
}

bool ANHEstate::SellCar(int32 Serial)
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const int32 Index = Hustle ? Hustle->Cars.IndexOfByPredicate([Serial](const FNHGarageCar& Have) { return Have.Serial == Serial; }) : INDEX_NONE;
	if (Index == INDEX_NONE)
	{
		return false;
	}
	const int64 Price = Hustle->Cars[Index].bWrecked ? 0 : ANHVehicle::ValueOf(Hustle->Cars[Index].Type) / 2;
	if (ANHVehicle* Car = Standing(Serial))
	{
		if (Car->IsPlayerControlled())
		{
			ANHHUD::Toast(this, TEXT("Get out of it first"), 0);
			return false;
		}
		Garage.Remove(Car);
		Car->Destroy();
	}
	Hustle->Cars.RemoveAt(Index);
	Hustle->Bankroll(Price, TEXT("sold a car"));
	Hustle->Save();
	ANHHUD::Toast(this, FString::Printf(TEXT("Sold. %s in the bank."), *UNHHustleSubsystem::Naira(Price)), 1);
	return true;
}

void ANHEstate::GarageCards(TArray<FNHGarageCard>& Out) const
{
	const UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const UNHGameData* Data = UNHGameData::Get(this);
	for (const FNHGarageCar& Record : Hustle ? Hustle->Cars : TArray<FNHGarageCar>())
	{
		const FNHVehicleSpec* Spec = Data ? Data->Vehicles.Find(Record.Type) : nullptr;
		const FPlace* Home = Places.FindByPredicate([&Record](const FPlace& Have) { return Have.Id == Record.Home; });
		Out.Add({ Record.Serial, Spec ? Spec->Name : Record.Type.ToString(), Home ? Home->Name : FString(), Record.bWrecked, Standing(Record.Serial) != nullptr, Record.Fuel, ANHVehicle::ValueOf(Record.Type) });
	}
}

bool ANHEstate::GoTo(FName Id)
{
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	FPlace* P = Places.FindByPredicate([Id](const FPlace& Have) { return Have.Id == Id; });
	if (!PC || !PC->GetPawn() || !P || (!P->bPlaced && !Place(*P)))
	{
		return false;
	}
	if (Inside != INDEX_NONE)
	{
		LeaveRoom(PC->GetPawn());
	}
	if (Cast<ANHVehicle>(PC->GetPawn()))
	{
		PC->LeaveVehicle(true);
	}
	PC->GetPawn()->SetActorLocation(P->At + FRotator(0.f, P->Yaw, 0.f).Vector() * 200.f + FVector(0.f, 0.f, 110.f), false, nullptr, ETeleportType::TeleportPhysics);
	PC->SetControlRotation(FRotator(0.f, P->Yaw + 180.f, 0.f));
	Look = 0.f;
	return true;
}

bool ANHEstate::PlayAs(FName Id)
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const FPerson* Who = People.FindByPredicate([Id](const FPerson& Have) { return Have.Id == Id; });
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	ANHCharacter* Me = PC ? Cast<ANHCharacter>(PC->GetPawn()) : nullptr;
	if (!Hustle || !Who || !GoTo(Who->Home))
	{
		UE_LOG(LogNHGame, Warning, TEXT("NAIJA HUSTLE: estate: cannot play as %s (no such person, or nowhere to stand their home)"), *Id.ToString());
		return false;
	}
	Me = Me ? Me : Cast<ANHCharacter>(PC->GetPawn());
	if (Hustle->Persona != Id)
	{
		// a new life: their money and their house, once. Coming back to it later keeps what has been spent since.
		Hustle->Persona = Id;
		Hustle->Bank = Who->Bank;
		Hustle->Cash = Pocket;
		Hustle->Owned.AddUnique(Who->Home);
		Hustle->Home = Who->Home;
		Hustle->Save();
	}
	if (Me)
	{
		Me->WearSkin(Who->Skin, true);
		Me->Health = 100.f;
	}
	const FPlace* Home = Places.FindByPredicate([Who](const FPlace& Have) { return Have.Id == Who->Home; });
	if (Home)
	{
		if (Home->Built.IsValid())
		{
			Home->Built->Destroy();
		}
		if (KeptAt(Home->Id) == 0)
		{
			// their cars, the first time: into the garage of the house
			static const FLinearColor Paints[] = { FLinearColor(0.01f, 0.01f, 0.012f), FLinearColor(0.8f, 0.8f, 0.78f), FLinearColor(0.35f, 0.02f, 0.03f), FLinearColor(0.02f, 0.05f, 0.2f) };
			for (int32 I = 0; I < Who->Cars.Num() && I < Slots(*Home); ++I)
			{
				FNHGarageCar Record;
				Record.Serial = Hustle->NextCar++;
				Record.Type = Who->Cars[I];
				Record.Paint = Paints[I % UE_ARRAY_COUNT(Paints)];
				Record.Home = Home->Id;
				Hustle->Cars.Add(Record);
			}
			Hustle->Save();
		}
		Park(*Home);
	}
	ANHHUD::Toast(this, FString::Printf(TEXT("%s. %s in the bank."), *Who->Name, *UNHHustleSubsystem::Naira(Hustle->Bank)), 1);
	UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: estate: playing as %s at %s, bank %lld, %d cars outside"), *Who->Name, Home ? *Home->Name : TEXT("?"), Hustle->Bank, Garage.Num());
	return true;
}

void ANHEstate::Cards(TArray<FNHPlaceCard>& Out) const
{
	for (const FPlace& P : Places)
	{
		const FPerson* Who = People.FindByPredicate([&P](const FPerson& Have) { return Have.Home == P.Id; });
		Out.Add({ P.Id, P.Kind, P.Name, P.Area, P.About, P.Price, P.Fee, Owns(P), P.bPlaced ? FVector2D(P.At) : P.Want, Who ? Who->Name : FString(), IsHome(P) });
	}
}

FString ANHEstate::Describe() const
{
	const UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	int32 Placed = 0, Built = 0;
	for (const FPlace& P : Places)
	{
		Placed += P.bPlaced;
		Built += P.Built.IsValid();
	}
	return FString::Printf(TEXT("%d places (%d stood, %d built), near %s, inside %s, owned %d, home %s, pocket %d, bank %lld"), Places.Num(), Placed, Built,
		Places.IsValidIndex(Near) ? *Places[Near].Name : TEXT("none"), Places.IsValidIndex(Inside) ? *Places[Inside].Name : TEXT("nowhere"),
		Hustle ? Hustle->Owned.Num() : 0, Hustle ? *Hustle->Home.ToString() : TEXT("?"), Hustle ? Hustle->Cash : 0, Hustle ? Hustle->Bank : 0);
}
