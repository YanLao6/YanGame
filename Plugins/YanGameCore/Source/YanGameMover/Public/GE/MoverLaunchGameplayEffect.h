#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "MoverLaunchGameplayEffect.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 击飞 GameplayEffect：构造时即接好 UMoverLaunchExecutionCalculation，可直接使用而无须再配资产。
 *
 * 击飞速度由施加方在命中当帧算出并经 SetByCaller 传入，故不在此配置；
 * 本类上的参数只描述速度落到目标 Mover 之后的施加方式，属于随效果一同定死的静态配置。
 *
 * 需要挂 GameplayCue、附带标签或改动这些参数时，以本类为父类新建 GameplayEffect 蓝图；
 * 直接引用本类即取下列默认值。
 */
UCLASS(MinimalAPI)
class UMoverLaunchGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UE_API UMoverLaunchGameplayEffect();

	/**
	 * 击飞持续时长（毫秒），仅标准 Mover 生效。
	 * Chaos Mover 以单帧瞬时速度落地，无持续语义，此项对其无效。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0", ForceUnits = "ms"))
	float LaunchDurationMs = 0.f;

	/** 击飞期间强制切入的移动模式，仅标准 Mover 生效 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Launch")
	FName ForceMovementMode = FName("Falling");

	/** 覆盖目标当前速度还是在其上叠加，仅 Chaos Mover 生效 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Launch")
	bool bOverrideVelocity = true;

	/**
	 * 目标满负荷时的击飞速度倍率，零负荷时恒为 1，其间线性插值。
	 *
	 * 取倍率而非固定加量，各技能自身的击飞强度差异才不会在高负荷段被抹平。
	 * 设为 1 即关闭负荷缩放。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Launch|Load", meta = (ClampMin = "1.0"))
	float SpeedMultiplierAtMaxLoad = 3.0f;

	/**
	 * 格挡生效的正面锥张角（全宽，度）。来袭方向落在锥内则本次击飞被完全抵消。
	 *
	 * 设为 0 即本效果无法被格挡，用于设计上不给反制手段的击飞。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Launch|Block", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float BlockFacingConeDegrees = 120.f;

	/** 格挡成功时回补给格挡方的机动能量格数。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Launch|Block", meta = (ClampMin = "0.0"))
	float ChargeRefundOnBlock = 1.f;

	/**
	 * 目标处于低摩擦滑行时的击飞速度倍率。
	 *
	 * 滑行没有摩擦去吃掉这股速度，同一次击退推得更远——这是滑行那份「加速快、难预判」
	 * 的收益所对应的代价。设为 1 即滑行不额外放大。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Launch|Load", meta = (ClampMin = "1.0"))
	float SpeedMultiplierWhileSliding = 1.6f;

	/**
	 * 目标附墙时被击退，额外附加的向下速度（cm/s）。
	 *
	 * 附墙者被击中却仍挂在原处会让墙面成为免疫区，故一律强制脱墙；向下的分量则保证
	 * 脱墙不等于「原地起跳」，被打下来就是被打下来。设为 0 即只脱墙、不附加下坠。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Launch|Load", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float WallDetachDownwardSpeed = 400.f;

	/**
	 * 强制脱墙后禁止落地的时长（秒）。
	 * 期间不判定落地，击退速度因而不会当帧被地面摩擦吃掉。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Launch|Load", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float WallDetachNoLandingSeconds = 0.2f;

	/**
	 * 击退成立时从目标身上抽走的机动能量格数。
	 *
	 * 这一条把「进攻」与「断人退路」合并成同一个动作：挨打本身就在削弱对方的逃生能力，
	 * 打空能量因而是一个可以主动执行的战术目标，而非运气。设为 0 即只推开、不抽能量。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Launch|Load", meta = (ClampMin = "0.0"))
	float ChargePenaltyOnHit = 1.f;

	/**
	 * 击退成立时取消目标身上带有这些标签的技能。
	 *
	 * 存在的理由是钩锁：飞行中的钩锁由目标自己的输入包持续驱动，外部施力改不了它的轨迹，
	 * 只有让那个技能结束，输入标志才会停写、运动模式才会退出。以标签而非某个具体技能类
	 * 表达，是为了让「哪一击能打断什么」留在资产里，击退计算不必认识任何一个技能。
	 *
	 * 断索本身不清动量——击退以 override 方式落到目标速度上，原有动量自然被顶掉。
	 *
	 * 取消经服务器发起，目标端要等一个往返才真正脱钩。这与本作「不做帧级精确防守」的
	 * 前提一致：打断是否成立由服务器裁定，不指望两端在同一帧看到同一结果。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Launch|Break", meta = (Categories = "Ability"))
	FGameplayTagContainer AbilityTagsToCancelOnHit;

	/**
	 * 击退成立时挂给目标的卸力窗口效果，其持续时间即窗口长度。
	 *
	 * 窗口由击退方配置而非受击方，因为「这一击给不给反制机会」是击退本身的属性；
	 * 留空即本次击退无法被卸力，用于设计上不给退路的击飞。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Launch|Break")
	TSubclassOf<UGameplayEffect> BreakWindowEffect;
};

#undef UE_API
