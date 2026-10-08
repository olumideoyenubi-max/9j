#include "World/NHBlockoutActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "World/NHShapes.h"

namespace NHBlockout
{
	constexpr int32 NumCustomData = 8;
	const FName ISMTag(TEXT("NHBlockoutISM"));
	const TCHAR* KeyPrefix = TEXT("NHKey_");

	int32 Key(ENHShape Shape, bool bSolid, ENHSurfaceType Type)
	{
		return static_cast<int32>(Shape) | (bSolid ? 0x10 : 0) | (static_cast<int32>(Type) << 8);
	}
}

ANHBlockoutActor::ANHBlockoutActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/NaijaHustle/Environment/Materials/M_NHBlockout.M_NHBlockout")));
}

UMaterialInterface* ANHBlockoutActor::MaterialFor(ENHSurfaceType Type)
{
	if (const TObjectPtr<UMaterialInterface>* Found = TypeMaterials.Find(Type))
	{
		return *Found;
	}
	const FString Name = StaticEnum<ENHSurfaceType>()->GetNameStringByValue(static_cast<int64>(Type));
	const FString Path = FString::Printf(TEXT("/Game/NaijaHustle/Environment/Materials/MI_NHSurface_%s.MI_NHSurface_%s"), *Name, *Name);
	UMaterialInterface* Mat = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(Path)).LoadSynchronous();
	if (!Mat)
	{
		Mat = Material.LoadSynchronous(); // before nh_blockout_materials.py has made the instances
	}
	TypeMaterials.Add(Type, Mat);
	return Mat;
}

UInstancedStaticMeshComponent* ANHBlockoutActor::ISMFor(ENHShape Shape, bool bSolid, ENHSurfaceType Type)
{
	// spheres and cones are always detail: their collision is a poor fit for what they stand in for
	bSolid = bSolid && (Shape == ENHShape::Box || Shape == ENHShape::Cylinder);
	const int32 Key = NHBlockout::Key(Shape, bSolid, Type);
	if (const TObjectPtr<UInstancedStaticMeshComponent>* Found = ISMs.Find(Key))
	{
		return *Found;
	}

	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transactional);
	ISM->CreationMethod = EComponentCreationMethod::Instance;
	ISM->ComponentTags.Add(NHBlockout::ISMTag);
	ISM->ComponentTags.Add(FName(*FString::Printf(TEXT("%s%d"), NHBlockout::KeyPrefix, Key)));
	ISM->SetupAttachment(GetRootComponent());
	ISM->SetStaticMesh(NHShapes::Mesh(Shape));
	ISM->NumCustomDataFloats = NHBlockout::NumCustomData;
	ISM->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	ISM->SetCollisionProfileName(bSolid ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	ISM->SetCanEverAffectNavigation(bSolid);
	if (UMaterialInterface* Mat = MaterialFor(Type))
	{
		ISM->SetMaterial(0, Mat);
	}
	ISM->RegisterComponent();
	AddInstanceComponent(ISM);
	ISMs.Add(Key, ISM);
	return ISM;
}

void ANHBlockoutActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void ANHBlockoutActor::Rebuild()
{
	// pick up the components a previous build left (they are saved with the actor), emptied and ready to refill
	ISMs.Reset();
	TypeMaterials.Reset();
	TArray<UInstancedStaticMeshComponent*> Old;
	GetComponents<UInstancedStaticMeshComponent>(Old);
	for (UInstancedStaticMeshComponent* ISM : Old)
	{
		if (!ISM || !ISM->ComponentHasTag(NHBlockout::ISMTag))
		{
			continue;
		}
		int32 Key = INDEX_NONE;
		for (const FName& Tag : ISM->ComponentTags)
		{
			const FString T = Tag.ToString();
			if (T.StartsWith(NHBlockout::KeyPrefix))
			{
				Key = FCString::Atoi(*T.RightChop(FCString::Strlen(NHBlockout::KeyPrefix)));
			}
		}
		if (Key == INDEX_NONE || ISMs.Contains(Key))
		{
			ISM->DestroyComponent();
			continue;
		}
		ISM->ClearInstances();
		ISM->SetNumCustomDataFloats(NHBlockout::NumCustomData);
		if (UMaterialInterface* Mat = MaterialFor(static_cast<ENHSurfaceType>(Key >> 8)))
		{
			ISM->SetMaterial(0, Mat);
		}
		ISMs.Add(Key, ISM);
	}

	Build();

	for (auto It = ISMs.CreateIterator(); It; ++It)
	{
		UInstancedStaticMeshComponent* ISM = It.Value();
		if (ISM->GetInstanceCount() == 0)
		{
			ISM->DestroyComponent(); // a type this build no longer uses
			It.RemoveCurrent();
		}
		else
		{
			ISM->MarkRenderStateDirty();
		}
	}
}

void ANHBlockoutActor::AddShapeTransform(ENHShape Shape, const FTransform& Transform, const FNHSurface& Surface, bool bSolid, bool bWorldSpace)
{
	UInstancedStaticMeshComponent* ISM = ISMFor(Shape, bSolid, Surface.Type);
	const int32 Index = ISM->AddInstance(Transform, bWorldSpace);
	const float Data[NHBlockout::NumCustomData] = {
		Surface.Color.R, Surface.Color.G, Surface.Color.B, Surface.Roughness, Surface.Metallic, Surface.Glow, Surface.Wet, Surface.GlowWarm };
	ISM->SetCustomData(Index, MakeArrayView(Data, NHBlockout::NumCustomData), false);
}

void ANHBlockoutActor::AddShape(ENHShape Shape, const FVector& Center, const FVector& Size, const FNHSurface& Surface, bool bSolid, const FRotator& Rotation, bool bWorldSpace)
{
	AddShapeTransform(Shape, FTransform(Rotation, Center, Size / 100.f), Surface, bSolid, bWorldSpace);
}

void ANHBlockoutActor::AddBox(const FVector& Center, const FVector& Size, const FNHSurface& Surface, bool bSolid, const FRotator& Rotation, bool bWorldSpace)
{
	AddShape(ENHShape::Box, Center, Size, Surface, bSolid, Rotation, bWorldSpace);
}

float ANHBlockoutActor::Hash01(int32 A, int32 B, int32 C)
{
	uint32 H = static_cast<uint32>(A) * 374761393u + static_cast<uint32>(B) * 668265263u + static_cast<uint32>(C) * 2246822519u;
	H = (H ^ (H >> 13)) * 1274126177u;
	H ^= H >> 16;
	return static_cast<float>(H & 0xFFFFFF) / 16777216.f;
}
