#pragma once

#include "CoreMinimal.h"
#include "Actor/YanKineticSummon.h"

#include "YanKineticAttractor.generated.h"

#define UE_API YANGAMEMOVER_API

class UGameplayEffect;

/**
 * 苍：带牵引场的念力召唤物。
 *
 * 与赫的区别只在生效方式——赫要撞上才算数，苍不必碰到任何人：
 * 飞行途中持续把半径内的目标往自己身上拽，越近拽得越狠，出了半径完全不受影响。
 *
 * 牵引按 AttractionInterval 一拍一拍地下发，每拍带上当时的自身坐标，
 * 时长取略长于一拍以便首尾衔接。之所以不一次性下发一个长效果，
 * 是因为牵引中心随苍飞行而移动，而 LayeredMove 在物理线程求值，拿不到 Actor 的实时位置。
 *
 * 「谁会被拽」交由 AttractionEffect 这个 GE 裁决：本类只负责把半径内的候选交给 GAS，
 * 免疫、阵营、护盾等规则在 GE 资产上配置。召唤者自身一律排除。
 *
 * 蓝图默认值需设置：
 *   AttractionEffect   → 使用 UMoverRadialAttractionExecutionCalculation 的 GE
 *   ImpactLaunchEffect → 留空，苍靠牵引而非撞击生效
 */
UCLASS(MinimalAPI)
class AYanKineticAttractor : public AYanKineticSummon
{
	GENERATED_BODY()

public:
	UE_API AYanKineticAttractor();

	/** 牵引半径（cm）：超出此距离完全没有牵引 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Kinesis|Attraction", meta = (ClampMin = "0", ForceUnits = "cm"))
	float AttractionRadius = 800.f;

	/** 中心处的牵引速度（cm/s）；发射方按蓄力量给出作用强度时以其为准 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Kinesis|Attraction", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float AttractionMagnitude = 1200.f;

	/** 衰减指数，作用于 (1 - 距离/半径)：1 为线性，越大牵引越集中在中心附近 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Kinesis|Attraction", meta = (ClampMin = "0.01"))
	float FalloffExponent = 1.f;

	/** 牵引刷新间隔（秒）：越小跟随越紧，网络与 GE 开销越大 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis|Attraction", meta = (ClampMin = "0.01"))
	float AttractionInterval = 0.1f;

	/** 牵引 GameplayEffect（Instant，须使用 UMoverRadialAttractionExecutionCalculation） */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis|Attraction")
	TSubclassOf<UGameplayEffect> AttractionEffect;

	/** 单拍生效的目标数上限，避免密集人群下的网络尖峰 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis|Attraction", meta = (ClampMin = "1"))
	int32 MaxAttractionTargets = 8;

protected:
	//~Begin AActor Interface
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End AActor Interface

	//~Begin AYanKineticSummon Interface
	UE_API virtual void OnLaunched() override;
	//~End AYanKineticSummon Interface

private:
	UFUNCTION()
	UE_API void ApplyAttractionPulse();

	FTimerHandle AttractionTimer;
};

#undef UE_API
