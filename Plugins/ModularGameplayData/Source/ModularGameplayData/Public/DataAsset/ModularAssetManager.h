// Copyright Chronicler.

#pragma once

#include "Engine/AssetManager.h"
#include "ModularAssetManagerStartupJob.h"
#include "ModularGameData.h"
#include "ModularPawnData.h"
#include "Templates/SubclassOf.h"
#include "ModularAssetManager.generated.h"

/**
 * UModularAssetManager
 *
 *	项目侧 UAssetManager 实现：扩展加载逻辑并集中存放游戏侧 Primary Data 类型。
 *	多数项目会重载 AssetManager，便于统一 Primary Asset 与加载策略。
 *	在 DefaultEngine.ini 中设置 AssetManagerClassName 指向本类。
 */
UCLASS(Config="Game")
class MODULARGAMEPLAYDATA_API UModularAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:

	UModularAssetManager();

	/** 返回 UAssetManager 单例（实际类型为本类）。 */
	static UModularAssetManager& Get();

	/**
	 * 根据 TSoftObjectPtr 解析资源；若未加载则同步加载。
	 * @param bKeepInMemory 为 true 时将资源登记到本 Manager 的常驻集合。
	 */
	template<typename AssetType>
	static AssetType* GetAsset(const TSoftObjectPtr<AssetType>& AssetPointer, bool bKeepInMemory = true);

	/**
	 * 根据 TSoftClassPtr 解析 UClass；若未加载则同步加载。
	 * @param bKeepInMemory 为 true 时将 Class 登记到常驻集合。
	 */
	template<typename AssetType>
	static TSubclassOf<AssetType> GetSubclass(const TSoftClassPtr<AssetType>& AssetPointer, bool bKeepInMemory = true);

	/** 将当前由本 Manager 跟踪、已加载的资源 Dump 到日志。 */
	static void DumpLoadedAssets();

	/** 获取全局 ModularGameData（会按需加载）。 */
	virtual const UModularGameData& GetModularGameData();
	/** 获取默认 PawnData（PlayerState 未指定时使用）。 */
	virtual const UModularPawnData* GetDefaultPawnData();

	/**
	 * 从 GameDataMap 取已缓存的指定类 GameData，否则按 DataPath 同步加载并注册。
	 */
	template <typename GameDataClass>
	const GameDataClass& GetOrLoadTypedGameData(const TSoftObjectPtr<GameDataClass>& DataPath)
	{
		if (TObjectPtr<UPrimaryDataAsset> const * PResult = GameDataMap.Find(GameDataClass::StaticClass()))
		{
			return *CastChecked<GameDataClass>(*PResult);
		}

		// 需要时阻塞加载对应 GameData
		return *CastChecked<const GameDataClass>(LoadGameDataOfClass(GameDataClass::StaticClass(), DataPath, GameDataClass::StaticClass()->GetFName()));
	}

protected:
	// 同步解析 SoftObjectPath（优先 StreamableManager，未初始化则 TryLoad）。
	static UObject* SynchronousLoadAsset(const FSoftObjectPath& AssetPath);
	// 命令行 -LogAssetLoads 时打印单次同步加载耗时。
	static bool ShouldLogAssetLoads();

	// 线程安全：将已加载资源加入常驻跟踪集合。
	virtual void AddLoadedAsset(const UObject* Asset);

	//~ UAssetManager 接口
	virtual void StartInitialLoading() override;
#if WITH_EDITOR
	virtual void PreBeginPIE(bool bStartSimulate) override;
#endif
	//~ UAssetManager 接口结束

	/** 加载指定 PrimaryAssetType 的 GameData 并写入 GameDataMap（失败则 Fatal）。 */
	virtual UPrimaryDataAsset* LoadGameDataOfClass(TSubclassOf<UPrimaryDataAsset> DataClass, const TSoftObjectPtr<UPrimaryDataAsset>& DataClassPath, FPrimaryAssetType PrimaryAssetType);

protected:

	/** Config：全局 ModularGameData 的 Soft 路径。 */
	UPROPERTY(Config)
	TSoftObjectPtr<UModularGameData> ModularGameDataPath;

	/** 已加载的各类 Primary GameData（按 UClass 索引）。 */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UClass>, TObjectPtr<UPrimaryDataAsset>> GameDataMap;

	/** Config：默认 PawnData（PlayerState 未设置时使用）。 */
	UPROPERTY(Config)
	TSoftObjectPtr<UModularPawnData> DefaultPawnData;

private:
	// 顺序执行 StartupJobs 队列中的启动任务。
	virtual void DoAllStartupJobs();

	// 加载进度回调；可接到 Loading Screen 或自定义 UI。
	static void UpdateInitialGameContentLoadPercent(float GameContentPercent);

	// 启动阶段任务列表（含权重，用于汇总进度）。
	TArray<FModularAssetManagerStartupJob> StartupJobs;

private:
	
	/** 由本 Manager 跟踪、可选常驻内存的已加载资源。 */
	UPROPERTY()
	TSet<TObjectPtr<const UObject>> LoadedAssets;

	// 修改 LoadedAssets 时的临界区。
	FCriticalSection LoadedAssetsCritical;
};


template<typename AssetType>
AssetType* UModularAssetManager::GetAsset(const TSoftObjectPtr<AssetType>& AssetPointer, bool bKeepInMemory)
{
	AssetType* LoadedAsset = nullptr;

	if (const FSoftObjectPath& AssetPath = AssetPointer.ToSoftObjectPath(); AssetPath.IsValid())
	{
		LoadedAsset = AssetPointer.Get();
		if (!LoadedAsset)
		{
			LoadedAsset = Cast<AssetType>(SynchronousLoadAsset(AssetPath));
			ensureAlwaysMsgf(LoadedAsset, TEXT("Failed to load asset [%s]"), *AssetPointer.ToString());
		}

		if (LoadedAsset && bKeepInMemory)
		{
			// 登记到已加载资源集合
			Get().AddLoadedAsset(Cast<UObject>(LoadedAsset));
		}
	}

	return LoadedAsset;
}

template<typename AssetType>
TSubclassOf<AssetType> UModularAssetManager::GetSubclass(const TSoftClassPtr<AssetType>& AssetPointer, bool bKeepInMemory)
{
	TSubclassOf<AssetType> LoadedSubclass;

	if (const FSoftObjectPath& AssetPath = AssetPointer.ToSoftObjectPath(); AssetPath.IsValid())
	{
		LoadedSubclass = AssetPointer.Get();
		if (!LoadedSubclass)
		{
			LoadedSubclass = Cast<UClass>(SynchronousLoadAsset(AssetPath));
			ensureAlwaysMsgf(LoadedSubclass, TEXT("Failed to load asset class [%s]"), *AssetPointer.ToString());
		}

		if (LoadedSubclass && bKeepInMemory)
		{
			// 登记到已加载资源集合
			Get().AddLoadedAsset(Cast<UObject>(LoadedSubclass));
		}
	}

	return LoadedSubclass;
}
