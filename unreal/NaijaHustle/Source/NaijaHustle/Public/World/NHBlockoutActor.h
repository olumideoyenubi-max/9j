#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NHBlockoutActor.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;

/** Blockout primitive shapes, all from /Engine/BasicShapes (100 cm, pivot at the centre). */
UENUM(BlueprintType)
enum class ENHShape : uint8
{
	Box,
	Cylinder,
	Sphere,
	Cone
};

/**
 * How one blockout piece looks. Written into the instance's custom data, which M_NHBlockout reads:
 * [0..2] colour (linear), [3] roughness, [4] metallic, [5] night glow (nits, scaled by the weather
 * collection's NightLights), [6] how much it gets wet in the rain, [7] glow tint (0 = own colour, 1 = warm lamp light).
 */
USTRUCT(BlueprintType)
struct FNHSurface
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface") FLinearColor Color = FLinearColor(0.5f, 0.5f, 0.5f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface") float Roughness = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface") float Metallic = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface") float Glow = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface") float Wet = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface") float GlowWarm = 0.f;

	FNHSurface() = default;
	FNHSurface(const FLinearColor& InColor, float InRoughness, float InWet = 0.f, float InMetallic = 0.f, float InGlow = 0.f, float InGlowWarm = 0.f)
		: Color(InColor), Roughness(InRoughness), Metallic(InMetallic), Glow(InGlow), Wet(InWet), GlowWarm(InGlowWarm) {}

};

/**
 * Base for the step-2 blockout: everything is drawn with instanced engine shapes (box, cylinder, sphere,
 * cone) and one material, so a whole building is a handful of draw calls and the layout can be rebuilt
 * from data at any time. "Solid" pieces collide; "detail" pieces (bars, frames, rails, markings) don't.
 * Subclasses override Build() and call AddBox / AddShape. Rebuild() runs from the construction script and
 * from the editor (Details panel button) after the build script fills in the data.
 */
UCLASS(Abstract)
class NAIJAHUSTLE_API ANHBlockoutActor : public AActor
{
	GENERATED_BODY()

public:
	ANHBlockoutActor();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Clears every instance and builds again from this actor's data. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Blockout")
	void Rebuild();

	/** M_NHBlockout, made by Scripts/nh_blockout_materials.py. Falls back to the engine default material. */
	UPROPERTY(EditAnywhere, Category = "Blockout")
	TSoftObjectPtr<UMaterialInterface> Material;

protected:
	virtual void Build() {}

	/** Box by centre and full size in cm, in this actor's local space (or world space). */
	void AddBox(const FVector& Center, const FVector& Size, const FNHSurface& Surface, bool bSolid = true, const FRotator& Rotation = FRotator::ZeroRotator, bool bWorldSpace = false);
	/** Any shape fitted to a box of Size (cm) at Center; for cylinders and cones, Z is the height. */
	void AddShape(ENHShape Shape, const FVector& Center, const FVector& Size, const FNHSurface& Surface, bool bSolid, const FRotator& Rotation = FRotator::ZeroRotator, bool bWorldSpace = false);
	/** A shape with a full transform (scale in BasicShapes units: 1 = 100 cm). */
	void AddShapeTransform(ENHShape Shape, const FTransform& Transform, const FNHSurface& Surface, bool bSolid, bool bWorldSpace);

	/** Small deterministic random numbers in [0, 1), so rebuilding gives the same result */
	static float Hash01(int32 A, int32 B = 0, int32 C = 0);

	UPROPERTY(VisibleAnywhere, Category = "Blockout") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Blockout") TObjectPtr<UInstancedStaticMeshComponent> BoxSolid;
	UPROPERTY(VisibleAnywhere, Category = "Blockout") TObjectPtr<UInstancedStaticMeshComponent> BoxDetail;
	UPROPERTY(VisibleAnywhere, Category = "Blockout") TObjectPtr<UInstancedStaticMeshComponent> CylinderSolid;
	UPROPERTY(VisibleAnywhere, Category = "Blockout") TObjectPtr<UInstancedStaticMeshComponent> CylinderDetail;
	UPROPERTY(VisibleAnywhere, Category = "Blockout") TObjectPtr<UInstancedStaticMeshComponent> SphereDetail;
	UPROPERTY(VisibleAnywhere, Category = "Blockout") TObjectPtr<UInstancedStaticMeshComponent> ConeDetail;

private:
	UInstancedStaticMeshComponent* MakeISM(const TCHAR* Name, UStaticMesh* Mesh, bool bSolid);
	UInstancedStaticMeshComponent* ISMFor(ENHShape Shape, bool bSolid) const;
	TArray<UInstancedStaticMeshComponent*> AllISMs() const;
};
