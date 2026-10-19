#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilities/ModularGameplayAbility.h"
#include "YanMeleeStrikeAbility.generated.h"

#define UE_API YANGAMEMOVER_API

class UGameplayEffect;
class USoundBase;
class UAnimMontage;
class UAbilitySystemComponent;

/**
 * 近战挥击：按下即结算，把视线前方扇形范围内的敌人一并击退。
 *
 * 判定口径沿用项目其他 Mover 技能：原点取 Pawn 视点，方向取输入包上传的 ControlRotation，
 * 而非纯客户端的 PlayerCameraManager——服务器据此能用与客户端相同的数据复核命中。
 * 本地端只播预测的挥击表现，命中裁决与效果施加一律在服务器。
 *
 * 每个命中目标依次承受三件事，顺序不可颠倒：
 *  - 先禁止落地：击退若在地面模式下结算，水平速度当帧就被地面摩擦吃掉；
 *  - 再施加击退 GE：经 UMoverLaunchGameplayEffect 落到目标 Mover；
 *  - 最后施加效果组：伤害、减速等与位移无关的常规效果。
 *
 * 蓝图默认值需设置：
 *   KnockbackEffect → UMoverLaunchGameplayEffect 本身，或需要挂 Cue 时取其蓝图子类
 *
 * 普通形态与特殊形态是本类的两个蓝图子类，两者参数各自独立，共用同一 InputTag，
 * 由标签而非激活顺序决定谁生效——同一 InputTag 下的所有技能都会被逐个尝试激活，
 * 其顺序取自 ActivatableAbilities 的授予顺序，不可依赖：
 *   特殊形态：配 CooldownGameplayEffect 授予冷却标签，并 BlockAbilitiesWithTag 普通形态的资产标签；
 *   普通形态：ActivationRequiredTags 要求该冷却标签，即只在特殊形态冷却期间可用。
 * 缺了那条 Block，特殊形态先激活并打上冷却标签后，普通形态会在同一帧一并满足条件而双重结算。
 */
UCLASS(MinimalAPI)
class UYanMeleeStrikeAbility : public UModularGameplayAbility
{
	GENERATED_BODY()

public:
	UE_API UYanMeleeStrikeAbility(const FObjectInitializer& Initializer);

	//~Begin UGameplayAbility Interface
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	UE_API virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	//~End UGameplayAbility Interface

protected:
	/** 攻击距离（cm），以施法者视点起算 */
	UPROPERTY(EditDefaultsOnly, Category = "Melee", meta = (ClampMin = "1", ForceUnits = "cm"))
	float AttackRange = 300.f;

	/** 扇形半角（度）：与视线夹角在此以内的目标才算命中，90 度即为前方半球 */
	UPROPERTY(EditDefaultsOnly, Category = "Melee", meta = (ClampMin = "1", ClampMax = "180", ForceUnits = "degrees"))
	float AttackHalfAngle = 45.f;

	/** 水平击退速度（cm/s），沿视线的水平投影方向 */
	UPROPERTY(EditDefaultsOnly, Category = "Melee", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float KnockbackSpeed = 800.f;

	/** 垂直上抬速度（cm/s），与俯仰角和水平速度均无关，滞空时间由它与重力单独决定 */
	UPROPERTY(EditDefaultsOnly, Category = "Melee", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float UpwardSpeed = 0.f;

	/** 击退 GE，取 UMoverLaunchGameplayEffect 或其蓝图子类；留空即只施加效果组而不推开目标 */
	UPROPERTY(EditDefaultsOnly, Category = "Melee|Effects")
	TSubclassOf<UGameplayEffect> KnockbackEffect;

	/** 命中时依次施加给目标的常规效果，如伤害、减速、受击标记 */
	UPROPERTY(EditDefaultsOnly, Category = "Melee|Effects")
	TArray<TSubclassOf<UGameplayEffect>> HitEffects;

	UPROPERTY(EditDefaultsOnly, Category = "Melee|Feedback")
	TObjectPtr<USoundBase> StrikeSound;

	UPROPERTY(EditDefaultsOnly, Category = "Melee|Feedback")
	TObjectPtr<UAnimMontage> StrikeMontage;

	/** 蓝图实现：挥击瞬间的本地表现，此时命中尚未裁决，故不带命中信息 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Melee")
	UE_API void OnStrikePerformed();

private:
	// 收集视点前方扇形内、视线未被遮挡且带 ASC 的敌方 Pawn
	void GatherTargets(const FVector& Origin, const FVector& AimForward, TArray<APawn*>& OutTargets) const;

	// 服务器权威：对单个目标依次施加禁止落地、击退与效果组
	void ApplyStrikeToTarget(UAbilitySystemComponent* InstigatorASC, APawn* Target, const FVector& KnockbackVelocity) const;

	void PlayLocalFeedback() const;
};

#undef UE_API
