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
#include "AnchorVolumeActor.generated.h"

class USphereComponent;
class UBoxComponent;
class UBillboardComponent;

UENUM()
enum class EAnchorVolumeShape : uint8
{
	Box,
	Sphere
};

UCLASS(ClassGroup = (RealtimeDestruction))
class REALTIMEDESTRUCTION_API AAnchorVolumeActor : public AAnchorActor
{
	GENERATED_BODY()
	
public:
	// 设置 Actor 属性的默认值
	AAnchorVolumeActor();

	//~Begin AAnchorActor Interface
	virtual void ApplyToAnchors(const FTransform& MeshTransform, FGridCellLayout& CellCache) override;
	//~End AAnchorActor Interface

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;

	UPROPERTY()
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY()
	TObjectPtr<USphereComponent> Sphere;	
#endif

	FORCEINLINE FVector GetLocalBoxExtent() const { return BoxExtent; }
	FORCEINLINE float GetLocalSphereRadius() const { return SphereRadius; }

	UPROPERTY(EditAnywhere, Category="AnchorActor|Options")
	EAnchorVolumeShape Shape = EAnchorVolumeShape::Box;

	UPROPERTY(EditAnywhere, Category="AnchorActor|Options")
	bool bIsEraser = false;

	UPROPERTY(EditAnywhere, Category="AnchorActor|Options")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, Category="AnchorActor|Options", meta=(ClampMin="1.0", EditCondition="Shape==EAnchorVolumeShape::Box", EditConditionHides))
	FVector BoxExtent = FVector(50.0f, 50.0f, 50.0f);

	UPROPERTY(EditAnywhere, Category="AnchorActor|Options", meta=(ClampMin="1.0", EditCondition="Shape==EAnchorVolumeShape::Sphere", EditConditionHides))
	float SphereRadius = 100.0f;

protected:
	//~Begin AActor Interface
	virtual void BeginPlay() override;
	//~End AActor Interface

public:
	//~Begin AActor Interface
	virtual void Tick(float DeltaTime) override;
	//~End AActor Interface

protected:
	//~Begin AActor Interface
	virtual void OnConstruction(const FTransform& Transform) override;
	//~End AActor Interface

#if WITH_EDITOR
	//~Begin AActor Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown) override;
	//~End AActor Interface

	// 将当前 Actor Scale 烘焙为形状参数并重置 Scale 为 1
	void CommitScaleToShapeParamAndReset();
	// 根据 Actor Scale 预览 Sphere 半径缩放效果
	void UpdateSphereScalePreviewFromActorScale();

	// 根据绝对 Scale 计算 Sphere 缩放系数
	static float ComputeSphereFactorFromAbsScale(const FVector& AbsScale);
	// 安全计算绝对 Scale 的倒数（防止除零）
	static FVector SafeReciprocalAbsScale(const FVector& AbsScale);

	bool bBakingScale = false;
	bool bSphereScalePreview = false;
	float SphereRadiusAtScale = 0.0f;
	float SpherePreviewFactor = 1.0f;
#endif

private:
	// 刷新 Editor 中的体积可视化组件（Box/Sphere）
	void RefreshVisualization();
};
