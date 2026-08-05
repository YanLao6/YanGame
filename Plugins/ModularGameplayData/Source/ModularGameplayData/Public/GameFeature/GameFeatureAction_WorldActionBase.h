// Copyright Chronicler.

#pragma once

#include "GameFeatureAction.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeatureAction_WorldActionBase.generated.h"

#define UE_API MODULARGAMEPLAYDATA_API

class FDelegateHandle;
class UGameInstance;
class UObject;
struct FGameFeatureActivatingContext;
struct FGameFeatureDeactivatingContext;
struct FGameFeatureStateChangeContext;
struct FWorldContext;

/**
 * GameFeatureAction 基类：针对具体 UWorld / GameInstance 生命周期注入逻辑。
 */
UCLASS(MinimalAPI, Abstract)
class UGameFeatureAction_WorldActionBase : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	/** GameInstance 启动时回调，用于将本 Action 应用到对应 WorldContext。 */
	UE_API void HandleGameInstanceStart(UGameInstance* GameInstance, FGameFeatureStateChangeContext ChangeContext);

private:
	/** 子类实现：向目标 World 注入或注册内容。 */
	UE_API virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) PURE_VIRTUAL(UGameFeatureAction_WorldActionBase::AddToWorld,);

	// OnStartGameInstance 委托句柄，按 Activating Context 保存以便 Deactivating 时移除。
	TMap<FGameFeatureStateChangeContext, FDelegateHandle> GameInstanceStartHandles;

	/** @ingroup UGameFeatureAction @{ */
public:
	UE_API virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	UE_API virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
};

#undef UE_API
