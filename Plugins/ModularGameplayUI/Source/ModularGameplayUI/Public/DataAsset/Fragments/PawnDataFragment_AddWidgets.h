// Copyright Chronicler.

#pragma once

#include "CommonActivatableWidget.h"
#include "GameplayTagContainer.h"
#include "UIExtensionSystem.h"
#include "DataAsset/PawnDataFragment.h"

#include "PawnDataFragment_AddWidgets.generated.h"

#define UE_API MODULARGAMEPLAYUI_API

/**
 * 一条推入 HUD Layer 的 Layout 配置。
 */
USTRUCT()
struct FPawnDataWidgetLayoutEntry
{
	GENERATED_BODY()

	/** 要生成的 Layout（ActivatableWidget）Soft Class。 */
	UPROPERTY(EditDefaultsOnly, Category = "UI", Meta = (AssetBundles = "Client"))
	TSoftClassPtr<UCommonActivatableWidget> LayoutClass;

	/** 插入目标 Layer 的 GameplayTag。 */
	UPROPERTY(EditDefaultsOnly, Category = "UI", Meta = (Categories = "UI.Layer"))
	FGameplayTag LayerID;
};

/**
 * 一条挂到 UIExtension 扩展点的 Widget 配置。
 */
USTRUCT()
struct FPawnDataWidgetElementEntry
{
	GENERATED_BODY()

	/** 要生成的 Widget Soft Class。 */
	UPROPERTY(EditDefaultsOnly, Category = "UI", Meta = (AssetBundles = "Client"))
	TSoftClassPtr<UUserWidget> WidgetClass;

	/** UIExtension Slot 的 GameplayTag。 */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	FGameplayTag SlotID;

	/** 同一 Slot 内的排序优先级。 */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	int32 Priority = -1;
};

/**
 * 记录本次注入创建的界面内容，供撤销时回收。
 */
UCLASS(MinimalAPI)
class UPawnDataFragmentState_AddWidgets : public UPawnDataFragmentState
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<TWeakObjectPtr<UCommonActivatableWidget>> LayoutsAdded;

	UPROPERTY()
	TArray<FUIExtensionHandle> ExtensionHandles;
};

/**
 * 界面注入 Fragment。
 *
 * 对应 GameFeatureAction 的 Add Widgets，但作用域为单个玩家的英雄配置。
 * 仅在拥有 LocalPlayer 的一端生效，专用服务器与他人客户端上为空操作。
 */
UCLASS(MinimalAPI, Const, DisplayName = "Add Widgets")
class UPawnDataFragment_AddWidgets : public UPawnDataFragment
{
	GENERATED_BODY()

public:
	//~Begin UPawnDataFragment Interface
	virtual EPawnDataFragmentScope  GetScope() const override { return EPawnDataFragmentScope::Pawn; }
	virtual bool                    RequiresAuthority() const override { return false; }
	UE_API virtual UPawnDataFragmentState* Apply(const FPawnDataFragmentContext& Context) const override;
	UE_API virtual void                    Revoke(const FPawnDataFragmentContext& Context, UPawnDataFragmentState* State) const override;
#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsFragmentValid(FDataValidationContext& Context) const override;
#endif
	//~End UPawnDataFragment Interface

	/** 推入 HUD 各 Layer 的 Layout 列表。 */
	UPROPERTY(EditDefaultsOnly, Category = "UI", Meta = (TitleProperty = "{LayerID} -> {LayoutClass}"))
	TArray<FPawnDataWidgetLayoutEntry> Layout;

	/** 通过 UIExtension 挂载的 Widget 条目。 */
	UPROPERTY(EditDefaultsOnly, Category = "UI", Meta = (TitleProperty = "{SlotID} -> {WidgetClass}"))
	TArray<FPawnDataWidgetElementEntry> Widgets;
};

#undef UE_API
