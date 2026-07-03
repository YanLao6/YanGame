// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "GameplayTagContainer.h"
#include "UIExtensionSystem.h"
#include "GameFeature/GameFeatureAction_WorldActionBase.h"

#include "GameFeatureAction_AddWidget.generated.h"

struct FWorldContext;
struct FComponentRequestHandle;

USTRUCT()
struct FModularGameplayHUDLayoutRequest
{
	GENERATED_BODY()

	/** 要生成的 Layout（ActivatableWidget）Soft Class。 */
	UPROPERTY(EditAnywhere, Category=UI, meta=(AssetBundles="Client"))
	TSoftClassPtr<UCommonActivatableWidget> LayoutClass;

	/** 插入目标 Layer 的 GameplayTag（需与 PrimaryGameLayout 注册一致）。 */
	UPROPERTY(EditAnywhere, Category=UI, meta=(Categories="UI.Layer"))
	FGameplayTag LayerID;
};


USTRUCT()
struct FModularGameplayHUDElementEntry
{
	GENERATED_BODY()

	/** 要生成的 Widget Soft Class。 */
	UPROPERTY(EditAnywhere, Category=UI, meta=(AssetBundles="Client"))
	TSoftClassPtr<UUserWidget> WidgetClass;

	/** UIExtension Slot 的 GameplayTag。 */
	UPROPERTY(EditAnywhere, Category = UI)
	FGameplayTag SlotID;
};

/**
 * GameFeature Widget 注入动作。
 *
 * 在目标 HUD/扩展点上添加布局与元素 Widget，并在停用时回收。
 */
UCLASS(MinimalAPI, meta = (DisplayName = "Add Widgets"))
class UGameFeatureAction_AddWidgets final : public UGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()

public:
	//~ UGameFeatureAction 接口
	/** GameFeature 反激活时移除已添加 UI。 */
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
#if WITH_EDITORONLY_DATA
	/** Editor：为 Client Load State 追加本 Action 引用的 Widget 资源 Bundle。 */
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) override;
#endif
	//~

	//~ UObject 接口
#if WITH_EDITOR
	/** Editor：校验 Layout/Widgets 条目中 Class 与 GameplayTag 是否有效。 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~

private:
	/** 推入 HUD 各 Layer 的 Layout 列表。 */
	UPROPERTY(EditAnywhere, Category=UI, meta=(TitleProperty="{LayerID} -> {LayoutClass}"))
	TArray<FModularGameplayHUDLayoutRequest> Layout;

	/** 通过 UIExtension 挂到 HUD 的 Widget 条目。 */
	UPROPERTY(EditAnywhere, Category=UI, meta=(TitleProperty="{SlotID} -> {WidgetClass}"))
	TArray<FModularGameplayHUDElementEntry> Widgets;

private:

	// 每个 HUD Actor 上创建的 Layout 与 Extension Handle
	struct FPerActorData
	{
		TArray<TWeakObjectPtr<UCommonActivatableWidget>> LayoutsAdded;
		TArray<FUIExtensionHandle> ExtensionHandles;
	};

	struct FPerContextData
	{
		TArray<TSharedPtr<FComponentRequestHandle>> ComponentRequests;
		TMap<FObjectKey, FPerActorData> ActorData; 
	};

	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;

	//~ UGameFeatureAction_WorldActionBase 接口
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	//~

	// 清空某 ChangeContext 下的注册与 Extension
	void Reset(FPerContextData& ActiveData);

	// GameFrameworkComponentManager 扩展回调
	void HandleActorExtension(AActor* Actor, FName EventName, FGameFeatureStateChangeContext ChangeContext);

	void AddWidgets(AActor* Actor, FPerContextData& ActiveData);
	void RemoveWidgets(AActor* Actor, FPerContextData& ActiveData);
};
