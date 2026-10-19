// GameplayCue 路径注册动作声明。

#pragma once

#include "CoreMinimal.h"
#include "GameFeatureAction.h"
#include "UObject/SoftObjectPath.h"
#include "GameFeatureAction_AddGameplayCuePath.generated.h"

/**
 * 向 GameplayCue 管理器注册额外的搜索路径。
 *
 * GameplayCue 按目录扫描，插件自带的 Cue 若不在已注册路径下则不会被发现，
 * 表现为技能触发但特效音效全无。
 *
 * @see UAbilitySystemGlobals::GameplayCueNotifyPaths
 */
UCLASS(MinimalAPI, meta = (DisplayName = "Add Gameplay Cue Path"))
class UGameFeatureAction_AddGameplayCuePath final : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	UGameFeatureAction_AddGameplayCuePath();

	//~Begin UObject Interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~End UObject Interface

	//~Begin UGameFeatureAction Interface
	virtual void OnGameFeatureRegistering() override;
	virtual void OnGameFeatureUnregistering() override;
	//~End UGameFeatureAction Interface

	const TArray<FDirectoryPath>& GetDirectoryPathsToAdd() const { return DirectoryPathsToAdd; }

private:
	// 本 Action 随 GameFeatureData 存放于插件内容根下，故由自身包路径的挂载点得出插件根；
	// 取不到时返回空串，调用方据此跳过注册而不是拼出一条错误路径。
	FString GetPluginRootPath() const;

	// 把配置路径解析为插件下的完整包路径，供注册与注销共用，确保两侧拼法一致。
	void ResolveCuePaths(TArray<FString>& OutPaths) const;

	// 路径增删后重建运行时 Cue 库；AbilitySystemGlobals 尚未初始化时跳过，
	// 此时路径已记入其列表，会被首次扫描一并纳入。
	static void RebuildRuntimeCueLibrary();

private:
	/** 待注册的路径，相对于游戏内容目录。 */
	UPROPERTY(EditAnywhere, Category = "Game Feature | Gameplay Cues", meta = (RelativeToGameContentDir, LongPackageName))
	TArray<FDirectoryPath> DirectoryPathsToAdd;
};
