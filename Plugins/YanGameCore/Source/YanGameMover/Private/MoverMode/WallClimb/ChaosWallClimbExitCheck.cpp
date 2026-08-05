#include "MoverMode/WallClimb/ChaosWallClimbExitCheck.h"

#include "MoverMode/WallClimb/YanWallClimbState.h"
#include "YanMoverDataModelTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosWallClimbExitCheck)

UChaosWallClimbExitCheck::UChaosWallClimbExitCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;
}

FTransitionEvalResult UChaosWallClimbExitCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	// 主动脱墙意图从输入包读取，保证服务器 re-simulation 与客户端一致
	const FYanCharacterInputs* YanInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();
	if (YanInputs && YanInputs->bWantsToDetachFromWall)
	{
		return FTransitionEvalResult(TransitionToMode);
	}

	// 墙面丢失依据爬墙模式上一帧写入的法线判定：不自行查询，既免去一次物理查询，
	// 也不依赖本帧 GenerateMove 与 Transition 求值的先后顺序。
	// 状态不属于当前这一轮爬墙时（如进入后的首帧）尚无有效记录，此时不做判定。
	const FYanWallClimbState* ClimbState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FYanWallClimbState>();
	if (ClimbState && ClimbState->IsCurrentFor(Params.TimeStep.ServerFrame) && ClimbState->WallNormal.IsNearlyZero())
	{
		return FTransitionEvalResult(TransitionToMode);
	}

	return FTransitionEvalResult::NoTransition;
}
