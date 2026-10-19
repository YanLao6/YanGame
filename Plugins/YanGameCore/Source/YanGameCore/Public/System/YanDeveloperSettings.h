// 开发期设置与查询声明。

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "YanDeveloperSettings.generated.h"

#define UE_API YANGAMECORE_API

/**
 * 开发期设置。
 *
 * 仅影响编辑器内的调试流程，不参与打包后的运行时行为。
 */
UCLASS(MinimalAPI, config = EditorPerProjectUserSettings, meta = (DisplayName = "Yan 开发者设置"))
class UYanDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UYanDeveloperSettings();

	//~Begin UDeveloperSettings Interface
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	//~End UDeveloperSettings Interface

	/** 为真时在 PIE 中走完整的前端流程，而非直接进入对局。 */
	UPROPERTY(config, EditAnywhere, Category = "Yan|PIE")
	bool bTestFullGameFlowInPIE = false;

	/** 为真时跳过加载大厅等装扮背景，缩短 PIE 启动时间。 */
	UPROPERTY(config, EditAnywhere, Category = "Yan|PIE")
	bool bSkipLoadingCosmeticBackgroundsInPIE = false;

	/** 为假时 Bot 只移动不开火，便于调试。 */
	UPROPERTY(config, EditAnywhere, Category = "Yan|PIE")
	bool bAllowPlayerBotsToAttack = true;
};

/**
 * 开发期查询。
 *
 * 各项在非编辑器环境下一律返回正式流程应有的取值，因此可安全用于运行时逻辑。
 */
UCLASS(MinimalAPI)
class UYanDevelopmentStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 是否跳过前端流程直接进入对局。 */
	UFUNCTION(BlueprintPure, Category = "Yan|Development")
	static UE_API bool ShouldSkipDirectlyToGameplay();

	/** 是否加载装扮背景。 */
	UFUNCTION(BlueprintPure, Category = "Yan|Development")
	static UE_API bool ShouldLoadCosmeticBackgrounds();

	/** Bot 是否允许开火。 */
	UFUNCTION(BlueprintPure, Category = "Yan|Development")
	static UE_API bool CanPlayerBotsAttack();
};

#undef UE_API
