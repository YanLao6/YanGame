#pragma once

#include "Abilities/YanGameplayAbility.h"
#include "NativeGameplayTags.h"

#include "Ability_ColorShot.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class AColorShotBeam;

/** 持有此 Loose Tag 时禁止发射颜色射线，已在发射中的也会立即停止。 */
COLORTRANSFERRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_ColorShotBlocked);

/**
 * 左键颜色射线：按住持续射出身上栈顶的颜色，松开即结束。
 *
 * 射线是长度受 MaxRange 限制的直线（见 AColorShotBeam::PredictPath）。服务器按 SprayInterval 结算：
 * 扣除颜料，命中角色则注入对方，命中实现了 IColorShotReceiver 的机关则交给机关。
 * 本地预测执行只为让客户端松手时的 EndAbility 同步到服务器，命中始终以服务器为准。
 */
UCLASS(MinimalAPI)
class UAbility_ColorShot : public UYanGameplayAbility
{
	GENERATED_BODY()

public:
	UE_API explicit UAbility_ColorShot(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UGameplayAbility Interface
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	UE_API virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	UE_API virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	//~End UGameplayAbility Interface

protected:
	/** 每秒射出的颜料量；外来颜料不足时只射出剩余部分 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorShot", Meta = (ClampMin = "0.0"))
	float PigmentPerSecond = 10.0f;

	/** 射线极限距离（cm），超出部分既不绘制也不判定命中 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorShot", Meta = (ClampMin = "1.0"))
	float MaxRange = 2000.0f;

	/** 服务器结算间隔（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorShot", Meta = (ClampMin = "0.02"))
	float SprayInterval = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorShot")
	TSubclassOf<AColorShotBeam> BeamClass;

private:
	// 服务器上的一次结算
	void SprayTick();

	UPROPERTY(Transient)
	TObjectPtr<AColorShotBeam> Beam;

	FTimerHandle SprayTimer;
};

#undef UE_API
