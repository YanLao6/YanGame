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
#include "SEditorViewport.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "AdvancedPreviewScene.h"
#include "RealtimeDestruction/Public/Components/DestructionTypes.h"

class UDestructionProjectileComponent;
class FImpactProfileViewportClient;
class FAdvancedPreviewScene;

/**
 * 用于编辑 DecalSize 的预览视口
 */
class SImpactProfileEditorViewport : public SEditorViewport, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SImpactProfileEditorViewport) {}
		SLATE_ARGUMENT(UDestructionProjectileComponent*, TargetComponent)
	SLATE_END_ARGS()

	void    Construct(const FArguments& InArgs);
	virtual ~SImpactProfileEditorViewport() override;

	// FGCObject Interface
	virtual void    AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SImpactProfileEditorViewport"); };

	/** 设置目标组件 */
	void SetTargetComponent(UDestructionProjectileComponent* InComponent);

	/** 刷新预览 */
	void RefreshPreview();

	/** 设置贴花变换 */
	void       SetDecalTransform(const FTransform& InTransform);
	FTransform GetDecalTransform() const { return DecalTransform; }

	/** 设置工具形状变换 */
	void SetToolShapeLocation(const FVector& InLocation);
	void SetToolShapeRotation(const FRotator& InRotation);

	void SetPreviewMesh(UStaticMesh* InPreviewMesh);
	void SetPreviewToolShape(EDestructionToolShape NewShape);
	void SetPreviewSphere(float InRadius);
	void SetPreviewCylinderRadius(float InRadius);
	void SetPreviewCylinderHeight(float InHeight);

	void SetPreviewMeshLocation(const FVector& InLocation);
	void SetPreviewMeshRotation(const FRotator& InRotator);

	FVector  GetToolShapeLocation() const { return ToolShapeTransform.GetLocation(); }
	FRotator GetToolShapeRotation() const { return ToolShapeTransform.GetRotation().Rotator(); }

	EDestructionToolShape GetPreviewToolShape() const { return PreviewToolShape; }
	float                 GetPreviewSphereRadius() const { return PreviewSphereRadius; }
	float                 GetPreviewCylinderRadius() const { return PreviewCylinderRadius; }
	float                 GetPreviewCylinderHeight() const { return PreviewCylinderHeight; }
	UStaticMesh*          GetPreviewMesh() const { return PreviewMesh; }

	FVector  GetPreviewMeshLocation() const { return PreviewMeshLocation; }
	FRotator GetPreviewMeshRotation() const { return PreviewMeshRotation; }

	/** 仅更新预览网格（无需完全刷新） */
	void UpdateDecalMesh();
	void UpdateDecalWireframe();


	/** 贴花材质 */
	void                SetDecalMaterial(UMaterialInterface* InMaterial);
	UMaterialInterface* GetDecalMaterial() const { return DecalMaterial; }

	void    SetDecalSize(const FVector& InSize);
	FVector GetDecalSize() const { return DecalSize; }

	// 可见性切换
	void SetDecalVisible(bool bVisible);
	void SetToolShapeVisible(bool bVisible);
	void SetPreviewMeshVisible(bool bVisible);

	bool IsDecalVisible() const { return bShowDecal; }
	bool IsToolShapeVisible() const { return bShowToolShape; }
	bool IsPreviewMeshVisible() const { return bShowPreviewMesh; }

protected:
	//SEditorViewport 接口
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;

private:
	/** 目标组件 */
	TWeakObjectPtr<UDestructionProjectileComponent> TargetComponent;

	/** 预览场景 */
	TSharedPtr<FAdvancedPreviewScene> PreviewScene;

	/** 视口客户端 */
	TSharedPtr<FImpactProfileViewportClient> ViewportClient;

	/** 预览 Actor */
	TObjectPtr<AActor>                     PreviewActor          = nullptr;
	TObjectPtr<class UDecalComponent>      DecalPreviewComponent = nullptr;
	TObjectPtr<class UStaticMeshComponent> DecalTargetSurface    = nullptr; // 用于贴花投影的表面
	TObjectPtr<class ULineBatchComponent>  DecalWireframe        = nullptr;
	TObjectPtr<class UStaticMeshComponent> ProjectileMesh        = nullptr;
	TObjectPtr<class ULineBatchComponent>  ToolShapeWireframe    = nullptr;
	TObjectPtr<UStaticMesh>                PreviewMesh           = nullptr;

	/** 贴花预览变换 */
	FTransform DecalTransform;
	FVector    DecalSize = FVector(1.0f, 50.0f, 50.0f);

	/** 工具形状预览变换 */
	EDestructionToolShape PreviewToolShape = EDestructionToolShape::Cylinder;
	FTransform            ToolShapeTransform;

	/** 预览网格数据 */
	FVector  PreviewMeshLocation = FVector::ZeroVector;
	FRotator PreviewMeshRotation = FRotator::ZeroRotator;

	void UpdateToolShapeWireframe();

	float PreviewSphereRadius   = 5.0f;
	float PreviewCylinderRadius = 5.0f;
	float PreviewCylinderHeight = 20.0f;

	/** 贴花材质 */
	TObjectPtr<UMaterialInterface> DecalMaterial;

	/** 可见性标志 */
	bool bShowDecal       = true;
	bool bShowToolShape   = true;
	bool bShowPreviewMesh = true;

	/** 用于保存状态的函数 */
	void SaveState();
};


/** 视口客户端 */
class FImpactProfileViewportClient : public FEditorViewportClient
{
public:
	FImpactProfileViewportClient(FAdvancedPreviewScene* InAdvancedPreviewScene, const TWeakPtr<SEditorViewport>& InEditorViewport);

	// FEditorViewportClient 接口
	virtual void             Tick(float DeltaSeconds) override;
	virtual FSceneInterface* GetScene() const override;
	virtual FLinearColor     GetBackgroundColor() const override;

private:
	FAdvancedPreviewScene* PreviewScene;
};
