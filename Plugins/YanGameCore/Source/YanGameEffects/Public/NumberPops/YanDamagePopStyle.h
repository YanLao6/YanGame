#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "YanDamagePopStyle.generated.h"

#define UE_API YANGAMEEFFECTS_API

class UStaticMesh;

/**
 * 网格文字飘字的样式。
 * 一次飘字会按顺序匹配样式列表，颜色与网格分别取首个声明覆盖该项的样式。
 */
UCLASS(MinimalAPI)
class UYanDamagePopStyle : public UDataAsset
{
	GENERATED_BODY()

public:
	UE_API UYanDamagePopStyle();

	UPROPERTY(EditDefaultsOnly, Category = "DamagePop")
	FString DisplayText;

	// 对目标标签的匹配条件
	UPROPERTY(EditDefaultsOnly, Category = "DamagePop")
	FGameplayTagQuery MatchPattern;

	UPROPERTY(EditDefaultsOnly, Category = "DamagePop", meta = (EditCondition = bOverrideColor))
	FLinearColor Color;

	UPROPERTY(EditDefaultsOnly, Category = "DamagePop", meta = (EditCondition = bOverrideColor))
	FLinearColor CriticalColor;

	UPROPERTY(EditDefaultsOnly, Category = "DamagePop", meta = (EditCondition = bOverrideMesh))
	TObjectPtr<UStaticMesh> TextMesh;

	UPROPERTY()
	bool bOverrideColor = false;

	UPROPERTY()
	bool bOverrideMesh = false;
};

#undef UE_API
