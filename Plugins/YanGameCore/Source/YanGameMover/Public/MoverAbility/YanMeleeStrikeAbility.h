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
 *  - 再施加击退 GE：经 UMoverLaunchExecutionCalculation 落到目标 Mover；
 *  - 最后施加效果组：伤害、减速等与位移无关的常规效果。
 *
 * 蓝图默认值需设置：
 *   KnockbackEffect → 使用 UMoverLaunchExecutionCalculation 的 GameplayEffect 资产类
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

	/** 击退速度大小（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "Melee", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float KnockbackSpeed = 800.f;

	/** 击退方向的上抬比例：0 为纯水平，1 为视线与正上方各半 */
	UPROPERTY(EditDefaultsOnly, Category = "Melee", meta = (ClampMin = "0", ClampMax = "1"))
	float UpwardRatio = 0.4f;

	/**
	 * 命中后禁止目标落地的时长（秒），0 表示不禁止。
	 * 击退冲量若在地面模式下结算会被地面摩擦当帧吃掉，故先把目标压在空中一小段时间。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Melee", meta = (ClampMin = "0", ForceUnits = "s"))
	float NoLandingDuration = 0.3f;

	/** 击退 GE，须使用 UMoverLaunchExecutionCalculation；留空即只施加效果组而不推开目标 */
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
	UE_API void GatherTargets(const FVector& Origin, const FVector& AimForward, TArray<APawn*>& OutTargets) const;

	// 服务器权威：对单个目标依次施加禁止落地、击退与效果组
	UE_API void ApplyStrikeToTarget(UAbilitySystemComponent* InstigatorASC, APawn* Target, const FVector& KnockbackVelocity) const;

	UE_API void PlayLocalFeedback() const;
};

#undef UE_API
