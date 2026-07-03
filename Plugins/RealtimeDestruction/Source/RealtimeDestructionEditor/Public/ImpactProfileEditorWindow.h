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
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Misc/NotifyHook.h"

class UDestructionProjectileComponent;
class SImpactProfileEditorViewport;
class IDetailsView;

struct FImpactProfileConfig;
struct FImpactProfileConfigArray;
/**
 * 专门用于 Decal Size 编辑的编辑器窗口
 */

class UImpactProfileDataAsset; 

class SImpactProfileEditorWindow : public SCompoundWidget, public FNotifyHook
{
public: 
	SLATE_BEGIN_ARGS(SImpactProfileEditorWindow): _TargetComponent(nullptr), _TargetDataAsset(nullptr) {}
        SLATE_ARGUMENT(UDestructionProjectileComponent*, TargetComponent)
		SLATE_ARGUMENT(UImpactProfileDataAsset*, TargetDataAsset)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** 设置目标组件 */
	void SetTargetComponent(UDestructionProjectileComponent* InComponent);
	
	/** 作为 Tab 打开窗口的静态函数 */
	static void OpenWindow(UDestructionProjectileComponent* Component);

	static void OpenWindowForDataAsset(UImpactProfileDataAsset* DataAsset);
private:
	
	TArray<TSharedPtr<FString>> ToolShapeOptions;
private:
	/** 属性更改回调 */ 
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;

	void SaveToComponent();
	void SaveToDataAsset();
	void LoadConfigFromDataAsset(FName ConfigID, FName SurfaceType);
	 
	void RefreshConfigIDList();           
	void RefreshSurfaceTypeList();  
	void RefreshVariantIndexList();
	
	FImpactProfileConfig* GetCurrentImpactConfig();
	FImpactProfileConfigArray* GetCurrentImpactConfigArray();
	
	void OnConfigIDSelected(FName SelectedConfigID);
	void OnSurfaceTypeSelected(FName SelectedSurfaceType);
	void OnVariantIndexSelected(int32 SelectedIndex);
	 
	void AddNewSurfaceType();
	void AddNewVariant();

	FName EnsureUniqueConfigID(FName NewName);
	FName EnsureUniqueSurfaceType(FName NewName);

	void DeleteCurrentConfigID();
	void DeleteCurrentSurfaceType();
	void DeleteCurrentVariant();

	void RenameCurrentConfigID(FName NewName);
	void RenameCurrentSurfaceType(FName NewName);

	/** UI 创建辅助函数 */
	TSharedRef<SWidget> CreateToolShapeSection();
	TSharedRef<SWidget> CreateConfigSelectionSection();
	TSharedRef<SWidget> CreatePreviewMeshSection();
	TSharedRef<SWidget> CreateDecalSection();
	
	/** 目标组件 */
	TWeakObjectPtr<UDestructionProjectileComponent> TargetComponent;
	TWeakObjectPtr<UImpactProfileDataAsset> TargetDataAsset;

	/** 视口部件 */
	TSharedPtr<SImpactProfileEditorViewport> Viewport;

	/** 详情视图 */
	TSharedPtr<IDetailsView> DetailsView;

	/** 贴花材质 */
	TObjectPtr<UMaterialInterface> SelectedDecalMaterial;

	enum class EEditMode { Component, DataAsset };
	EEditMode CurrentEditMode = EEditMode::Component;
    
	
	/** 当前选定的 SurfaceType（表面材质） */
	FName CurrentSurfaceType = NAME_None;
   
	/** ConfigID 列表（用于 ComboBox） */
	TArray<TSharedPtr<FName>> ConfigIDList;

	/** SurfaceType 列表（用于 ComboBox） - 当前选定 ConfigID 的表面 */
	TArray<TSharedPtr<FName>> SurfaceTypeList;

	/** 匹配当前选定 Surface Type 的 Variant Index 列表 */
	TArray<TSharedPtr<FString>> VariantIndexList;
	
	/** 当前编辑的材质索引 */
	int32 CurVariantIndex = 0;

	
};
