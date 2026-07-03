// Copyright Chronicler.


#include "DataAsset/ModularAssetManager.h"
#include "ModularGameplayDataLogs.h"
#include "Misc/App.h"
#include "Stats/StatsMisc.h"
#include "Engine/Engine.h"
#include "Misc/ScopedSlowTask.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularAssetManager)

//////////////////////////////////////////////////////////////////////

static FAutoConsoleCommand CVarDumpLoadedAssets(
	TEXT("ModularGameData.DumpLoadedAssets"),
	TEXT("列出当前由 AssetManager 路径加载并仍驻留内存的资源。"),
	FConsoleCommandDelegate::CreateStatic(UModularAssetManager::DumpLoadedAssets)
);

//////////////////////////////////////////////////////////////////////

#define STARTUP_JOB_WEIGHTED(JobFunc, JobWeight) StartupJobs.Add(FModularAssetManagerStartupJob(#JobFunc, [this](const FModularAssetManagerStartupJob& StartupJob, TSharedPtr<FStreamableHandle>& LoadHandle){JobFunc;}, JobWeight))
#define STARTUP_JOB(JobFunc) STARTUP_JOB_WEIGHTED(JobFunc, 1.f)

//////////////////////////////////////////////////////////////////////

UModularAssetManager::UModularAssetManager()
{
	DefaultPawnData = nullptr;
}

UModularAssetManager& UModularAssetManager::Get()
{
	check(GEngine);

	if (UModularAssetManager* Singleton = Cast<UModularAssetManager>(GEngine->AssetManager))
	{
		return *Singleton;
	}

	UE_LOG(LogModularGameplayData, Fatal, TEXT("Invalid AssetManagerClassName in DefaultEngine.ini.  It must be set to ModularAssetManager!"));

	// 上述 Fatal 正常情况下不会执行到此处
	return *NewObject<UModularAssetManager>();
}

UObject* UModularAssetManager::SynchronousLoadAsset(const FSoftObjectPath& AssetPath)
{
	if (AssetPath.IsValid())
	{
		if (ShouldLogAssetLoads())
		{
			TUniquePtr<FScopeLogTime> LogTimePtr = MakeUnique<FScopeLogTime>(
				*FString::Printf(TEXT("Synchronously loaded asset [%s]"), *AssetPath.ToString()),
				nullptr,
				FScopeLogTime::ScopeLog_Seconds);
		}

		if (UAssetManager::IsInitialized())
		{
			return UAssetManager::GetStreamableManager().LoadSynchronous(AssetPath, false);
		}

		// AssetManager 尚未初始化时用 TryLoad 兜底
		return AssetPath.TryLoad();
	}

	return nullptr;
}

bool UModularAssetManager::ShouldLogAssetLoads()
{
	static bool bLogAssetLoads = FParse::Param(FCommandLine::Get(), TEXT("LogAssetLoads"));
	return bLogAssetLoads;
}

void UModularAssetManager::AddLoadedAsset(const UObject* Asset)
{
	if (ensureAlways(Asset))
	{
		FScopeLock LoadedAssetsLock(&LoadedAssetsCritical);
		LoadedAssets.Add(Asset);
	}
}

void UModularAssetManager::DumpLoadedAssets()
{
	UE_LOG(LogModularGameplayData, Log, TEXT("========== Start Dumping Loaded Assets =========="));

	for (const UObject* LoadedAsset : Get().LoadedAssets)
	{
		UE_LOG(LogModularGameplayData, Log, TEXT("  %s"), *GetNameSafe(LoadedAsset));
	}

	UE_LOG(LogModularGameplayData, Log, TEXT("... %d assets in loaded pool"), Get().LoadedAssets.Num());
	UE_LOG(LogModularGameplayData, Log, TEXT("========== Finish Dumping Loaded Assets =========="));
}

void UModularAssetManager::StartInitialLoading()
{
	SCOPED_BOOT_TIMING("UModularAssetManager::StartInitialLoading");

	// 完成 Primary Asset 扫描等基类逻辑；即使延迟加载也要在此处执行
	Super::StartInitialLoading();

	{
		// 可在此加入加载基础 GameData 的 STARTUP_JOB
		//STARTUP_JOB_WEIGHTED(GetGameData(), 25.f);
	}

	// 执行队列中的启动任务
	DoAllStartupJobs();
}

const UModularGameData& UModularAssetManager::GetModularGameData()
{
	return GetOrLoadTypedGameData<UModularGameData>(ModularGameDataPath);
}

const UModularPawnData* UModularAssetManager::GetDefaultPawnData()
{
	return GetAsset(DefaultPawnData);
}

UPrimaryDataAsset* UModularAssetManager::LoadGameDataOfClass(TSubclassOf<UPrimaryDataAsset> DataClass, const TSoftObjectPtr<UPrimaryDataAsset>& DataClassPath, FPrimaryAssetType PrimaryAssetType)
{
	UPrimaryDataAsset* Asset = nullptr;

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Loading GameData Object"), STAT_GameData, STATGROUP_LoadTime);
	if (!DataClassPath.IsNull())
	{
#if WITH_EDITOR
		FScopedSlowTask SlowTask(0, FText::Format(NSLOCTEXT("ModularGameDataEditor", "BeginLoadingGameDataTask", "Loading GameData {0}"), FText::FromName(DataClass->GetFName())));
		constexpr bool bShowCancelButton = false;
		constexpr bool bAllowInPIE = true;
		SlowTask.MakeDialog(bShowCancelButton, bAllowInPIE);
#endif
		UE_LOG(LogModularGameplayData, Log, TEXT("Loading GameData: %s ..."), *DataClassPath.ToString());
		SCOPE_LOG_TIME_IN_SECONDS(TEXT("    ... GameData loaded!"), nullptr);

		// Editor 中可能因 PostLoad 等按需递归调用：Primary 同步加载，其余走异步批量
		if (GIsEditor)
		{
			Asset = DataClassPath.LoadSynchronous();
			LoadPrimaryAssetsWithType(PrimaryAssetType);
		}
		else
		{
			TSharedPtr<FStreamableHandle> Handle = LoadPrimaryAssetsWithType(PrimaryAssetType);
			if (Handle.IsValid())
			{
				Handle->WaitUntilComplete(0.0f, false);

				// 正常完成时应能拿到已加载资源
				Asset = Cast<UPrimaryDataAsset>(Handle->GetLoadedAsset());
			}
		}
	}

	if (Asset)
	{
		GameDataMap.Add(DataClass, Asset);
	}
	else
	{
		// GameData 加载失败不可恢复，否则后续易出现难以排查的软错误
		UE_LOG(LogModularGameplayData, Fatal, TEXT("Failed to load GameData asset at %s. Type %s. This is not recoverable and likely means you do not have the correct data to run %s."), *DataClassPath.ToString(), *PrimaryAssetType.ToString(), FApp::GetProjectName());
	}

	return Asset;
}


void UModularAssetManager::DoAllStartupJobs()
{
	SCOPED_BOOT_TIMING("UModularAssetManager::DoAllStartupJobs");
	const double AllStartupJobsStartTime = FPlatformTime::Seconds();

	if (IsRunningDedicatedServer())
	{
		// Dedicated Server 无需向 Loading UI 汇报子进度，直接顺序执行
		for (const FModularAssetManagerStartupJob& StartupJob : StartupJobs)
		{
			StartupJob.DoJob();
		}
	}
	else
	{
		if (StartupJobs.Num() > 0)
		{
			float TotalJobValue = 0.0f;
			for (const FModularAssetManagerStartupJob& StartupJob : StartupJobs)
			{
				TotalJobValue += StartupJob.JobWeight;
			}

			float AccumulatedJobValue = 0.0f;
			for (FModularAssetManagerStartupJob& StartupJob : StartupJobs)
			{
				const float JobValue = StartupJob.JobWeight;
				StartupJob.SubstepProgressDelegate.BindLambda([This = this, AccumulatedJobValue, JobValue, TotalJobValue](float NewProgress)
					{
						const float SubstepAdjustment = FMath::Clamp(NewProgress, 0.0f, 1.0f) * JobValue;
						const float OverallPercentWithSubstep = (AccumulatedJobValue + SubstepAdjustment) / TotalJobValue;

						This->UpdateInitialGameContentLoadPercent(OverallPercentWithSubstep);
					});

				StartupJob.DoJob();

				StartupJob.SubstepProgressDelegate.Unbind();

				AccumulatedJobValue += JobValue;

				UpdateInitialGameContentLoadPercent(AccumulatedJobValue / TotalJobValue);
			}
		}
		else
		{
			UpdateInitialGameContentLoadPercent(1.0f);
		}
	}

	StartupJobs.Empty();

	UE_LOG(LogModularGameplayData, Display, TEXT("All startup jobs took %.2f seconds to complete"), FPlatformTime::Seconds() - AllStartupJobsStartTime);
}

void UModularAssetManager::UpdateInitialGameContentLoadPercent(float GameContentPercent)
{
	// 可将 GameContentPercent 转发给启动 Loading Screen 或 GameInstance
}

#if WITH_EDITOR
void UModularAssetManager::PreBeginPIE(const bool bStartSimulate)
{
	Super::PreBeginPIE(bStartSimulate);

	{
		FScopedSlowTask SlowTask(0, NSLOCTEXT("ModularGameDataEditor", "BeginLoadingPIEData", "Loading PIE Data"));
		constexpr bool bShowCancelButton = false;
		constexpr bool bAllowInPIE = true;
		SlowTask.MakeDialog(bShowCancelButton, bAllowInPIE);

		//const UModularGameData& LocalGameDataCommon = GetModularGameData();

		// 刻意放在 GetGameData 之后，避免把 GameData 耗时计入本段 SCOPE_LOG
		SCOPE_LOG_TIME_IN_SECONDS(TEXT("PreBeginPIE asset preloading complete"), nullptr);

		// 可在此预加载关卡/Experience 所需的其它资源（如从 WorldSettings 与 DeveloperSettings 解析默认 Experience）
	}
}
#endif
