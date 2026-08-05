#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/ChaosMovementModeTransition.h"
#include "MoverSimulationTypes.h"

#include "ChaosWallClimbEnterCheck.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * UChaosWallClimbEnterCheck - 进入爬墙的判定，挂在 Falling 模式的 Transitions 下。
 *
 * 五项条件全部满足才进入，严进宽出：进入后视角与移动意图的变化只影响能否主动移动，
 * 不再作为去留依据，避免玩家因镜头微调被动脱墙。
 *  - 墙面倾角落在可攀爬区间
 *  - 视角朝向墙面
 *  - 玩家的移动意图朝向墙面（无输入时不进入）
 *  - 与墙面距离足够近
 *  - 竖直速度不向上：起跳上升段不会重新吸附回墙面，下落段才会
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UChaosWallClimbEnterCheck : public UChaosMovementModeTransition
{
	GENERATED_BODY()

public:
	UE_API UChaosWallClimbEnterCheck(const FObjectInitializer& ObjectInitializer);

	/** 满足条件后切换到的运动模式 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb")
	FName TransitionToMode = FName("WallClimb");

	/** 可攀爬墙面的最小倾角（度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Surface", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MinWallAngle = 80.f;

	/** 可攀爬墙面的最大倾角（度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Surface", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MaxWallAngle = 100.f;

	/** 允许吸附的最大离墙距离（cm，自胶囊表面起算） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Surface", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxWallDistance = 30.f;

	/** 探墙球体扫描的半径（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Surface", meta = (ClampMin = "0", ForceUnits = "cm"))
	float WallProbeRadius = 20.f;

	/** 视角与墙面内法线的最大夹角（度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Condition", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float ViewFacingTolerance = 45.f;

	/** 移动意图与墙面内法线的最大夹角（度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Condition", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MoveIntentTolerance = 45.f;

	/** 玩家向上移动的速度低于这个值时才会触发爬墙 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallClimb|Condition", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MinUpSpeed = 100.f;
	
	//~Begin UBaseMovementModeTransition Interface
	/** 五项条件全部满足时切换到 TransitionToMode */
	UE_API virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	//~End UBaseMovementModeTransition Interface
};

#undef UE_API
