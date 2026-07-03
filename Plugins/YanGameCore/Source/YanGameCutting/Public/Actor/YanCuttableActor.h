#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YanCuttableActor.generated.h"

class UGeometryCollectionComponent;
class UGeometryCollection;
class UMaterialInterface;
class UStaticMesh;

/**
 * Fired after CutByPlane() modifies the geometry collection.
 * The actor remains alive; the two new physics pieces simulate inside GeoCollectionComp.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeshCutSignature, AYanCuttableActor*, Source);

/**
 * Actor that can be cut at runtime using PlanarCut.
 *
 * Internally holds a UGeometryCollectionComponent with a transient UGeometryCollection
 * built from a source StaticMesh. CutByPlane() fractures the collection with FPlanarCells,
 * optional Perlin noise displacement, and proper UV generation on the cut face; it then
 * switches the component to dynamic and breaks the root cluster via ApplyExternalStrain
 * so the two halves simulate independently.
 *
 * The actor is NOT destroyed on cut — GeoCollectionComp owns all fragment physics.
 * Recursive cuts are supported by passing the transform index of the specific piece to cut.
 */
UCLASS()
class YANGAMECUTTING_API AYanCuttableActor : public AActor
{
	GENERATED_BODY()

public:
	AYanCuttableActor();

	/**
	 * Convert a StaticMesh into a transient UGeometryCollection and assign it to the
	 * component. Must be called before any CutByPlane() invocation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cutting")
	void InitializeFromStaticMesh(UStaticMesh* StaticMesh);

	/**
	 * Slice the geometry piece at InTransformIdx with a world-space plane.
	 * The two resulting pieces are added as children in the GC hierarchy and immediately
	 * simulated as separate Chaos rigid bodies.
	 *
	 * @param WorldOrigin    A point on the cutting plane in world space (typically Hit.ImpactPoint)
	 * @param WorldNormal    Normal of the cutting plane in world space
	 * @param InTransformIdx Transform index of the piece to cut (0 = root / whole mesh;
	 *                       use Hit.Item from a line-trace result for recursive cuts)
	 */
	UFUNCTION(BlueprintCallable, Category = "Cutting")
	void CutByPlane(FVector WorldOrigin, FVector WorldNormal, int32 InTransformIdx = 0);

	/** Broadcast after each successful cut. The actor continues to exist. */
	UPROPERTY(BlueprintAssignable, Category = "Cutting")
	FOnMeshCutSignature OnMeshCut;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGeometryCollectionComponent> GeoCollectionComp;

	/**
	 * Material applied to the freshly-generated cut face (internal faces).
	 * If null, the last material slot of the source mesh is reused.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Cutting")
	TObjectPtr<UMaterialInterface> CutSurfaceMaterial;

	/** Enable Perlin noise displacement on the cut edge for a natural fracture look. */
	UPROPERTY(EditDefaultsOnly, Category = "Cutting")
	bool bEnableCutNoise = false;

	/** Peak displacement of the noise (cm). Only used when bEnableCutNoise is true. */
	UPROPERTY(EditDefaultsOnly, Category = "Cutting",
		meta = (ClampMin = "0", EditCondition = "bEnableCutNoise"))
	float CutNoiseAmplitude = 3.f;

	/** Frequency of the Perlin noise. Only used when bEnableCutNoise is true. */
	UPROPERTY(EditDefaultsOnly, Category = "Cutting",
		meta = (ClampMin = "0.001", EditCondition = "bEnableCutNoise"))
	float CutNoiseFrequency = 0.1f;

	/** Initial separation speed (cm/s) applied along ±WorldNormal to the two new pieces. */
	UPROPERTY(EditDefaultsOnly, Category = "Cutting", meta = (ClampMin = "0"))
	float FragmentImpulse = 150.f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UGeometryCollection> RuntimeCollection;
};
