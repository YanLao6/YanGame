// GameplayCue 路径注册动作实现。

#include "GameFeature/GameFeatureAction_AddGameplayCuePath.h"

#include "AbilitySystemGlobals.h"
#include "GameFeaturesSubsystem.h"
#include "GameplayCueManager.h"
#include "Misc/PackageName.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureAction_AddGameplayCuePath)

#define LOCTEXT_NAMESPACE "GameFeatures"

UGameFeatureAction_AddGameplayCuePath::UGameFeatureAction_AddGameplayCuePath()
{
	// 预置最常用的一条，省去逐个插件重复填写
	DirectoryPathsToAdd.Add(FDirectoryPath{ TEXT("/GameplayCues") });
}

//~Begin UGameFeatureAction Interface
void UGameFeatureAction_AddGameplayCuePath::OnGameFeatureRegistering()
{
	Super::OnGameFeatureRegistering();

	TArray<FString> CuePaths;
	ResolveCuePaths(CuePaths);
	if (CuePaths.IsEmpty())
	{
		return;
	}

	// 路径登记在 AbilitySystemGlobals 上而非直接交给 Cue 管理器：
	// 内置插件的注册早于其初始化，此时管理器尚不存在，但路径必须先留存下来。
	UAbilitySystemGlobals& AbilitySystemGlobals = UAbilitySystemGlobals::Get();
	for (const FString& CuePath : CuePaths)
	{
		AbilitySystemGlobals.AddGameplayCueNotifyPath(CuePath);
	}

	RebuildRuntimeCueLibrary();
}

void UGameFeatureAction_AddGameplayCuePath::OnGameFeatureUnregistering()
{
	TArray<FString> CuePaths;
	ResolveCuePaths(CuePaths);
	if (!CuePaths.IsEmpty())
	{
		UAbilitySystemGlobals& AbilitySystemGlobals = UAbilitySystemGlobals::Get();
		for (const FString& CuePath : CuePaths)
		{
			AbilitySystemGlobals.RemoveGameplayCueNotifyPath(CuePath);
		}

		// 重建以剔除已注销插件的 Cue，否则其条目会滞留到下次扫描。
		RebuildRuntimeCueLibrary();
	}

	Super::OnGameFeatureUnregistering();
}

//~End UGameFeatureAction Interface

FString UGameFeatureAction_AddGameplayCuePath::GetPluginRootPath() const
{
	const FName MountPoint = FPackageName::GetPackageMountPoint(GetOutermost()->GetName());
	return MountPoint.IsNone() ? FString() : (TEXT("/") + MountPoint.ToString());
}

void UGameFeatureAction_AddGameplayCuePath::ResolveCuePaths(TArray<FString>& OutPaths) const
{
	const FString PluginRootPath = GetPluginRootPath();
	if (PluginRootPath.IsEmpty())
	{
		return;
	}

	OutPaths.Reserve(DirectoryPathsToAdd.Num());
	for (const FDirectoryPath& Directory : DirectoryPathsToAdd)
	{
		if (Directory.Path.IsEmpty())
		{
			continue;
		}

		// 配置填的是相对插件根的目录，须补全为完整包路径后才能与扫描路径比对。
		FString MutablePath = Directory.Path;
		UGameFeaturesSubsystem::FixPluginPackagePath(MutablePath, PluginRootPath, /*bMakeRelativeToPluginRoot=*/ false);
		OutPaths.Add(MoveTemp(MutablePath));
	}
}

void UGameFeatureAction_AddGameplayCuePath::RebuildRuntimeCueLibrary()
{
	UAbilitySystemGlobals& AbilitySystemGlobals = UAbilitySystemGlobals::Get();
	if (!AbilitySystemGlobals.IsAbilitySystemGlobalsInitialized())
	{
		return;
	}

	if (UGameplayCueManager* CueManager = AbilitySystemGlobals.GetGameplayCueManager())
	{
		CueManager->InitializeRuntimeObjectLibrary();
	}
}

#if WITH_EDITOR
EDataValidationResult UGameFeatureAction_AddGameplayCuePath::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	for (const FDirectoryPath& Directory : DirectoryPathsToAdd)
	{
		if (Directory.Path.IsEmpty())
		{
			Context.AddError(FText::Format(LOCTEXT("InvalidCuePathError", "'{0}' 不是有效路径。"), FText::FromString(Directory.Path)));
			Result = CombineDataValidationResults(Result, EDataValidationResult::Invalid);
		}
	}

	return CombineDataValidationResults(Result, EDataValidationResult::Valid);
}
#endif	// WITH_EDITOR

#undef LOCTEXT_NAMESPACE
