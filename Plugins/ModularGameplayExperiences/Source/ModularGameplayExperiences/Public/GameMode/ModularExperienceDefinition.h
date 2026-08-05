// Copyright Chronicler.


#pragma once

#include "ModularExperienceActionSet.h"
#include "DataAsset/ModularPawnData.h"
#include "ModularExperienceDefinition.generated.h"

#define UE_API MODULARGAMEPLAYEXPERIENCES_API

/**
 * Experience 定义数据资产。
 *
 * 描述一个 Experience 需要启用的 GameFeature、默认 PawnData 以及动作集合。
 */
UCLASS(MinimalAPI)
class UModularExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 构造 Experience 定义对象。 */
	UE_API UModularExperienceDefinition();

	/** 该 Experience 需要启用的 GameFeature Plugin 列表。 */
	UPROPERTY(EditDefaultsOnly, Category="Gameplay")
	TArray<FString> GameFeaturesToEnable;

	/**
	 * 玩家默认使用的 PawnData（可指向 ModularPawnData）。
	 * Meta=(AssetBundles="Equipped") 使 AssetManager 在加载 Experience Equipped Bundle 时
	 * 自动将此软引用纳入异步加载，确保 OnExperienceLoadComplete 后 .Get() 有效。
	 */
	UPROPERTY(EditDefaultsOnly, Category="Gameplay", Meta=(AssetBundles="Equipped"))
	TSoftObjectPtr<const UModularPawnData> DefaultPawnData;

	/** 可复用的 ActionSet 列表。 */
	UPROPERTY(EditDefaultsOnly, Category="Gameplay")
	TArray<TObjectPtr<UModularExperienceActionSet>> ActionSets;

	/** Experience 加载/激活/停用/卸载各阶段要执行的 GameFeatureAction。 */
	UPROPERTY(EditDefaultsOnly, Category="Actions")
	TArray<TObjectPtr<UGameFeatureAction>> Actions;


	/** @implements UPrimaryDataAsset：汇总子资源的 AssetBundle 数据。 */
#if WITH_EDITORONLY_DATA
	UE_API virtual void UpdateAssetBundleData() override;
#endif

	/** @implements UObject：编辑器内数据校验。 */
#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};

#undef UE_API
