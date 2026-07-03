// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFeature/GameFeatureAction_WorldActionBase.h"
#include "UObject/ObjectKey.h"
#include "GameFeatureAction_SplitscreenConfig.generated.h"

class UObject;
struct FGameFeatureDeactivatingContext;
struct FGameFeatureStateChangeContext;
struct FWorldContext;

//////////////////////////////////////////////////////////////////////
// 分节：UGameFeatureAction_SplitscreenConfig

/**
 * GameFeature Action：按需在 Viewport 上强制禁用 Splitscreen（分屏）。
 *
 * 通过全局计数 Vote 合并多个 Feature 的请求，避免单个 Feature 卸载后误恢复分屏。
 */
UCLASS(MinimalAPI, meta = (DisplayName = "Splitscreen Config"))
class UGameFeatureAction_SplitscreenConfig final : public UGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()

public:
	//~ UGameFeatureAction 接口
	/** GameFeature 反激活：按本实例投票撤销分屏禁用。 */
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	//~

	//~ UGameFeatureAction_WorldActionBase 接口
	/** World 就绪：若启用 bDisableSplitscreen 则为 Viewport 增加禁用分屏投票。 */
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	//~

public:
	/** 为 true 时在本 World 流程中投票禁用分屏。 */
	UPROPERTY(EditAnywhere, Category=Action)
	bool bDisableSplitscreen = true;

private:
	// 本 Action 实例投出的 Viewport 投票（用于 Deactivate 时回滚）
	TArray<FObjectKey> LocalDisableVotes;

	// 跨实例合并的禁用计数（FObjectKey = GameViewportClient）
	static TMap<FObjectKey, int32> GlobalDisableVotes;
};
