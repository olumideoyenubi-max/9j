#include "World/NHBlockoutActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace NHBlockout
{
	constexpr int32 NumCustomData = 8;
}

ANHBlockoutActor::ANHBlockoutActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));

	BoxSolid = MakeISM(TEXT("BoxSolid"), Cube.Object, true);
	BoxDetail = MakeISM(TEXT("BoxDetail"), Cube.Object, false);
	CylinderSolid = MakeISM(TEXT("CylinderSolid"), Cylinder.Object, true);
	CylinderDetail = MakeISM(TEXT("CylinderDetail"), Cylinder.Object, false);
	SphereDetail = MakeISM(TEXT("SphereDetail"), Sphere.Object, false);
	ConeDetail = MakeISM(TEXT("ConeDetail"), Cone.Object, false);

	Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/NaijaHustle/Environment/Materials/M_NHBlockout.M_NHBlockout")));
}

UInstancedStaticMeshComponent* ANHBlockoutActor::MakeISM(const TCHAR* Name, UStaticMesh* Mesh, bool bSolid)
{
	UInstancedStaticMeshComponent* ISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
	ISM->SetupAttachment(Root);
	ISM->SetStaticMesh(Mesh);
	ISM->NumCustomDataFloats = NHBlockout::NumCustomData;
	ISM->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	ISM->SetCollisionProfileName(bSolid ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	ISM->SetCanEverAffectNavigation(bSolid);
	return ISM;
}

TArray<UInstancedStaticMeshComponent*> ANHBlockoutActor::AllISMs() const
{
	return { BoxSolid, BoxDetail, CylinderSolid, CylinderDetail, SphereDetail, ConeDetail };
}

UInstancedStaticMeshComponent* ANHBlockoutActor::ISMFor(ENHShape Shape, bool bSolid) const
{
	switch (Shape)
	{
	case ENHShape::Cylinder: return bSolid ? CylinderSolid : CylinderDetail;
	case ENHShape::Sphere:   return SphereDetail;
	case ENHShape::Cone:     return ConeDetail;
	default:                 return bSolid ? BoxSolid : BoxDetail;
	}
}

void ANHBlockoutActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void ANHBlockoutActor::Rebuild()
{
	UMaterialInterface* Mat = Material.LoadSynchronous();
	for (UInstancedStaticMeshComponent* ISM : AllISMs())
	{
		ISM->ClearInstances();
		ISM->SetNumCustomDataFloats(NHBlockout::NumCustomData);
		if (Mat)
		{
			ISM->SetMaterial(0, Mat);
		}
	}
	Build();
	for (UInstancedStaticMeshComponent* ISM : AllISMs())
	{
		ISM->MarkRenderStateDirty();
	}
}

void ANHBlockoutActor::AddShapeTransform(ENHShape Shape, const FTransform& Transform, const FNHSurface& Surface, bool bSolid, bool bWorldSpace)
{
	UInstancedStaticMeshComponent* ISM = ISMFor(Shape, bSolid);
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
