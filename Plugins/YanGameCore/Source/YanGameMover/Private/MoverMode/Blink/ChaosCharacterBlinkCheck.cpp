#include "MoverMode/Blink/ChaosCharacterBlinkCheck.h"

#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "MoverDataModelTypes.h"
#include "MoverLayeredMove/YanBlinkToMove.h"
#include "YanMoverDataModelTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosCharacterBlinkCheck)

UChaosCharacterBlinkCheck::UChaosCharacterBlinkCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;

	// 目标模式常与当前模式相同（空中发起瞬移），需允许重入才能触发 Trigger；
	// 一次按键只应入队一个效果，故只在首个子步求值
	bAllowModeReentry = true;
	bFirstSubStepOnly = true;
}

FTransitionEvalResult UChaosCharacterBlinkCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	const FYanCharacterInputs* YanInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();
	if (YanInputs && YanInputs->bWantsToBlink && !YanInputs->BlinkTargetPoint.IsZero())
	{
		return FTransitionEvalResult(TransitionToMode);
	}

	return FTransitionEvalResult::NoTransition;
}

void UChaosCharacterBlinkCheck::Trigger_Implementation(const FSimulationTickParams& Params)
{
	if (!Simulation)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosCharacterBlinkCheck 缺少 Simulation"));
		return;
	}

	const FYanCharacterInputs* YanInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();
	const FMoverDefaultSyncState* SyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!YanInputs || !SyncState)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosCharacterBlinkCheck 缺少解算瞬移轨迹所需的输入或状态"));
		return;
	}

	// 起点取仿真状态而非 Actor 位置：后者属 game-thread 状态，重模拟时两端不保证一致
	TSharedPtr<FYanLayeredMove_BlinkTo> BlinkMove = MakeShared<FYanLayeredMove_BlinkTo>();
	BlinkMove->StartLocation                      = SyncState->GetLocation_WorldSpace();
	BlinkMove->TargetLocation                     = YanInputs->BlinkTargetPoint;
	BlinkMove->DurationMs                         = BlinkDurationMs;
	BlinkMove->EaseExponent                       = EaseExponent;
	// 模式切换已由本 Transition 的 TransitionToMode 完成，效果内不再重复强制
	BlinkMove->ForceMovementMode = NAME_None;

	Simulation->QueueLayeredMove_Internal(BlinkMove);
}
