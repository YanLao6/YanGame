#pragma once

#include "Abilities/Ability_ColorCharge.h"

#include "Ability_BlueAttractOrb.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class AColorAttractOrb;

/**
 * 蓝方右键：蓄力期间手上的吸引球随蓄力变大并吸引周围 Pawn，松手后脱手进入操控阶段（见 AColorAttractOrb）。
 *
 * 操控阶段技能保持激活并屏蔽左键射线，吸引球到时销毁即结束技能；施法者仍可自由移动。
 * 吸引球在任何阶段被外部销毁（如被红球引爆）都会结束技能。
 */
UCLASS(MinimalAPI)
class UAbility_BlueAttractOrb : public UAbility_ColorCharge
{
	GENERATED_BODY()

public:
	UE_API explicit UAbility_BlueAttractOrb(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UGameplayAbility Interface
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	UE_API virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End UGameplayAbility Interface

protected:
	//~Begin UAbility_ColorCharge Interface
	UE_API virtual void OnChargeReleased(float ChargeSeconds) override;
	//~End UAbility_ColorCharge Interface

	/** 蓄力进度 0 与 1 时的操控时长（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|BlueAttract")
	FVector2D ControlDurationByCharge = FVector2D(3.0f, 10.0f);

private:
	void HandleControlFinished();

	// 仅服务器：已脱手的吸引球，技能被取消时一并销毁
	TWeakObjectPtr<AColorAttractOrb> LaunchedOrb;

	// 左键射线屏蔽标签是本地 Loose Tag，各端各自增减
	bool bBlockingColorShot = false;
};

#undef UE_API
