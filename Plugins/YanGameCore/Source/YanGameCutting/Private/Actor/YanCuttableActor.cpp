#include "Actor/YanCuttableActor.h"

#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"
#include "PlanarCut.h"
#include "FractureAutoUV.h"
#include "Chaos/ErrorReporter.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanCuttableActor)

AYanCuttableActor::AYanCuttableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	GeoCollectionComp = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GeoCollectionComp"));
	GeoCollectionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GeoCollectionComp->SetCollisionObjectType(ECC_WorldDynamic);
	GeoCollectionComp->SetCollisionResponseToAllChannels(ECR_Block);
	// Start kinematic so the object stays in place until first cut
	GeoCollectionComp->ObjectType       = EObjectStateTypeEnum::Chaos_Object_Kinematic;
	GeoCollectionComp->EnableClustering = true;
	SetRootComponent(GeoCollectionComp);
}

void AYanCuttableActor::InitializeFromStaticMesh(UStaticMesh* StaticMesh)
{
	if (!StaticMesh)
	{
		return;
	}

	RuntimeCollection = NewObject<UGeometryCollection>(this, NAME_None, RF_Transient);

	// Collect source materials; no internal duplicates — we add our own cut-face slot below
	TArray<UMaterialInterface*> SrcMaterials;
	for (const FStaticMaterial& Mat : StaticMesh->GetStaticMaterials())
	{
		SrcMaterials.Add(Mat.MaterialInterface);
	}

	FGeometryCollectionEngineConversion::AppendStaticMesh(
		StaticMesh,
		SrcMaterials,
		FTransform::Identity,
		RuntimeCollection,
		/*bReindexMaterials=*/
		true,
		/*bAddInternalMaterials=*/
		false);

	// Append the cut-surface material as the last slot.
	// FInternalSurfaceMaterials::GlobalMaterialID will index into this array.
	UMaterialInterface* InternalMat = CutSurfaceMaterial
		                                  ? CutSurfaceMaterial.Get()
		                                  : (SrcMaterials.Num() > 0 ? SrcMaterials.Last() : nullptr);

	if (InternalMat)
	{
		RuntimeCollection->Materials.Add(InternalMat);
	}

	FSharedSimulationParameters SharedParams;
	RuntimeCollection->GetSharedSimulationParams(SharedParams);
	Chaos::FErrorReporter ErrorReporter(TEXT("RuntimeCollection"));
	BuildSimulationData(ErrorReporter, *RuntimeCollection->GetGeometryCollection().Get(), SharedParams);
	RuntimeCollection->CacheMaterialDensity();

	GeoCollectionComp->SetRestCollection(RuntimeCollection);
}

void AYanCuttableActor::CutByPlane(FVector WorldOrigin, FVector WorldNormal, int32 InTransformIdx)
{
	if (!RuntimeCollection)
	{
		return;
	}

	// Snapshot the transform count so we can identify the new child pieces afterward
	int32 NegTransformIdx = INDEX_NONE;
	int32 PosTransformIdx = INDEX_NONE;

	// All geometry mutations happen inside the edit scope so the physics proxy is
	// cleanly torn down and rebuilt when the scope closes
	{
		FGeometryCollectionEdit EditScope(GeoCollectionComp, GeometryCollection::EEditUpdate::RestPhysicsDynamic);

		FGeometryCollection* FGC = RuntimeCollection->GetGeometryCollection().Get();
		if (!FGC || FGC->NumElements(FGeometryCollection::GeometryGroup) == 0)
		{
			return;
		}

		// Convert world-space plane to the collection's local space
		// (the collection lives at the actor's world transform)
		const FTransform ActorTransform = GetActorTransform();
		const FVector    LocalOrigin    = ActorTransform.InverseTransformPosition(WorldOrigin);
		const FVector    LocalNormal    = ActorTransform.InverseTransformVectorNoScale(WorldNormal).GetSafeNormal();

		FPlanarCells LocalCells(FPlane(LocalOrigin, LocalNormal));

		// Cut-surface material: use the last slot added in InitializeFromStaticMesh
		LocalCells.InternalSurfaceMaterials.GlobalMaterialID = FMath::Max(0, RuntimeCollection->Materials.Num() - 1);
		LocalCells.InternalSurfaceMaterials.GlobalUVScale    = 1.f;

		if (bEnableCutNoise)
		{
			FNoiseSettings Noise;
			Noise.Amplitude = CutNoiseAmplitude;
			Noise.Frequency = CutNoiseFrequency;
			LocalCells.SetNoise(Noise);
		}

		// Record how many transforms exist before the cut
		const int32 TransformCountBefore =
		FGC->NumElements(FTransformCollection::TransformGroup);

		CutWithPlanarCells(LocalCells, *FGC, InTransformIdx, 0.0, 10.0, FMath::Rand());

		// The two new leaf pieces are the first two transforms added after the cut.
		// CutWithPlanarCells appends: [negative-side piece, positive-side piece]
		NegTransformIdx = TransformCountBefore;
		PosTransformIdx = TransformCountBefore + 1;

		// Box-project UVs onto the new internal (cut) faces; auto-fit bounds
		UE::PlanarCut::BoxProjectUVs(
			/*TargetUVLayer=*/0,
			                  *FGC,
			                  FVector3d::One(),
			                  // ignored — overridden by actual bounds below
			                  UE::PlanarCut::ETargetFaces::InternalFaces,
			                  TArrayView<int32>(),
			                  FVector2f(0.5f, 0.5f),
			                  /*bOverrideBoxDimensionsWithBounds=*/
			                  true);

		// Switch to dynamic so pieces simulate once the proxy is rebuilt
		GeoCollectionComp->ObjectType = EObjectStateTypeEnum::Chaos_Object_Dynamic;
	} // ~FGeometryCollectionEdit — physics proxy is destroyed and recreated here

	// Wait one physics frame for the new proxy to register with the Chaos solver,
	// then break the root cluster and apply separation impulses
	const FVector CapturedNormal  = WorldNormal;
	const int32   CapturedRoot    = InTransformIdx;
	const int32   CapturedNeg     = NegTransformIdx;
	const int32   CapturedPos     = PosTransformIdx;
	const float   CapturedImpulse = FragmentImpulse;

	GetWorldTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this,
		                                 [this, CapturedRoot, CapturedNeg, CapturedPos, CapturedNormal, CapturedImpulse]()
		                                 {
			                                 if (!IsValid(GeoCollectionComp))
			                                 {
				                                 return;
			                                 }
			                                 // Break the cluster at the cut transform (strain exceeds any threshold)
			                                 GeoCollectionComp->ApplyExternalStrain(
				                                 CapturedRoot,
				                                 GetActorLocation(),
				                                 /*Radius=*/
				                                 0.f,
				                                 /*PropagationDepth=*/
				                                 0,
				                                 /*PropagationFactor=*/
				                                 1.f,
				                                 /*Strain=*/
				                                 TNumericLimits<float>::Max());

			                                 // Kick the two halves apart along the cut normal
			                                 if (CapturedNeg != INDEX_NONE)
			                                 {
				                                 GeoCollectionComp->ApplyLinearVelocity(CapturedNeg, -CapturedNormal * CapturedImpulse);
			                                 }
			                                 if (CapturedPos != INDEX_NONE)
			                                 {
				                                 GeoCollectionComp->ApplyLinearVelocity(CapturedPos, CapturedNormal * CapturedImpulse);
			                                 }
		                                 }));

	OnMeshCut.Broadcast(this);
}
