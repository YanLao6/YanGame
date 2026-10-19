#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/ChaosMovementModeTransition.h"
#include "MoverSimulationTypes.h"

#include "ChaosCharacterWallGrindCheck.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * UChaosCharacterWallGrindCheck - 贴墙摩擦推进的发起端入口，把角色送上预扫描的折线并沿其滑行。
 *
 * 由输入包的持续标志 FYanCharacterInputs::bWantsToWallGrind 触发，折线、沿墙初速与共同起滑时刻
 * 均直接取自输入包——折线来自一次沿墙扫描，两端各自重扫不保证得到同一条曲线，
 * 故与瞬移同一契约：只信任发起端上传的几何量。
 *
 * 仅服务发起方。被推方的同名效果由服务器经 sim action 下发，不经过本 Transition，
 * 因而此处入队的效果既不接管朝向也不可挣脱。
 *
 * 上传的折线取的是发起方的行进位置，故此处的路径偏移为零；被推方紧贴墙面的偏移
 * 由服务器在下发时按其权威位置定出。
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UChaosCharacterWallGrindCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UE_API UChaosCharacterWallGrindCheck(const FObjectInitializer& ObjectInitializer);

	/** 推进期间所处的运动模式。取空中模式以避开地面摩擦干扰折线跟踪，推进结束后由落地判定自行切走 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallGrind")
	FName TransitionToMode = FName("Falling");

	/**
	 * 摩擦减速度（cm/s²），与沿墙初速共同决定推进距离与时长。
	 * 应与发起技能扫描折线时所用的值一致：技能据此决定折线扫多长，此处据此决定滑多远。
	 * 不一致只会让两者中较短的一方先到，推进随之提前结束，不会出错但手感与预期有出入。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallGrind", meta = (ClampMin = "0"))
	float Deceleration = 900.f;

	/** 缓动段时长（毫秒），决定贴上折线的快慢 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallGrind", meta = (ClampMin = "1", UIMin = "30", UIMax = "300", ForceUnits = "ms"))
	float PullDurationMs = 80.f;

	/** 缓动段的时间重映射指数：小于 1 起步快、末端收得缓 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallGrind", meta = (ClampMin = "0.01", UIMin = "0.3", UIMax = "2"))
	float EaseExponent = 0.5f;

	/** 悬停段回正的速度上限（cm/s） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallGrind", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxHoldSpeed = 600.f;

	/** 推进效果的存续上限（毫秒），到期强制解除以免异常状态下被永久锁住 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallGrind", meta = (ClampMin = "100", ForceUnits = "ms"))
	float MaxDurationMs = 8000.f;

	//~Begin UBaseMovementModeTransition Interface
	/** 输入包请求推进且折线有效时切换到 TransitionToMode */
	UE_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;

	/** 以本帧位置为起点、输入包折线为轨迹入队推进效果 */
	UE_API virtual void Trigger_Implementation(const FSimulationTickParams& Params) override;
	//~End UBaseMovementModeTransition Interface
};

#undef UE_API
