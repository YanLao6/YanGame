// Copyright (c) 2026 LazyDevelopers <lazydeveloper24@gmail.com>. All rights reserved.
// This plugin is distributed under the Fab Standard License.
//
// This product was independently developed by us while participating in the Epic Project, a developer-support
// program of the KRAFTON JUNGLE GameTech Lab. All rights, title, and interest in and to the product are exclusively
// vested in us. Krafton, Inc. was not involved in its development and distribution and disclaims all representations
// and warranties, express or implied, and assumes no responsibility or liability for any consequences arising from
// the use of this product.

#pragma once

#include "CoreMinimal.h"
#include "Actors/AnchorActor.h"
#include "AnchorPlaneActor.generated.h"

UCLASS(ClassGroup = (RealtimeDestruction))
class REALTIMEDESTRUCTION_API AAnchorPlaneActor : public AAnchorActor
{
	GENERATED_BODY()
	
public:
	// 设置 Actor 属性的默认值
	AAnchorPlaneActor();

	//~Begin AAnchorActor Interface
	virtual void ApplyToAnchors(const FTransform& MeshTransform, FGridCellLayout& CellCache) override;
	//~End AAnchorActor Interface

#if WITH_EDITORONLY_DATA

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PlaneMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PlaneMID;

#endif

	UPROPERTY(EditAnywhere, Category = "AnchorActor|Options")
	bool bIsEraser = false;

	UPROPERTY(EditAnywhere, Category = "AnchorActor|Options")
	int32 Priority = 0;

protected:
	//~Begin AActor Interface
	virtual void BeginPlay() override;
	//~End AActor Interface

public:
	//~Begin AActor Interface
	virtual void Tick(float DeltaTime) override;
	//~End AActor Interface

	
	
};
