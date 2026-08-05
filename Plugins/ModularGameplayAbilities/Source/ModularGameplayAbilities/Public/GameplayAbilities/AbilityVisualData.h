// Copyright Chronicler.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"

#include "AbilityVisualData.generated.h"

#define UE_API MODULARGAMEPLAYABILITIES_API

class UTexture2D;

/**
 * 技能视觉数据资产。
 *
 * 把技能的表现信息（名称、图标）从技能类中剥离出来，技能资产只负责行为。
 * 二者以 AbilityTag 关联：此处的 AbilityTag 应与技能自身 AssetTags 中的标识一致，
 * 因而技能无需为了显示而继承特定基类，界面也无需引用技能类。
 *
 * 图标采用软引用，未显示时不驻留内存；配合 AssetManager 的 AbilityVisual 分组按需加载。
 */
UCLASS(MinimalAPI, BlueprintType)
class UAbilityVisualData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 所描述技能的标识，须与技能 AssetTags 中的对应项一致 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual", Meta = (Categories = "Ability"))
	FGameplayTag AbilityTag;

	/** 界面显示名称 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	FText DisplayName;

	/** 界面显示图标 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual", Meta = (AssetBundles = "AbilityVisual"))
	TSoftObjectPtr<UTexture2D> Icon;

	/**
	 * 是否描述指定技能。
	 * 采用包含父级的匹配，传入 A.B 可命中标识为 A.B.C 的技能。
	 */
	UFUNCTION(BlueprintPure, Category = "Visual")
	UE_API bool MatchesAbilityTag(FGameplayTag InAbilityTag) const;

	/** 取已加载的图标；未加载时返回空，不触发同步加载。 */
	UFUNCTION(BlueprintPure, Category = "Visual")
	UE_API UTexture2D* GetLoadedIcon() const;

	/** 取图标，必要时同步加载。仅在可接受卡顿的场合调用。 */
	UFUNCTION(BlueprintCallable, Category = "Visual")
	UE_API UTexture2D* LoadIcon() const;

	/** 在一组视觉数据中检索匹配指定标识的首个条目。 */
	UFUNCTION(BlueprintPure, Category = "Visual")
	static UE_API UAbilityVisualData* FindByAbilityTag(const TArray<UAbilityVisualData*>& VisualDataList, FGameplayTag InAbilityTag);
};

#undef UE_API
