#include "MoverMode/WallClimb/ChaosWallClimbExitCheck.h"

#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "ChaosMover/Character/Effects/ChaosCharacterApplyVelocityEffect.h"
#include "MoverMode/WallClimb/ChaosWallClimbMode.h"
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
	if (!ClimbState || !ClimbState->IsCurrentFor(Params.TimeStep.ServerFrame))
	{
		return FTransitionEvalResult::NoTransition;
	}

	if (ClimbState->WallNormal.IsNearlyZero())
	{
		return FTransitionEvalResult(TransitionToMode);
	}

	// 耗尽依据爬墙模式累计的时长判定，上限由所属爬墙模式配置
	const UChaosWallClimbMode* ClimbMode = GetTypedOuter<UChaosWallClimbMode>();
	if (ClimbMode && ClimbMode->MaxClimbDurationMs > 0.f && ClimbState->ClimbedTimeMs >= ClimbMode->MaxClimbDurationMs)
	{
		return FTransitionEvalResult(TransitionToMode);
	}

	return FTransitionEvalResult::NoTransition;
}

void UChaosWallClimbExitCheck::Trigger_Implementation(const FSimulationTickParams& Params)
{
	if (ExitUpwardsSpeed <= 0.f)
	{
		return;
	}

	if (!Simulation)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosWallClimbExitCheck 缺少 Simulation"));
		return;
	}

	const FChaosMoverSimulationDefaultInputs* DefaultSimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>();
	if (!DefaultSimInputs)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosWallClimbExitCheck 缺少模拟输入"));
		return;
	}

	TSharedPtr<FChaosCharacterApplyVelocityEffect> ExitEffect = MakeShared<FChaosCharacterApplyVelocityEffect>();
	ExitEffect->VelocityOrImpulseToApply = DefaultSimInputs->UpDir * ExitUpwardsSpeed;
	ExitEffect->Mode = EChaosMoverVelocityEffectMode::AdditiveVelocity;
	Simulation->QueueInstantMovementEffect_Internal(ExitEffect);
}
