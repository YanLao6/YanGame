// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/GameStateComponent.h"
#include "LoadingProcessInterface.h"
#include "GameMode/ModularExperienceDefinition.h"

#include "ModularExperienceComponent.generated.h"

namespace UE::GameFeatures { struct FResult; }

/** Experience 加载完成时回调，参数为当前 UModularExperienceDefinition。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnModularExperienceLoaded, const UModularExperienceDefinition* /*Experience*/);

/** Experience 异步加载状态机阶段。 */
enum class EModularExperienceLoadState
{
	Unloaded,
	Loading,
	LoadingGameFeatures,
	LoadingChaosTestingDelay,
	ExecutingActions,
	Loaded,
	Deactivating
};

/**
 * Experience 管理组件。
 *
 * 负责当前 Experience 的加载状态机、GameFeature 激活、Action 执行与加载完成回调分发。
 * 常挂载于 GameState。
 */
UCLASS()
class MODULARGAMEPLAYEXPERIENCES_API UModularExperienceComponent final : public UGameStateComponent, public ILoadingProcessInterface
{
	GENERATED_BODY()

public:
	/** 构造 Experience 组件。 */
	explicit UModularExperienceComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UActorComponent interface
	/** 组件结束运行时，停止加载流程并清理激活状态。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	//~ILoadingProcessInterface interface
	/** 告知 LoadingScreen 当前 Experience 是否仍在加载。 */
	virtual bool ShouldShowLoadingScreen(FString& OutReason) const override;
	//~End of ILoadingProcessInterface

	/** 设置当前 Experience（PrimaryAssetId），在 Authority 上触发加载状态机。 */
	void SetCurrentExperience(const FPrimaryAssetId& ExperienceId);

	/**
	 * 注册或立即执行：Experience 加载完成（高优先级，先于普通回调）。
	 * 若已加载则立刻 Execute。
	 */
	void CallOrRegister_OnExperienceLoaded_HighPriority(FOnModularExperienceLoaded::FDelegate&& Delegate);

	/**
	 * 注册或立即执行：Experience 加载完成（默认优先级）。
	 * 若已加载则立刻 Execute。
	 */
	void CallOrRegister_OnExperienceLoaded(FOnModularExperienceLoaded::FDelegate&& Delegate);

	/**
	 * 注册或立即执行：Experience 加载完成（低优先级，最后触发）。
	 * 若已加载则立刻 Execute。
	 */
	void CallOrRegister_OnExperienceLoaded_LowPriority(FOnModularExperienceLoaded::FDelegate&& Delegate);

	/**
	 * 返回已完全加载的 CurrentExperience；若过早调用将 check 失败。
	 */
	const UModularExperienceDefinition* GetCurrentExperienceChecked() const;

	/** 是否已完成加载（Loaded 且 CurrentExperience 非空）。 */
	bool IsExperienceLoaded() const;

private:
	UFUNCTION()
	void OnRep_CurrentExperience();

	void StartExperienceLoad();
	void OnExperienceLoadComplete();
	void OnGameFeaturePluginLoadComplete(const UE::GameFeatures::FResult& Result);
	void OnExperienceFullLoadCompleted();

	void OnActionDeactivationCompleted();
	void OnAllActionsDeactivated();

private:
	UPROPERTY(ReplicatedUsing=OnRep_CurrentExperience)
	TObjectPtr<const UModularExperienceDefinition> CurrentExperience;

	EModularExperienceLoadState LoadState = EModularExperienceLoadState::Unloaded;

	int32 NumGameFeaturePluginsLoading = 0;
	TArray<FString> GameFeaturePluginURLs;

	int32 NumObservedPausers = 0;
	int32 NumExpectedPausers = 0;

	// 加载完成回调：高优先级（Gameplay 就绪前需完成的子系统）
	FOnModularExperienceLoaded OnExperienceLoaded_HighPriority;

	// 加载完成回调：默认优先级
	FOnModularExperienceLoaded OnExperienceLoaded;

	// 加载完成回调：低优先级
	FOnModularExperienceLoaded OnExperienceLoaded_LowPriority;
};
