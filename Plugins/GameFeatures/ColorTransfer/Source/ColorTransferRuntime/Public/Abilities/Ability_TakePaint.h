#pragma once

#include "Abilities/YanGameplayAbility.h"

#include "Ability_TakePaint.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

/**
 * 从颜料源取颜料：由交互技能以 Ability.Interaction.Activate 事件触发，Payload.Target 为实现 IColorPigmentSource 的机关。
 *
 * 本地预测执行只为把事件数据带到服务器，取色只在服务器上结算；
 * 目标由客户端指定，服务器按 MaxInteractDistance 校验后才生效。
 */
UCLASS(MinimalAPI)
class UAbility_TakePaint : public UYanGameplayAbility
{
	GENERATED_BODY()

public:
	UE_API explicit UAbility_TakePaint(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UGameplayAbility Interface
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~End UGameplayAbility Interface

protected:
	/** 每次交互从栈顶取出的颜料量 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|TakePaint", Meta = (ClampMin = "0.0"))
	float AmountPerInteract = 10.0f;

	/** 服务器校验的角色与颜料源的最大距离（cm），应略大于交互扫描范围以容忍延迟 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|TakePaint", Meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxInteractDistance = 700.0f;
};

#undef UE_API
