#include "YanGameplayCueManager.h"

#include "AbilitySystemGlobals.h"
#include "Async/Async.h"
#include "Engine/AssetManager.h"
#include "GameplayCueSet.h"
#include "GameplayTagsManager.h"
#include "ModularGameplayAbilitiesLogChannels.h"
#include "UObject/UObjectThreadContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanGameplayCueManager)

namespace
{
	enum class EYanCueLoadMode
	{
		// 启动时全量加载：编辑器加载慢，但 PIE 进入快且效果不会缺失
		LoadUpfront,

		// 打包运行时按引用异步预载；编辑器下退化为触发时才加载（迭代快，但 PIE 可能看不到效果）
		PreloadAsCuesAreReferenced_GameOnly,

		// 始终按引用异步预载
		PreloadAsCuesAreReferenced
	};

	constexpr EYanCueLoadMode LoadMode = EYanCueLoadMode::LoadUpfront;

	FAutoConsoleCommand CVarDumpGameplayCues(
		TEXT("Yan.DumpGameplayCues"),
		TEXT("列出经 YanGameplayCueManager 加载且当前驻留内存的 GameplayCue 资产。"),
		FConsoleCommandWithArgsDelegate::CreateStatic(UYanGameplayCueManager::DumpGameplayCues));

	// 标签加载回调可能发生在异步加载线程，借图任务跳回游戏线程
	struct FYanGameplayCueTagSynchronizeGraphTask : public FAsyncGraphTaskBase
	{
		TFunction<void()> TheTask;

		explicit FYanGameplayCueTagSynchronizeGraphTask(TFunction<void()>&& Task)
			: TheTask(MoveTemp(Task))
		{}

		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) { TheTask(); }

		ENamedThreads::Type GetDesiredThread() { return ENamedThreads::GameThread; }
	};

	const FPrimaryAssetType GameplayCueRefsType = TEXT("GameplayCueRefs");
	const FName             GameplayCueRefsName = TEXT("GameplayCueReferences");
	const FName             LoadStateClient     = TEXT("Client");
}

UYanGameplayCueManager::UYanGameplayCueManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

UYanGameplayCueManager* UYanGameplayCueManager::Get()
{
	return Cast<UYanGameplayCueManager>(UAbilitySystemGlobals::Get().GetGameplayCueManager());
}

void UYanGameplayCueManager::OnCreated()
{
	Super::OnCreated();

	UpdateDelayLoadDelegateListeners();
}

void UYanGameplayCueManager::LoadAlwaysLoadedCues()
{
	if (!ShouldDelayLoadGameplayCues())
	{
		return;
	}

	UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();

	// 需常驻的 Cue 标签在此登记；留空表示全部走按需预载
	const TArray<FName> AdditionalAlwaysLoadedCueTags;

	for (const FName& CueTagName : AdditionalAlwaysLoadedCueTags)
	{
		const FGameplayTag CueTag = TagManager.RequestGameplayTag(CueTagName, /*ErrorIfNotFound=*/ false);
		if (CueTag.IsValid())
		{
			ProcessTagToPreload(CueTag, nullptr);
		}
		else
		{
			UE_LOG(LogModularGameplayAbilities, Warning, TEXT("常驻 Cue 标签列表含无效标签 %s"), *CueTagName.ToString());
		}
	}
}

bool UYanGameplayCueManager::ShouldAsyncLoadRuntimeObjectLibraries() const
{
	switch (LoadMode)
	{
		case EYanCueLoadMode::LoadUpfront:
			return true;
		case EYanCueLoadMode::PreloadAsCuesAreReferenced_GameOnly:
#if WITH_EDITOR
			if (GIsEditor)
			{
				return false;
			}
#endif
			break;
		case EYanCueLoadMode::PreloadAsCuesAreReferenced:
			break;
	}

	return !ShouldDelayLoadGameplayCues();
}

bool UYanGameplayCueManager::ShouldSyncLoadMissingGameplayCues() const
{
	return false;
}

bool UYanGameplayCueManager::ShouldAsyncLoadMissingGameplayCues() const
{
	return true;
}

void UYanGameplayCueManager::DumpGameplayCues(const TArray<FString>& Args)
{
	UYanGameplayCueManager* CueManager = Get();
	if (!CueManager)
	{
		UE_LOG(LogModularGameplayAbilities, Error, TEXT("DumpGameplayCues 失败：未找到 UYanGameplayCueManager"));
		return;
	}

	const bool bIncludeRefs = Args.Contains(TEXT("Refs"));

	UE_LOG(LogModularGameplayAbilities, Log, TEXT("=========== 常驻 GameplayCue ==========="));
	for (UClass* CueClass : CueManager->AlwaysLoadedCues)
	{
		UE_LOG(LogModularGameplayAbilities, Log, TEXT("  %s"), *GetPathNameSafe(CueClass));
	}

	UE_LOG(LogModularGameplayAbilities, Log, TEXT("=========== 预载 GameplayCue ==========="));
	for (UClass* CueClass : CueManager->PreloadedCues)
	{
		const TSet<FObjectKey>* ReferencerSet = CueManager->PreloadedCueReferencers.Find(CueClass);
		const int32             NumRefs       = ReferencerSet ? ReferencerSet->Num() : 0;
		UE_LOG(LogModularGameplayAbilities, Log, TEXT("  %s (%d 个引用)"), *GetPathNameSafe(CueClass), NumRefs);

		if (bIncludeRefs && ReferencerSet)
		{
			for (const FObjectKey& Ref : *ReferencerSet)
			{
				UE_LOG(LogModularGameplayAbilities, Log, TEXT("    ^- %s"), *GetPathNameSafe(Ref.ResolveObjectPtr()));
			}
		}
	}

	UE_LOG(LogModularGameplayAbilities, Log, TEXT("=========== 按需加载的 GameplayCue ==========="));
	int32 NumMissingCuesLoaded = 0;
	if (CueManager->RuntimeGameplayCueObjectLibrary.CueSet)
	{
		for (const FGameplayCueNotifyData& CueData : CueManager->RuntimeGameplayCueObjectLibrary.CueSet->GameplayCueData)
		{
			if (CueData.LoadedGameplayCueClass
				&& !CueManager->AlwaysLoadedCues.Contains(CueData.LoadedGameplayCueClass)
				&& !CueManager->PreloadedCues.Contains(CueData.LoadedGameplayCueClass))
			{
				++NumMissingCuesLoaded;
				UE_LOG(LogModularGameplayAbilities, Log, TEXT("  %s"), *CueData.LoadedGameplayCueClass->GetPathName());
			}
		}
	}

	UE_LOG(LogModularGameplayAbilities, Log, TEXT("=========== 汇总 ==========="));
	UE_LOG(LogModularGameplayAbilities, Log, TEXT("  ... 常驻 %d 个"), CueManager->AlwaysLoadedCues.Num());
	UE_LOG(LogModularGameplayAbilities, Log, TEXT("  ... 预载 %d 个"), CueManager->PreloadedCues.Num());
	UE_LOG(LogModularGameplayAbilities, Log, TEXT("  ... 按需 %d 个"), NumMissingCuesLoaded);
	UE_LOG(LogModularGameplayAbilities, Log, TEXT("  ... 合计 %d 个"), CueManager->AlwaysLoadedCues.Num() + CueManager->PreloadedCues.Num() + NumMissingCuesLoaded);
}

void UYanGameplayCueManager::OnGameplayTagLoaded(const FGameplayTag& Tag)
{
	FScopeLock ScopeLock(&LoadedGameplayTagsToProcessCS);

	const bool                bStartTask   = LoadedGameplayTagsToProcess.Num() == 0;
	FUObjectSerializeContext* LoadContext  = FUObjectThreadContext::Get().GetSerializeContext();
	UObject*                  OwningObject = LoadContext ? LoadContext->SerializedObject : nullptr;

	LoadedGameplayTagsToProcess.Emplace(Tag, OwningObject);

	if (bStartTask)
	{
		TGraphTask<FYanGameplayCueTagSynchronizeGraphTask>::CreateTask().ConstructAndDispatchWhenReady([]()
		{
			if (!GIsRunning)
			{
				return;
			}

			if (UYanGameplayCueManager* StrongThis = Get())
			{
				// GC 期间禁止 StaticFindObject 一类调用，推迟到 GC 结束后处理
				if (IsGarbageCollecting())
				{
					StrongThis->bProcessLoadedTagsAfterGC = true;
				}
				else
				{
					StrongThis->ProcessLoadedTags();
				}
			}
		});
	}
}

void UYanGameplayCueManager::HandlePostGarbageCollect()
{
	if (bProcessLoadedTagsAfterGC)
	{
		ProcessLoadedTags();
	}

	bProcessLoadedTagsAfterGC = false;
}

void UYanGameplayCueManager::ProcessLoadedTags()
{
	TArray<FLoadedGameplayTagToProcessData> TaskLoadedGameplayTagsToProcess;
	{
		// 只在拷贝与清空期间持锁
		FScopeLock TaskScopeLock(&LoadedGameplayTagsToProcessCS);
		TaskLoadedGameplayTagsToProcess = LoadedGameplayTagsToProcess;
		LoadedGameplayTagsToProcess.Empty();
	}

	// 关闭流程中可能走到这里，此时不应继续
	if (!GIsRunning)
	{
		return;
	}

	if (!RuntimeGameplayCueObjectLibrary.CueSet)
	{
		UE_LOG(LogModularGameplayAbilities, Warning, TEXT("处理已加载 Cue 标签时 RuntimeGameplayCueObjectLibrary.CueSet 为空，跳过"));
		return;
	}

	for (const FLoadedGameplayTagToProcessData& LoadedTagData : TaskLoadedGameplayTagsToProcess)
	{
		if (RuntimeGameplayCueObjectLibrary.CueSet->GameplayCueDataMap.Contains(LoadedTagData.Tag) && !LoadedTagData.WeakOwner.IsStale())
		{
			ProcessTagToPreload(LoadedTagData.Tag, LoadedTagData.WeakOwner.Get());
		}
	}
}

void UYanGameplayCueManager::ProcessTagToPreload(const FGameplayTag& Tag, UObject* OwningObject)
{
	switch (LoadMode)
	{
		case EYanCueLoadMode::LoadUpfront:
			return;
		case EYanCueLoadMode::PreloadAsCuesAreReferenced_GameOnly:
#if WITH_EDITOR
			if (GIsEditor)
			{
				return;
			}
#endif
			break;
		case EYanCueLoadMode::PreloadAsCuesAreReferenced:
			break;
	}

	check(RuntimeGameplayCueObjectLibrary.CueSet);

	const int32* DataIdx = RuntimeGameplayCueObjectLibrary.CueSet->GameplayCueDataMap.Find(Tag);
	if (!DataIdx || !RuntimeGameplayCueObjectLibrary.CueSet->GameplayCueData.IsValidIndex(*DataIdx))
	{
		return;
	}

	const FGameplayCueNotifyData& CueData = RuntimeGameplayCueObjectLibrary.CueSet->GameplayCueData[*DataIdx];

	if (UClass* LoadedGameplayCueClass = FindObject<UClass>(nullptr, *CueData.GameplayCueNotifyObj.ToString()))
	{
		RegisterPreloadedCue(LoadedGameplayCueClass, OwningObject);
	}
	else
	{
		// 无属主意味着由代码而非内容引用，视为常驻
		const bool                    bAlwaysLoadedCue = OwningObject == nullptr;
		const TWeakObjectPtr<UObject> WeakOwner        = OwningObject;

		StreamableManager.RequestAsyncLoad(
			CueData.GameplayCueNotifyObj,
			FStreamableDelegate::CreateUObject(this, &ThisClass::OnPreloadCueComplete, CueData.GameplayCueNotifyObj, WeakOwner, bAlwaysLoadedCue),
			FStreamableManager::DefaultAsyncLoadPriority, false, false, TEXT("GameplayCueManager"));
	}
}

void UYanGameplayCueManager::OnPreloadCueComplete(FSoftObjectPath Path, TWeakObjectPtr<UObject> OwningObject, bool bAlwaysLoadedCue)
{
	if (bAlwaysLoadedCue || OwningObject.IsValid())
	{
		if (UClass* LoadedGameplayCueClass = Cast<UClass>(Path.ResolveObject()))
		{
			RegisterPreloadedCue(LoadedGameplayCueClass, OwningObject.Get());
		}
	}
}

void UYanGameplayCueManager::RegisterPreloadedCue(UClass* LoadedGameplayCueClass, UObject* OwningObject)
{
	check(LoadedGameplayCueClass);

	const bool bAlwaysLoadedCue = OwningObject == nullptr;
	if (bAlwaysLoadedCue)
	{
		AlwaysLoadedCues.Add(LoadedGameplayCueClass);
		PreloadedCues.Remove(LoadedGameplayCueClass);
		PreloadedCueReferencers.Remove(LoadedGameplayCueClass);
	}
	else if ((OwningObject != LoadedGameplayCueClass)
		&& (OwningObject != LoadedGameplayCueClass->GetDefaultObject())
		&& !AlwaysLoadedCues.Contains(LoadedGameplayCueClass))
	{
		PreloadedCues.Add(LoadedGameplayCueClass);
		PreloadedCueReferencers.FindOrAdd(LoadedGameplayCueClass).Add(OwningObject);
	}
}

void UYanGameplayCueManager::HandlePostLoadMap(UWorld* NewWorld)
{
	// 已由本管理器持有强引用的类无需再挂在 CueSet 上，避免重复保活
	if (RuntimeGameplayCueObjectLibrary.CueSet)
	{
		for (UClass* CueClass : AlwaysLoadedCues)
		{
			RuntimeGameplayCueObjectLibrary.CueSet->RemoveLoadedClass(CueClass);
		}

		for (UClass* CueClass : PreloadedCues)
		{
			RuntimeGameplayCueObjectLibrary.CueSet->RemoveLoadedClass(CueClass);
		}
	}

	// 属主已全部销毁的预载 Cue 可以释放
	for (auto CueIt = PreloadedCues.CreateIterator(); CueIt; ++CueIt)
	{
		TSet<FObjectKey>& ReferencerSet = PreloadedCueReferencers.FindChecked(*CueIt);
		for (auto RefIt = ReferencerSet.CreateIterator(); RefIt; ++RefIt)
		{
			if (!RefIt->ResolveObjectPtr())
			{
				RefIt.RemoveCurrent();
			}
		}

		if (ReferencerSet.Num() == 0)
		{
			PreloadedCueReferencers.Remove(*CueIt);
			CueIt.RemoveCurrent();
		}
	}
}

void UYanGameplayCueManager::UpdateDelayLoadDelegateListeners()
{
	UGameplayTagsManager::Get().OnGameplayTagLoadedDelegate.RemoveAll(this);
	FCoreUObjectDelegates::GetPostGarbageCollect().RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	switch (LoadMode)
	{
		case EYanCueLoadMode::LoadUpfront:
			return;
		case EYanCueLoadMode::PreloadAsCuesAreReferenced_GameOnly:
#if WITH_EDITOR
			if (GIsEditor)
			{
				return;
			}
#endif
			break;
		case EYanCueLoadMode::PreloadAsCuesAreReferenced:
			break;
	}

	UGameplayTagsManager::Get().OnGameplayTagLoadedDelegate.AddUObject(this, &ThisClass::OnGameplayTagLoaded);
	FCoreUObjectDelegates::GetPostGarbageCollect().AddUObject(this, &ThisClass::HandlePostGarbageCollect);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
}

bool UYanGameplayCueManager::ShouldDelayLoadGameplayCues() const
{
	// 专用服务器不播放 Cue，无需延迟加载机制
	return !IsRunningDedicatedServer();
}

void UYanGameplayCueManager::RefreshGameplayCuePrimaryAsset()
{
	TArray<FSoftObjectPath> CuePaths;
	if (UGameplayCueSet* RuntimeGameplayCueSet = GetRuntimeCueSet())
	{
		RuntimeGameplayCueSet->GetSoftObjectPaths(CuePaths);
	}

	FAssetBundleData BundleData;
	BundleData.AddBundleAssetsTruncated(LoadStateClient, CuePaths);

	const FPrimaryAssetId PrimaryAssetId = FPrimaryAssetId(GameplayCueRefsType, GameplayCueRefsName);
	UAssetManager::Get().AddDynamicAsset(PrimaryAssetId, FSoftObjectPath(), BundleData);
}
