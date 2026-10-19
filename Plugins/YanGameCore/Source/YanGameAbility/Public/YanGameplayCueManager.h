#pragma once

#include "GameplayCueManager.h"

#include "YanGameplayCueManager.generated.h"

#define UE_API YANGAMEABILITY_API

class UClass;
class UWorld;
struct FObjectKey;

/**
 * 项目级 GameplayCue 管理器。
 *
 * 客户端不再一次性加载全部 Cue，而是在 Cue 标签被内容引用时异步预载，
 * 以此避免首次触发时的同步加载卡顿；专用服务器仍走一次性加载。
 * 需在 DefaultGame.ini 的 /Script/GameplayAbilities.AbilitySystemGlobals
 * 下以 GlobalGameplayCueManagerClass 指向本类方可生效。
 */
UCLASS(MinimalAPI)
class UYanGameplayCueManager : public UGameplayCueManager
{
	GENERATED_BODY()

public:
	UE_API UYanGameplayCueManager(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static UE_API UYanGameplayCueManager* Get();

	//~Begin UGameplayCueManager Interface
	UE_API virtual void OnCreated() override;
	UE_API virtual bool ShouldAsyncLoadRuntimeObjectLibraries() const override;
	UE_API virtual bool ShouldSyncLoadMissingGameplayCues() const override;
	UE_API virtual bool ShouldAsyncLoadMissingGameplayCues() const override;
	//~End UGameplayCueManager Interface

	/** 控制台命令入口，列出当前驻留内存的 Cue 类；传入 Refs 参数可一并列出引用者。 */
	static UE_API void DumpGameplayCues(const TArray<FString>& Args);

	/** 延迟加载模式下，预载那些无论如何都必须常驻的 Cue。 */
	UE_API void LoadAlwaysLoadedCues();

	/** 将运行时 Cue 集合的软引用刷新到动态 PrimaryAsset 的 Client bundle 上。 */
	UE_API void RefreshGameplayCuePrimaryAsset();

private:
	void OnGameplayTagLoaded(const FGameplayTag& Tag);
	void HandlePostGarbageCollect();
	void ProcessLoadedTags();
	void ProcessTagToPreload(const FGameplayTag& Tag, UObject* OwningObject);
	void OnPreloadCueComplete(FSoftObjectPath Path, TWeakObjectPtr<UObject> OwningObject, bool bAlwaysLoadedCue);
	void RegisterPreloadedCue(UClass* LoadedGameplayCueClass, UObject* OwningObject);
	void HandlePostLoadMap(UWorld* NewWorld);
	void UpdateDelayLoadDelegateListeners();
	bool ShouldDelayLoadGameplayCues() const;

	// 标签加载发生在异步加载线程，需连同当时正在序列化的属主一并记录，回到游戏线程再处理
	struct FLoadedGameplayTagToProcessData
	{
		FGameplayTag            Tag;
		TWeakObjectPtr<UObject> WeakOwner;

		FLoadedGameplayTagToProcessData() {}

		FLoadedGameplayTagToProcessData(const FGameplayTag& InTag, const TWeakObjectPtr<UObject>& InWeakOwner)
			: Tag(InTag)
			  , WeakOwner(InWeakOwner)
		{}
	};

	// 因被内容引用而在客户端预载的 Cue，属主全部失效后可随 GC 释放
	UPROPERTY(transient)
	TSet<TObjectPtr<UClass>> PreloadedCues;

	TMap<FObjectKey, TSet<FObjectKey>> PreloadedCueReferencers;

	// 代码引用或显式声明的常驻 Cue，不随属主生命周期释放
	UPROPERTY(transient)
	TSet<TObjectPtr<UClass>> AlwaysLoadedCues;

	TArray<FLoadedGameplayTagToProcessData> LoadedGameplayTagsToProcess;
	FCriticalSection                        LoadedGameplayTagsToProcessCS;
	bool                                    bProcessLoadedTagsAfterGC = false;
};

#undef UE_API
