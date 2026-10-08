#include "Core/NHGameData.h"

#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NaijaHustleGame.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace NHJson
{
	TSharedPtr<FJsonObject> Load(const FString& Path)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			return nullptr;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		return FJsonSerializer::Deserialize(Reader, Root) ? Root : nullptr;
	}

	double Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double Default = 0.0)
	{
		double V = Default;
		return (O.IsValid() && O->TryGetNumberField(Key, V)) ? V : Default;
	}

	FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		FString V;
		return (O.IsValid() && O->TryGetStringField(Key, V)) ? V : FString();
	}

	TArray<FString> Strings(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		TArray<FString> Out;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Key, Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				Out.Add(V->AsString());
			}
		}
		return Out;
	}

	FVector2D XY(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TSharedPtr<FJsonObject>* P = nullptr;
		if (O.IsValid() && O->TryGetObjectField(Key, P))
		{
			return FVector2D(Num(*P, TEXT("x")), Num(*P, TEXT("y")));
		}
		return FVector2D::ZeroVector;
	}

	FVector2D Pair(const TSharedPtr<FJsonValue>& V)
	{
		const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
		return A.Num() >= 2 ? FVector2D(A[0]->AsNumber(), A[1]->AsNumber()) : FVector2D::ZeroVector;
	}

	FLinearColor Color(const FString& Hex)
	{
		FString H = Hex;
		H.RemoveFromStart(TEXT("#"));
		if (H.Len() == 3)
		{
			H = FString::Printf(TEXT("%c%c%c%c%c%c"), H[0], H[0], H[1], H[1], H[2], H[2]);
		}
		return FLinearColor(FColor::FromHex(H)); // sRGB -> linear
	}

	FNHVehicleSpec VehicleSpec(const TSharedPtr<FJsonObject>& J)
	{
		FNHVehicleSpec S;
		S.Name = Str(J, TEXT("name"));
		S.Length = static_cast<float>(Num(J, TEXT("len"), S.Length));
		S.Width = static_cast<float>(Num(J, TEXT("wid"), S.Width));
		S.MaxSpeed = static_cast<float>(Num(J, TEXT("max"), S.MaxSpeed));
		S.Accel = static_cast<float>(Num(J, TEXT("acc"), S.Accel));
		S.Turn = static_cast<float>(Num(J, TEXT("turn"), S.Turn));
		S.Hp = static_cast<float>(Num(J, TEXT("hp"), S.Hp));
		bool bBike = false;
		J->TryGetBoolField(TEXT("bike"), bBike);
		S.bBike = bBike;
		for (const FString& C : Strings(J, TEXT("colors")))
		{
			S.Colors.Add(Color(C));
		}
		S.Body = FName(*Str(J, TEXT("body")));
		return S;
	}

	FNHRoute Route(const TSharedPtr<FJsonObject>& O)
	{
		FNHRoute R;
		R.Id = FName(Str(O, TEXT("id")));
		R.Name = Str(O, TEXT("name"));
		R.Blurb = Str(O, TEXT("blurb"));
		for (const FString& S : Strings(O, TEXT("stops")))
		{
			R.Stops.Add(FName(S));
		}
		const TArray<TSharedPtr<FJsonValue>>* Fare = nullptr;
		if (O->TryGetArrayField(TEXT("fare"), Fare) && Fare->Num() >= 2)
		{
			R.FareLo = static_cast<int32>((*Fare)[0]->AsNumber());
			R.FareHi = static_cast<int32>((*Fare)[1]->AsNumber());
		}
		R.Busy = static_cast<int32>(Num(O, TEXT("busy"), 5));
		bool bLinear = false;
		O->TryGetBoolField(TEXT("linear"), bLinear);
		R.bLinear = bLinear;
		const TSharedPtr<FJsonObject>* Labels = nullptr;
		if (O->TryGetObjectField(TEXT("label"), Labels))
		{
			for (const auto& KV : (*Labels)->Values)
			{
				R.Labels.Add(FName(KV.Key), KV.Value->AsString());
			}
		}
		return R;
	}
}

FString UNHGameData::DataDir()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("NaijaHustleGame"));
	return Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Data")) : FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("NaijaHustleGame"), TEXT("Data"));
}

UNHGameData* UNHGameData::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UNHGameData>() : nullptr;
}

void UNHGameData::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const FString Dir = DataDir();
	const bool bCity = LoadCity(FPaths::Combine(Dir, TEXT("lagos_city.json")));
	const bool bRules = LoadRules(FPaths::Combine(Dir, TEXT("naija_rules.json")));
	bLoaded = bCity && bRules;
	if (bLoaded)
	{
		LoadVehicleExtras(FPaths::Combine(Dir, TEXT("unreal_vehicles.json")), FPaths::Combine(Dir, TEXT("vehicle_meshes.json")));
	}
	if (!bLoaded)
	{
		UE_LOG(LogNHGame, Error, TEXT("NAIJA HUSTLE: could not read the game data in %s (city %d, rules %d). Re-export with web/tools/export-unreal.js."), *Dir, bCity, bRules);
	}
	else
	{
		UE_LOG(LogNHGame, Log, TEXT("NAIJA HUSTLE: data loaded from %s: %d stops, %d routes, %d vehicle types (%d with a real model), %d extra parked vehicles"), *Dir, Stops.Num(), Routes.Num(), Vehicles.Num(), VehicleMeshes.Num(), Parked.Num());
	}
}

bool UNHGameData::LoadCity(const FString& Path)
{
	using namespace NHJson;
	const TSharedPtr<FJsonObject> Root = Load(Path);
	if (!Root.IsValid())
	{
		return false;
	}
	Cols = static_cast<int32>(Num(Root, TEXT("cols"), 96));
	Rows = static_cast<int32>(Num(Root, TEXT("rows"), 64));
	CellSize = static_cast<float>(Num(Root, TEXT("cellSize"), 400));
	Tiles = Strings(Root, TEXT("tiles"));
	const TSharedPtr<FJsonObject>* Districts = nullptr;
	if (Root->TryGetObjectField(TEXT("districts"), Districts))
	{
		DistrictNames = Strings(*Districts, TEXT("names"));
		DistrictGrid = Strings(*Districts, TEXT("grid"));
	}
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Root->TryGetArrayField(TEXT("busStops"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FNHBusStop S;
			S.Id = FName(Str(O, TEXT("id")));
			S.Name = Str(O, TEXT("name"));
			S.Agbero = static_cast<int32>(Num(O, TEXT("agbero")));
			const TArray<TSharedPtr<FJsonValue>>* K = nullptr;
			if (O->TryGetArrayField(TEXT("kerb"), K) && K->Num() >= 2) S.Kerb = FVector2D((*K)[0]->AsNumber(), (*K)[1]->AsNumber());
			if (O->TryGetArrayField(TEXT("wait"), K) && K->Num() >= 2) S.Wait = FVector2D((*K)[0]->AsNumber(), (*K)[1]->AsNumber());
			Stops.Add(S.Id, S);
		}
	}
	if (Root->TryGetArrayField(TEXT("parkBays"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FNHParkBay B;
			B.Number = static_cast<int32>(Num(O, TEXT("n")));
			B.Pos = FVector2D(Num(O, TEXT("x")), Num(O, TEXT("y")));
			B.Yaw = static_cast<float>(Num(O, TEXT("yaw")));
			ParkBays.Add(B);
		}
	}
	return Tiles.Num() == Rows;
}

bool UNHGameData::LoadRules(const FString& Path)
{
	using namespace NHJson;
	const TSharedPtr<FJsonObject> Root = Load(Path);
	if (!Root.IsValid())
	{
		return false;
	}
	const TSharedPtr<FJsonObject>* O = nullptr;
	if (Root->TryGetObjectField(TEXT("start"), O))
	{
		StartCash = static_cast<int32>(Num(*O, TEXT("cash"), 5000));
		StartMinutes = static_cast<float>(Num(*O, TEXT("minutes"), 480));
	}
	ClockMinutesPerSecond = static_cast<float>(Num(Root, TEXT("clockMinutesPerSecond"), 2));
	if (Root->TryGetObjectField(TEXT("wanted"), O))
	{
		SecondsPerStar = static_cast<float>(Num(*O, TEXT("perStar"), 18));
		WantedUnits = Strings(*O, TEXT("units"));
	}
	if (Root->TryGetObjectField(TEXT("conductor"), O))
	{
		FNHConductorRules& C = Conductor;
		const TSharedPtr<FJsonObject>& J = *O;
		C.Capacity = static_cast<int32>(Num(J, TEXT("capacity"), C.Capacity));
		C.OwnerCut = static_cast<float>(Num(J, TEXT("ownerCut"), C.OwnerCut));
		C.DamageCost = static_cast<float>(Num(J, TEXT("damageCost"), C.DamageCost));
		C.FirstCut = static_cast<float>(Num(J, TEXT("firstCut"), C.FirstCut));
		C.FirstTicket = static_cast<int32>(Num(J, TEXT("firstTicket"), C.FirstTicket));
		C.FullBusBonus = static_cast<int32>(Num(J, TEXT("fullBusBonus"), C.FullBusBonus));
		C.MissedStopFine = static_cast<int32>(Num(J, TEXT("missedStopFine"), C.MissedStopFine));
		C.ArriveRadius = static_cast<float>(Num(J, TEXT("arriveRadius"), C.ArriveRadius));
		C.SlowSpeed = static_cast<float>(Num(J, TEXT("slowSpeed"), C.SlowSpeed));
		C.NearRadius = static_cast<float>(Num(J, TEXT("nearRadius"), C.NearRadius));
		C.PassRadius = static_cast<float>(Num(J, TEXT("passRadius"), C.PassRadius));
		C.DepartSpeed = static_cast<float>(Num(J, TEXT("departSpeed"), C.DepartSpeed));
		C.DepartRadius = static_cast<float>(Num(J, TEXT("departRadius"), C.DepartRadius));
		C.RoughAccel = static_cast<float>(Num(J, TEXT("roughAccel"), C.RoughAccel));
		C.RoughLateral = static_cast<float>(Num(J, TEXT("roughLateral"), C.RoughLateral));
		C.SmoothTipAfter = static_cast<float>(Num(J, TEXT("smoothTipAfter"), C.SmoothTipAfter));
		C.Refill = static_cast<float>(Num(J, TEXT("refill"), C.Refill));
	}
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Root->TryGetArrayField(TEXT("routes"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			Routes.Add(Route(V->AsObject()));
		}
	}
	if (Root->TryGetObjectField(TEXT("firstRoute"), O))
	{
		FirstRoute = Route(*O);
	}
	PaxNames = Strings(Root, TEXT("paxNames"));
	Calls = Strings(Root, TEXT("calls"));
	if (Root->TryGetObjectField(TEXT("babaLines"), O))
	{
		for (const auto& KV : (*O)->Values)
		{
			TArray<FString> Lines;
			for (const TSharedPtr<FJsonValue>& L : KV.Value->AsArray())
			{
				Lines.Add(L->AsString());
			}
			Baba.Add(FString(*KV.Key), Lines); // JSON keys are not FString in UE 5.8
		}
	}
	if (Root->TryGetArrayField(TEXT("outfits"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			OutfitRespect.Add(FName(Str(V->AsObject(), TEXT("id"))), static_cast<int32>(Num(V->AsObject(), TEXT("respect"))));
		}
	}
	if (Root->TryGetObjectField(TEXT("vehicles"), O))
	{
		for (const auto& KV : (*O)->Values)
		{
			const TSharedPtr<FJsonObject> J = KV.Value->AsObject();
			Vehicles.Add(FName(*KV.Key), VehicleSpec(KV.Value->AsObject()));
		}
	}
	if (Root->TryGetObjectField(TEXT("places"), O))
	{
		Park = XY(*O, TEXT("park"));
		Home = XY(*O, TEXT("home"));
	}
	const TSharedPtr<FJsonObject>* Missions = nullptr;
	const TSharedPtr<FJsonObject>* First = nullptr;
	if (Root->TryGetObjectField(TEXT("missions"), Missions) && (*Missions)->TryGetObjectField(TEXT("lag_01"), First))
	{
		FirstDayTitle = Str(*First, TEXT("title"));
		FirstDayCred = static_cast<int32>(Num(*First, TEXT("cred"), 20));
		if ((*First)->TryGetArrayField(TEXT("objectives"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				FirstDayObjectives.Add(Str(V->AsObject(), TEXT("text")));
				FirstDaySubs.Add(Str(V->AsObject(), TEXT("sub")));
				const double Clock = Num(V->AsObject(), TEXT("clock"));
				if (Clock > 0)
				{
					FirstDayClock = static_cast<float>(Clock);
				}
			}
		}
	}
	DefaultSpec.Name = TEXT("Car");
	return Routes.Num() > 0 && Vehicles.Num() > 0;
}

void UNHGameData::LoadVehicleExtras(const FString& TypesPath, const FString& MeshesPath)
{
	using namespace NHJson;
	const TSharedPtr<FJsonObject>* O = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (const TSharedPtr<FJsonObject> Root = Load(TypesPath))
	{
		if (Root->TryGetObjectField(TEXT("types"), O))
		{
			for (const auto& KV : (*O)->Values)
			{
				Vehicles.Add(FName(*KV.Key), VehicleSpec(KV.Value->AsObject()));
			}
		}
		if (Root->TryGetArrayField(TEXT("parked"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const TSharedPtr<FJsonObject> J = V->AsObject();
				FNHParkedVehicle P;
				P.Type = FName(*Str(J, TEXT("type")));
				P.Pos = FVector2D(Num(J, TEXT("x")), Num(J, TEXT("y")));
				P.Yaw = static_cast<float>(Num(J, TEXT("yaw")));
				P.Color = Color(Str(J, TEXT("color")));
				if (Vehicles.Contains(P.Type))
				{
					Parked.Add(P);
				}
				else
				{
					UE_LOG(LogNHGame, Warning, TEXT("unreal_vehicles.json: parked vehicle of unknown type '%s'"), *P.Type.ToString());
				}
			}
		}
	}
	// written by Scripts/assign_vehicle_meshes.py; not there until real models are assigned
	if (const TSharedPtr<FJsonObject> Root = Load(MeshesPath))
	{
		if (Root->TryGetObjectField(TEXT("meshes"), O))
		{
			for (const auto& KV : (*O)->Values)
			{
				const TSharedPtr<FJsonObject> J = KV.Value->AsObject();
				FNHVehicleMesh M;
				M.Meshes = Strings(J, TEXT("meshes"));
				M.Yaw = static_cast<float>(Num(J, TEXT("yaw")));
				M.Scale = static_cast<float>(Num(J, TEXT("scale"), 1.0));
				M.Height = static_cast<float>(Num(J, TEXT("height"), 150.0));
				const TArray<TSharedPtr<FJsonValue>>* Off = nullptr;
				if (J->TryGetArrayField(TEXT("offset"), Off) && Off->Num() >= 3)
				{
					M.Offset = FVector((*Off)[0]->AsNumber(), (*Off)[1]->AsNumber(), (*Off)[2]->AsNumber());
				}
				if (M.Meshes.Num() && Vehicles.Contains(FName(*KV.Key)))
				{
					VehicleMeshes.Add(FName(*KV.Key), M);
				}
			}
		}
	}
}

TArray<FString> UNHGameData::BabaLines(const FString& Key) const
{
	const TArray<FString>* L = Baba.Find(Key);
	return L ? *L : TArray<FString>();
}

TCHAR UNHGameData::TileAt(const FVector& World) const
{
	const int32 C = FMath::FloorToInt(World.X / CellSize), R = FMath::FloorToInt(World.Y / CellSize);
	if (C < 0 || R < 0 || R >= Tiles.Num() || C >= Tiles[R].Len())
	{
		return TEXT('#');
	}
	return Tiles[R][C];
}

FString UNHGameData::DistrictAt(const FVector& World) const
{
	const int32 C = FMath::FloorToInt(World.X / CellSize), R = FMath::FloorToInt(World.Y / CellSize);
	if (C < 0 || R < 0 || R >= DistrictGrid.Num() || C >= DistrictGrid[R].Len())
	{
		return FString();
	}
	const int32 I = DistrictGrid[R][C] - TEXT('a');
	return DistrictNames.IsValidIndex(I) ? DistrictNames[I] : FString();
}

const FNHVehicleSpec& UNHGameData::Spec(FName Type) const
{
	const FNHVehicleSpec* S = Vehicles.Find(Type);
	return S ? *S : DefaultSpec;
}

const FNHRoute* UNHGameData::FindRoute(FName Id) const
{
	if (FirstRoute.Id == Id)
	{
		return &FirstRoute;
	}
	return Routes.FindByPredicate([Id](const FNHRoute& R) { return R.Id == Id; });
}
