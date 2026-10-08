#include "World/NHCables.h"

#include "Materials/MaterialInterface.h"
#include "World/NHShapes.h"

namespace NHCable
{
	const FName GeneratedTag(TEXT("NHGenerated"));
}

void UNHCableComponent::OnRegister()
{
	Super::OnRegister(); // the cable starts as a straight line between its ends
	Settled = 0.f;
	SetComponentTickEnabled(true);
}

void UNHCableComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Settled += DeltaTime;
	if (Settled >= SettleSeconds)
	{
		SetComponentTickEnabled(false); // it keeps the shape it has
	}
}

ANHCables::ANHCables()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void ANHCables::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void ANHCables::Rebuild()
{
	TArray<UNHCableComponent*> Old;
	GetComponents<UNHCableComponent>(Old);
	for (UNHCableComponent* C : Old)
	{
		if (C && C->ComponentHasTag(NHCable::GeneratedTag))
		{
			C->DestroyComponent();
		}
	}

	UMaterialInterface* Mat = NHShapes::PrimMaterial();
	const float Surface[8] = { Color.R, Color.G, Color.B, 0.6f, 0.f, 0.f, 0.5f, 0.f }; // the blockout custom data layout
	for (const FNHCableSpan& S : Spans)
	{
		const float Length = static_cast<float>(FVector::Dist(S.Start, S.End));
		if (Length < 50.f)
		{
			continue;
		}
		UNHCableComponent* C = NewObject<UNHCableComponent>(this, NAME_None, RF_Transactional);
		C->CreationMethod = EComponentCreationMethod::Instance;
		C->ComponentTags.Add(NHCable::GeneratedTag);
		C->SetupAttachment(GetRootComponent());
		C->SetWorldLocation(S.Start);
		// with nothing set to attach to, the end is measured from the actor's root
		C->EndLocation = GetRootComponent()->GetComponentTransform().InverseTransformPosition(S.End);
		C->CableLength = Length * (1.f + S.Slack);
		C->CableWidth = S.Width;
		C->NumSegments = 8;
		C->NumSides = 3;
		C->SolverIterations = 4;
		C->bEnableStiffness = false;
		C->bEnableCollision = false;
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(false); // hundreds of hair-thin shadows cost more than they show
		C->SetCullDistance(DrawDistance);
		if (Mat)
		{
			C->SetMaterial(0, Mat);
		}
		for (int32 I = 0; I < 8; ++I)
		{
			C->SetCustomPrimitiveDataFloat(I, Surface[I]);
		}
		C->RegisterComponent();
		AddInstanceComponent(C);
	}
}
