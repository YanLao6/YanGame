// Copyright Chronicler.

#pragma once

#include "Engine/DataAsset.h"
#include "PawnDataFragment.h"

#include "ModularPawnDataFragmentSet.generated.h"

#define UE_API MODULARGAMEPLAYDATA_API

/**
 * 可复用的 PawnDataFragment 集合资产。
 *
 * 供多份 PawnData 共享同一组基础注入（通用输入、通用界面、基础组件等）。
 * 不承载 PawnClass：生成哪个 Pawn 是单份 PawnData 的职责，本资产只描述能力。
 * 不引用其它 Set：一层组合已满足复用，嵌套会引入环检测与展平顺序的歧义。
 */
UCLASS(MinimalAPI, BlueprintType, Const, Meta = (DisplayName = "Pawn Data Fragment Set", ShortTooltip = "Reusable set of pawn data fragments."))
class UModularPawnDataFragmentSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * 本 Set 包含的数据单元。
	 *
	 * 顺序在 PawnData 展平后保持不变，故本数组内的先后依赖同样成立。
	 */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Pawn")
	TArray<TObjectPtr<UPawnDataFragment>> Fragments;

	//~Begin UObject Interface
#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	//~End UObject Interface
};

#undef UE_API
