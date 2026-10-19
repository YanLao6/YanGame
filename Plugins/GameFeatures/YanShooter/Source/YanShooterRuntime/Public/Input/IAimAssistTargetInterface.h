// 瞄准辅助目标接口声明。

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "IAimAssistTargetInterface.generated.h"

class UShapeComponent;

/** 一个瞄准辅助目标的描述数据。 */
USTRUCT(BlueprintType)
struct FAimAssistTargetOptions
{
	GENERATED_BODY()

	FAimAssistTargetOptions()
		: bIsActive(true)
	{}

	/** 用作该目标命中范围的形状组件。 */
	TWeakObjectPtr<UShapeComponent> TargetShapeComponent;

	/** 该目标携带的 Tag；与玩家的瞄准辅助过滤 Tag 有任一匹配即被排除。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameplayTagContainer AssociatedTags;

	/** 目标当前是否参与瞄准辅助。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	uint8 bIsActive : 1;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UAimAssistTaget : public UInterface
{
	GENERATED_BODY()
};

/**
 * 瞄准辅助目标接口。
 *
 * 实现方描述自身的命中形状与关联 Tag。目标进入玩家外圈准星范围时被查询。
 */
class IAimAssistTaget
{
	GENERATED_BODY()

public:
	/** 填写本目标的描述数据；目标进入玩家视野范围时调用。 */
	virtual void GatherTargetOptions(OUT FAimAssistTargetOptions& TargetData) = 0;
};
