// Copyright Chronicler.

#pragma once

#include "Engine/DataAsset.h"
#include "ModularInputConfig.h"
#include "ModularPawnData.generated.h"


class UPawnDataFragment;

/**
 * Pawn 数据定义资产。
 *
 * 不可变（Const）数据资产，用于描述 Pawn 的核心生成与输入配置。
 */
UCLASS(BlueprintType, Const, Meta = (DisplayName = "Pawn Data", ShortTooltip = "Data asset used to define a Pawn."))
class MODULARGAMEPLAYDATA_API UModularPawnData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 构造 PawnData 并初始化默认配置。 */
	explicit UModularPawnData(const FObjectInitializer& ObjectInitializer);

public:
	/** 生成该 Pawn 时实例化的 Class（通常派生自项目侧 Modular Pawn/Character）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pawn")
	TSubclassOf<APawn> PawnClass;

	/** PlayerController 控制下的 Pawn 使用的 Input 配置（映射 UInputAction 与 GameplayTag 等）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UModularInputConfig> InputConfig;
};
