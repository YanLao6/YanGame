// Copyright Chronicler.


#pragma once

#include "GameFeatureAction.h"
#include "ModularExperienceActionSet.generated.h"

/**
 * 可复用的 GameFeatureAction 集合数据资产。
 *
 * 由 UModularExperienceDefinition 引用，用于拆分 Experience 的动作与 Feature 依赖。
 */
UCLASS()
class UModularExperienceActionSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 构造 ActionSet。 */
	UModularExperienceActionSet();

#if WITH_EDITOR
	/** 编辑器内校验 Actions 数组无空项等。 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

#if WITH_EDITORONLY_DATA
	/** 将子 Action 的 AssetBundle 数据合并进本资产。 */
	virtual void UpdateAssetBundleData() override;
#endif

public:
	/** 本 ActionSet 包含的 GameFeatureAction（Instanced）。 */
	UPROPERTY(EditAnywhere, Instanced, Category="Actions to Perform")
	TArray<TObjectPtr<UGameFeatureAction>> Actions;

	/** 激活本 ActionSet 时需要启用的 GameFeature Plugin 名称列表。 */
	UPROPERTY(EditAnywhere, Category="Feature Dependencies")
	TArray<FString> GameFeaturesToEnable;
};
