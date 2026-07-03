// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorComponent/ModularExperienceComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeatureAction.h"
#include "GameFeaturesSubsystemSettings.h"
#include "ModularBundles.h"
#include "ModularGameplayExperiencesLogs.h"
#include "TimerManager.h"
#include "DataAsset/ModularAssetManager.h"
#include "GameMode/ModularExperienceManager.h"
#include "GameMode/ModularWorldSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularExperienceComponent)

//@TODO: ExperienceDefinition 自身改为异步加载路径
//@TODO: 显式失败状态机，避免仅靠 check() 崩溃
//@TODO: 按生命周期阶段拆分执行 Action，而非一次性跑完
//@TODO: 支持完整 Experience Deactivate + Unload 动作链
//@TODO: 明确预加载资源在 Deactivate 时的回收语义
//@TODO: GameFeature 启用差分：客户端切换 Experience 时应 Diff 依赖，仅卸载增量，避免「全卸再全装」泄漏/抖动
//@TODO: 同时支持内置 Plugin 与 URL Plugin（可用冒号启发式区分）

namespace ModularGameplayExperiencesConsoleVariables
{
	static float                   ExperienceLoadRandomDelayMin = 0.0f;
	static FAutoConsoleVariableRef CVarExperienceLoadRandomDelayMin(
	                                                                TEXT("mge.chaos.ExperienceDelayLoad.MinSecs"),
	                                                                ExperienceLoadRandomDelayMin,
	                                                                TEXT("Experience 加载完成后的固定延迟（秒），会与 RandomSecs 随机量叠加，用于 Chaos 测试"),
	                                                                ECVF_Default);

	static float                   ExperienceLoadRandomDelayRange = 0.0f;
	static FAutoConsoleVariableRef CVarExperienceLoadRandomDelayRange(
	                                                                  TEXT("mge.chaos.ExperienceDelayLoad.RandomSecs"),
	                                                                  ExperienceLoadRandomDelayRange,
	                                                                  TEXT("在 [0, 本值] 秒之间取的随机延迟，会与 MinSecs 固定量叠加"),
	                                                                  ECVF_Default);

	float GetExperienceLoadDelayDuration()
	{
		return FMath::Max(0.0f, ExperienceLoadRandomDelayMin + FMath::FRand() * ExperienceLoadRandomDelayRange);
	}
}

UModularExperienceComponent::UModularExperienceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UModularExperienceComponent::SetCurrentExperience(const FPrimaryAssetId& ExperienceId)
{
	if (!ExperienceId.IsValid())
	{
		return;
	}

	const UModularAssetManager&                     AssetManager = UModularAssetManager::Get();
	const FSoftObjectPath                           AssetPath    = AssetManager.GetPrimaryAssetPath(ExperienceId);
	const TSubclassOf<UModularExperienceDefinition> AssetClass   = Cast<UClass>(AssetPath.TryLoad());
	check(AssetClass);
	const UModularExperienceDefinition* Experience = GetDefault<UModularExperienceDefinition>(AssetClass);

	check(Experience != nullptr);
	check(CurrentExperience == nullptr);
	CurrentExperience = Experience;
	StartExperienceLoad();
}

void UModularExperienceComponent::CallOrRegister_OnExperienceLoaded_HighPriority(FOnModularExperienceLoaded::FDelegate&& Delegate)
{
	if (IsExperienceLoaded())
	{
		Delegate.Execute(CurrentExperience);
	}
	else
	{
		OnExperienceLoaded_HighPriority.Add(MoveTemp(Delegate));
	}
}

void UModularExperienceComponent::CallOrRegister_OnExperienceLoaded(FOnModularExperienceLoaded::FDelegate&& Delegate)
{
	if (IsExperienceLoaded())
	{
		Delegate.Execute(CurrentExperience);
	}
	else
	{
		OnExperienceLoaded.Add(MoveTemp(Delegate));
	}
}

void UModularExperienceComponent::CallOrRegister_OnExperienceLoaded_LowPriority(FOnModularExperienceLoaded::FDelegate&& Delegate)
{
	if (IsExperienceLoaded())
	{
		Delegate.Execute(CurrentExperience);
	}
	else
	{
		OnExperienceLoaded_LowPriority.Add(MoveTemp(Delegate));
	}
}

const UModularExperienceDefinition* UModularExperienceComponent::GetCurrentExperienceChecked() const
{
	check(LoadState == EModularExperienceLoadState::Loaded);
	check(CurrentExperience != nullptr);
	return CurrentExperience;
}

bool UModularExperienceComponent::IsExperienceLoaded() const
{
	const bool bIsLoaded = (LoadState == EModularExperienceLoadState::Loaded) && (CurrentExperience != nullptr);
	return bIsLoaded;
}

void UModularExperienceComponent::OnRep_CurrentExperience()
{
	StartExperienceLoad();
}

void UModularExperienceComponent::StartExperienceLoad()
{
	check(CurrentExperience != nullptr);
	check(LoadState == EModularExperienceLoadState::Unloaded);

	UE_LOG(LogModularGameplayExperiences,
	       Log,
	       TEXT("EXPERIENCE: StartExperienceLoad(CurrentExperience = %s, %s)"),
	       *CurrentExperience->GetPrimaryAssetId().ToString(),
	       *GetClientServerContextString(this));

	LoadState = EModularExperienceLoadState::Loading;

	UModularAssetManager& AssetManager = UModularAssetManager::Get();

	TSet<FPrimaryAssetId> BundleAssetList;
	TSet<FSoftObjectPath> RawAssetList;

	BundleAssetList.Add(CurrentExperience->GetPrimaryAssetId());
	for (const TObjectPtr<UModularExperienceActionSet>& ActionSet : CurrentExperience->ActionSets)
	{
		if (ActionSet != nullptr)
		{
			BundleAssetList.Add(ActionSet->GetPrimaryAssetId());
		}
	}

	// WorldSettings 上直接配置的 ActionSet 也需要纳入 Bundle 加载
	if (const AModularWorldSettings* WorldSettings = Cast<AModularWorldSettings>(GetWorld()->GetWorldSettings()))
	{
		for (const TObjectPtr<UModularExperienceActionSet>& ActionSet : WorldSettings->ActionSets)
		{
			if (ActionSet != nullptr)
			{
				BundleAssetList.Add(ActionSet->GetPrimaryAssetId());
			}
		}
	}

	// 加载 Experience 关联 PrimaryAsset 的 Bundle

	TArray<FName> BundlesToLoad;
	BundlesToLoad.Add(FModularBundles::Equipped);

	//@TODO: 将 Client/Server Bundle 选择逻辑收敛到项目 AssetManager
	const ENetMode OwnerNetMode = GetOwner()->GetNetMode();
	const bool     bLoadClient  = GIsEditor || (OwnerNetMode != NM_DedicatedServer);
	const bool     bLoadServer  = GIsEditor || (OwnerNetMode != NM_Client);
	if (bLoadClient)
	{
		BundlesToLoad.Add(UGameFeaturesSubsystemSettings::LoadStateClient);
	}
	if (bLoadServer)
	{
		BundlesToLoad.Add(UGameFeaturesSubsystemSettings::LoadStateServer);
	}

	TSharedPtr<FStreamableHandle> BundleLoadHandle = nullptr;
	if (BundleAssetList.Num() > 0)
	{
		BundleLoadHandle = AssetManager.ChangeBundleStateForPrimaryAssets(BundleAssetList.Array(), BundlesToLoad, {}, false, FStreamableDelegate(), FStreamableManager::AsyncLoadHighPriority);
	}

	TSharedPtr<FStreamableHandle> RawLoadHandle = nullptr;
	if (RawAssetList.Num() > 0)
	{
		RawLoadHandle = AssetManager.LoadAssetList(RawAssetList.Array(), FStreamableDelegate(), FStreamableManager::AsyncLoadHighPriority, TEXT("StartExperienceLoad()"));
	}

	// Bundle 与 Raw 异步加载并行时合并 Handle
	TSharedPtr<FStreamableHandle> Handle = nullptr;
	if (BundleLoadHandle.IsValid() && RawLoadHandle.IsValid())
	{
		Handle = AssetManager.GetStreamableManager().CreateCombinedHandle({BundleLoadHandle, RawLoadHandle});
	}
	else
	{
		Handle = BundleLoadHandle.IsValid() ? BundleLoadHandle : RawLoadHandle;
	}

	FStreamableDelegate OnAssetsLoadedDelegate = FStreamableDelegate::CreateUObject(this, &ThisClass::OnExperienceLoadComplete);
	if (!Handle.IsValid() || Handle->HasLoadCompleted())
	{
		// 资源已在内存：立即触发完成委托
		FStreamableHandle::ExecuteDelegate(OnAssetsLoadedDelegate);
	}
	else
	{
		Handle->BindCompleteDelegate(OnAssetsLoadedDelegate);

		Handle->BindCancelDelegate(FStreamableDelegate::CreateLambda([OnAssetsLoadedDelegate]()
		{
			OnAssetsLoadedDelegate.ExecuteIfBound();
		}));
	}

	// 预加载列表：不阻塞 Experience 启动
	//@TODO: 明确需预加载的 PrimaryAsset 集合（非阻塞）
	if (const TSet<FPrimaryAssetId> PreloadAssetList; PreloadAssetList.Num() > 0)
	{
		AssetManager.ChangeBundleStateForPrimaryAssets(PreloadAssetList.Array(), BundlesToLoad, {});
	}
}

void UModularExperienceComponent::OnExperienceLoadComplete()
{
	check(LoadState == EModularExperienceLoadState::Loading);
	check(CurrentExperience != nullptr);

	UE_LOG(LogModularGameplayExperiences,
	       Log,
	       TEXT("EXPERIENCE: OnExperienceLoadComplete(CurrentExperience = %s, %s)"),
	       *CurrentExperience->GetPrimaryAssetId().ToString(),
	       *GetClientServerContextString(this));

	// 收集 GameFeaturePlugin URL（去重；无效映射则 ensure 并跳过）
	GameFeaturePluginURLs.Reset();

	auto CollectGameFeaturePluginURLs = [This=this](const UPrimaryDataAsset* Context, const TArray<FString>& FeaturePluginList)
	{
		for (const FString& PluginName : FeaturePluginList)
		{
			FString PluginURL;
			if (UGameFeaturesSubsystem::Get().GetPluginURLByName(PluginName, /*out*/ PluginURL))
			{
				This->GameFeaturePluginURLs.AddUnique(PluginURL);
			}
			else
			{
				ensureMsgf(false, TEXT("OnExperienceLoadComplete failed to find plugin URL from PluginName %s for experience %s - fix data, ignoring for this run"), *PluginName, *Context->GetPrimaryAssetId().ToString());
			}
		}

		// 		// Add in our extra plugin
		// 		if (!CurrentPlaylistData->GameFeaturePluginToActivateUntilDownloadedContentIsPresent.IsEmpty())
		// 		{
		// 			FString PluginURL;
		// 			if (UGameFeaturesSubsystem::Get().GetPluginURLByName(CurrentPlaylistData->GameFeaturePluginToActivateUntilDownloadedContentIsPresent, PluginURL))
		// 			{
		// 				GameFeaturePluginURLs.AddUnique(PluginURL);
		// 			}
		// 		}

		//@todo 可按 Playlist/DLC 策略追加「下载完成前临时激活」的 PluginURL
	};

	CollectGameFeaturePluginURLs(CurrentExperience, CurrentExperience->GameFeaturesToEnable);
	for (const TObjectPtr<UModularExperienceActionSet>& ActionSet : CurrentExperience->ActionSets)
	{
		if (ActionSet != nullptr)
		{
			CollectGameFeaturePluginURLs(ActionSet, ActionSet->GameFeaturesToEnable);
		}
	}

	// 收集 WorldSettings ActionSets 声明的 GameFeature 依赖
	if (const AModularWorldSettings* WorldSettings = Cast<AModularWorldSettings>(GetWorld()->GetWorldSettings()))
	{
		for (const TObjectPtr<UModularExperienceActionSet>& ActionSet : WorldSettings->ActionSets)
		{
			if (ActionSet != nullptr)
			{
				CollectGameFeaturePluginURLs(ActionSet, ActionSet->GameFeaturesToEnable);
			}
		}
	}

	// 加载并激活 GameFeaturePlugin
	NumGameFeaturePluginsLoading = GameFeaturePluginURLs.Num();
	if (NumGameFeaturePluginsLoading > 0)
	{
		LoadState = EModularExperienceLoadState::LoadingGameFeatures;
		for (const FString& PluginURL : GameFeaturePluginURLs)
		{
			UModularExperienceManager::NotifyOfPluginActivation(PluginURL);
			UGameFeaturesSubsystem::Get().LoadAndActivateGameFeaturePlugin(PluginURL, FGameFeaturePluginLoadComplete::CreateUObject(this, &ThisClass::OnGameFeaturePluginLoadComplete));
		}
	}
	else
	{
		OnExperienceFullLoadCompleted();
	}
}

void UModularExperienceComponent::OnGameFeaturePluginLoadComplete(const UE::GameFeatures::FResult& Result)
{
	// 仍在加载的插件计数 -1
	NumGameFeaturePluginsLoading--;

	if (NumGameFeaturePluginsLoading == 0)
	{
		OnExperienceFullLoadCompleted();
	}
}

void UModularExperienceComponent::OnExperienceFullLoadCompleted()
{
	check(LoadState != EModularExperienceLoadState::Loaded);

	// Chaos 测试：可配置随机延迟
	if (LoadState != EModularExperienceLoadState::LoadingChaosTestingDelay)
	{
		const float DelaySecs = ModularGameplayExperiencesConsoleVariables::GetExperienceLoadDelayDuration();
		if (DelaySecs > 0.0f)
		{
			FTimerHandle DummyHandle;

			LoadState = EModularExperienceLoadState::LoadingChaosTestingDelay;
			GetWorld()->GetTimerManager().SetTimer(DummyHandle, this, &ThisClass::OnExperienceFullLoadCompleted, DelaySecs, /*bLooping=*/ false);

			return;
		}
	}

	LoadState = EModularExperienceLoadState::ExecutingActions;

	// 执行 GameFeatureAction 激活阶段
	FGameFeatureActivatingContext Context;

	// 绑定当前 WorldContext，限制 Action 生效范围
	if (const FWorldContext* ExistingWorldContext = GEngine->GetWorldContextFromWorld(GetWorld()))
	{
		Context.SetRequiredWorldContextHandle(ExistingWorldContext->ContextHandle);
	}

	auto ActivateListOfActions = [&Context](const TArray<UGameFeatureAction*>& ActionList)
	{
		for (UGameFeatureAction* Action : ActionList)
		{
			if (Action != nullptr)
			{
				//@TODO 这些 API 不含 UWorld，在 Client-Server PIE 下可能有边界问题；与 GameplayTag 等「进程级注册 + World 内应用」模式一致
				Action->OnGameFeatureRegistering();
				Action->OnGameFeatureLoading();
				Action->OnGameFeatureActivating(Context);
			}
		}
	};

	ActivateListOfActions(CurrentExperience->Actions);
	for (const TObjectPtr<UModularExperienceActionSet>& ActionSet : CurrentExperience->ActionSets)
	{
		if (ActionSet != nullptr)
		{
			ActivateListOfActions(ActionSet->Actions);
		}
	}

	// 激活 WorldSettings 上直接配置的 Actions 及 ActionSets
	if (const AModularWorldSettings* WorldSettings = Cast<AModularWorldSettings>(GetWorld()->GetWorldSettings()))
	{
		ActivateListOfActions(WorldSettings->Actions);
		for (const TObjectPtr<UModularExperienceActionSet>& ActionSet : WorldSettings->ActionSets)
		{
			if (ActionSet != nullptr)
			{
				ActivateListOfActions(ActionSet->Actions);
			}
		}
	}

	LoadState = EModularExperienceLoadState::Loaded;

	OnExperienceLoaded_HighPriority.Broadcast(CurrentExperience);
	OnExperienceLoaded_HighPriority.Clear();

	OnExperienceLoaded.Broadcast(CurrentExperience);
	OnExperienceLoaded.Clear();

	OnExperienceLoaded_LowPriority.Broadcast(CurrentExperience);
	OnExperienceLoaded_LowPriority.Clear();

	// Scalability：按 Experience 指定档位（项目层实现）
	//@todo Experience 资产中声明 Scalability 组
#if !UE_SERVER
	//UModularSettingsLocal::Get()->OnExperienceLoaded();
#endif
}

void UModularExperienceComponent::OnActionDeactivationCompleted()
{
	check(IsInGameThread());
	++NumObservedPausers;

	if (NumObservedPausers == NumExpectedPausers)
	{
		OnAllActionsDeactivated();
	}
}

void UModularExperienceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, CurrentExperience);
}

void UModularExperienceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// 停用本 Experience 激活的 GameFeaturePlugin
	//@TODO: 与激活顺序对称的 FILO 停用
	for (const FString& PluginURL : GameFeaturePluginURLs)
	{
		if (UModularExperienceManager::RequestToDeactivatePlugin(PluginURL))
		{
			UGameFeaturesSubsystem::Get().DeactivateGameFeaturePlugin(PluginURL);
		}
	}

	//@TODO: 正确处理「仅部分加载」的清理
	if (LoadState == EModularExperienceLoadState::Loaded)
	{
		LoadState = EModularExperienceLoadState::Deactivating;

		// 防止 Pauser 立即触发导致过早结束过渡
		NumExpectedPausers = INDEX_NONE;
		NumObservedPausers = 0;

		// 执行 GameFeatureAction 停用 / 卸载
		FGameFeatureDeactivatingContext Context(TEXT(""), [this](FStringView) { this->OnActionDeactivationCompleted(); });

		if (const FWorldContext* ExistingWorldContext = GEngine->GetWorldContextFromWorld(GetWorld()))
		{
			Context.SetRequiredWorldContextHandle(ExistingWorldContext->ContextHandle);
		}

		auto DeactivateListOfActions = [&Context](const TArray<UGameFeatureAction*>& ActionList)
		{
			for (UGameFeatureAction* Action : ActionList)
			{
				if (Action)
				{
					Action->OnGameFeatureDeactivating(Context);
					Action->OnGameFeatureUnregistering();
				}
			}
		};

		DeactivateListOfActions(CurrentExperience->Actions);
		for (const TObjectPtr<UModularExperienceActionSet>& ActionSet : CurrentExperience->ActionSets)
		{
			if (ActionSet != nullptr)
			{
				DeactivateListOfActions(ActionSet->Actions);
			}
		}

		// 停用 WorldSettings 上直接配置的 Actions 及 ActionSets
		if (const AModularWorldSettings* WorldSettings = Cast<AModularWorldSettings>(GetWorld()->GetWorldSettings()))
		{
			DeactivateListOfActions(WorldSettings->Actions);
			for (const TObjectPtr<UModularExperienceActionSet>& ActionSet : WorldSettings->ActionSets)
			{
				if (ActionSet != nullptr)
				{
					DeactivateListOfActions(ActionSet->Actions);
				}
			}
		}

		NumExpectedPausers = Context.GetNumPausers();

		if (NumExpectedPausers > 0)
		{
			UE_LOG(LogModularGameplayExperiences, Error, TEXT("Actions that have asynchronous deactivation aren't fully supported yet in Lyra experiences"));
		}

		if (NumExpectedPausers == NumObservedPausers)
		{
			OnAllActionsDeactivated();
		}
	}
}

bool UModularExperienceComponent::ShouldShowLoadingScreen(FString& OutReason) const
{
	if (LoadState != EModularExperienceLoadState::Loaded)
	{
		OutReason = TEXT("Experience 仍在加载");
		return true;
	}
	else
	{
		return false;
	}
}

void UModularExperienceComponent::OnAllActionsDeactivated()
{
	//@TODO 当前仅 Deactivate，未完整 Unload Plugin/资源差分
	LoadState         = EModularExperienceLoadState::Unloaded;
	CurrentExperience = nullptr;
	//@TODO 按需 ForceGarbageCollection
}
