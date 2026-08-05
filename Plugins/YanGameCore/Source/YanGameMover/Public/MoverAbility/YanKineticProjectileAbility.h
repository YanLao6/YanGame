#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilities/ModularGameplayAbility.h"
#include "YanKineticProjectileAbility.generated.h"

#define UE_API YANGAMEMOVER_API

class AYanKineticSummon;
class UCurveFloat;
class USoundBase;
class UAnimMontage;

/**
 * 念力召唤物的蓄力发射技能（苍／赫的共同执行层）。
 *
 * 按住起蓄、松手射出，射向玩家视线所指。蓄力时长归一化为 [0, 1] 的蓄力量，
 * 同时决定两件事：飞得多快（LaunchSpeed），以及命中或牵引时作用多强（EffectMagnitude）。
 * 苍与赫的差别不在本类，而在各自配置的 SummonClass——本类只管蓄与射。
 *
 * 蓄力时长由 UAbilityTask_WaitInputRelease 在两端各自测量：
 * 客户端据本地时长播放表现，服务器据自己收到的松手时刻决定实际威力。
 * 生成与施力一律在服务器，本地端只播预测的施法表现。
 *
 * 蓝图默认值需设置：
 *   SummonClass           → 苍或赫的召唤物类
 *   ActivationBlockedTags → 须含 State.Kinesis.Controlling，
 *                           念力会话期间左右键被征用为前后修饰，此时不应触发发射
 */
UCLASS(MinimalAPI)
class UYanKineticProjectileAbility : public UModularGameplayAbility
{
	GENERATED_BODY()

public:
	UE_API UYanKineticProjectileAbility(const FObjectInitializer& Initializer);

	//~Begin UGameplayAbility Interface
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~End UGameplayAbility Interface

protected:
	/** 召唤物类：苍与赫各配一个，本技能的行为差异全部来自它 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis")
	TSubclassOf<AYanKineticSummon> SummonClass;

	/** 出生点在视点前方的距离（cm），须为正值，否则出生点与视点重合而无从定向 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", meta = (ClampMin = "1", ForceUnits = "cm"))
	float SpawnForwardOffset = 200.f;

	/** 蓄满所需时长（秒），超过此时长继续按住不再增强 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", meta = (ClampMin = "0.01"))
	float FullChargeSeconds = 1.f;

	/** 松手时的最低蓄力量，低于此值视为误触，不发射也不消耗 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", meta = (ClampMin = "0", ClampMax = "1"))
	float MinChargeToLaunch = 0.1f;

	/**
	 * 蓄力量到威力的映射曲线：横轴与纵轴均为 [0, 1]。
	 * 留空为线性。用曲线可以做出「后半段收益陡增」这类手感，鼓励蓄满再放。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis")
	TObjectPtr<UCurveFloat> ChargeResponseCurve;

	/** 空蓄发射的初速度（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MinLaunchSpeed = 900.f;

	/** 蓄满发射的初速度（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxLaunchSpeed = 3000.f;

	/** 空蓄的作用强度：赫为命中冲量（cm/s），苍为牵引强度 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", meta = (ClampMin = "0"))
	float MinEffectMagnitude = 600.f;

	/** 蓄满的作用强度，语义同上 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", meta = (ClampMin = "0"))
	float MaxEffectMagnitude = 2000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Kinesis")
	TObjectPtr<USoundBase> ChargeSound;

	UPROPERTY(EditDefaultsOnly, Category = "Kinesis")
	TObjectPtr<UAnimMontage> ChargeMontage;

	/** 蓝图实现：起蓄时的本地表现，如聚气特效 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Kinesis")
	UE_API void OnChargeStarted();

	/** 蓝图实现：射出时的本地表现，ChargeRatio 为本次的蓄力量 [0, 1] */
	UFUNCTION(BlueprintImplementableEvent, Category = "Kinesis")
	UE_API void OnProjectileLaunched(float ChargeRatio);

	/** 蓝图实现：蓄力不足而未发射，用于收起聚气表现 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Kinesis")
	UE_API void OnChargeAborted();

private:
	UFUNCTION()
	UE_API void HandleInputReleased(float TimeHeld);

	// 按住时长归一化并过响应曲线；曲线缺省时为线性
	UE_API float ComputeChargeRatio(float TimeHeld) const;

	// 服务器权威：在视点前方生成召唤物并即刻射出
	UE_API void SpawnAndLaunch(float ChargeRatio) const;

	UE_API void PlayChargeFeedback() const;
};

#undef UE_API
