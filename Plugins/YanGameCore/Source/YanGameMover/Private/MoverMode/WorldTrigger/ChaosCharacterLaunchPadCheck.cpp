#include "MoverMode/WorldTrigger/ChaosCharacterLaunchPadCheck.h"

#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "MoverEffect/YanWorldTriggerEffects.h"
#include "MoverMode/WorldTrigger/YanWorldTriggerState.h"
#include "YanMoverDataModelTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosCharacterLaunchPadCheck)

UChaosCharacterLaunchPadCheck::UChaosCharacterLaunchPadCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;

	// 空中踩板（连续多块）时目标模式与当前相同，需允许重入才能触发 Trigger。
	// bFirstSubStepOnly 只约束状态机在单个 tick 内的子步；跨物理步复用同一输入包的去重靠请求序号
	bAllowModeReentry = true;
	bFirstSubStepOnly = true;
}

FTransitionEvalResult UChaosCharacterLaunchPadCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	const FYanCharacterInputs* YanInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();
	if (YanInputs && YanInputs->bWantsWorldLaunch && !YanInputs->WorldLaunchVelocity.IsNearlyZero()
	    && !FYanWorldTriggerState::IsLaunchConsumed(Params.StartState.SyncState, YanInputs->WorldLaunchRequestId))
	{
		return FTransitionEvalResult(TransitionToMode);
	}

	return FTransitionEvalResult::NoTransition;
}

void UChaosCharacterLaunchPadCheck::Trigger_Implementation(const FSimulationTickParams& Params)
{
	if (!Simulation)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosCharacterLaunchPadCheck 缺少 Simulation"));
		return;
	}

	const FYanCharacterInputs* YanInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();
	if (!YanInputs)
	{
		return;
	}

	TSharedPtr<FYanWorldLaunchEffect> LaunchEffect = MakeShared<FYanWorldLaunchEffect>();
	LaunchEffect->RequestId                 = YanInputs->WorldLaunchRequestId;
	LaunchEffect->VelocityOrImpulseToApply  = YanInputs->WorldLaunchVelocity;
	LaunchEffect->Mode                      = EChaosMoverVelocityEffectMode::AdditiveVelocity;
	Simulation->QueueInstantMovementEffect_Internal(LaunchEffect);
}
