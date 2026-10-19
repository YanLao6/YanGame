#include "MoverMode/WorldTrigger/ChaosCharacterTeleporterCheck.h"

#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "MoverEffect/YanWorldTriggerEffects.h"
#include "MoverMode/WorldTrigger/YanWorldTriggerState.h"
#include "YanMoverDataModelTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosCharacterTeleporterCheck)

UChaosCharacterTeleporterCheck::UChaosCharacterTeleporterCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;

	// 空中入门时目标模式与当前相同，需允许重入才能触发 Trigger。
	// bFirstSubStepOnly 只约束状态机在单个 tick 内的子步；跨物理步复用同一输入包的去重靠请求序号
	bAllowModeReentry = true;
	bFirstSubStepOnly = true;
}

FTransitionEvalResult UChaosCharacterTeleporterCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	const FYanCharacterInputs* YanInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();
	if (YanInputs && YanInputs->bWantsWorldTeleport
	    && !FYanWorldTriggerState::IsTeleportConsumed(Params.StartState.SyncState, YanInputs->WorldTeleportRequestId))
	{
		return FTransitionEvalResult(TransitionToMode);
	}

	return FTransitionEvalResult::NoTransition;
}

void UChaosCharacterTeleporterCheck::Trigger_Implementation(const FSimulationTickParams& Params)
{
	if (!Simulation)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosCharacterTeleporterCheck 缺少 Simulation"));
		return;
	}

	const FYanCharacterInputs* YanInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();
	if (!YanInputs)
	{
		return;
	}

	TSharedPtr<FYanWorldTeleportEffect> TeleportEffect = MakeShared<FYanWorldTeleportEffect>();
	TeleportEffect->RequestId                      = YanInputs->WorldTeleportRequestId;
	TeleportEffect->TargetLocation                 = YanInputs->WorldTeleportLocation;
	TeleportEffect->TargetRotation                 = YanInputs->WorldTeleportRotation;
	TeleportEffect->bUseActorRotation              = false;
	TeleportEffect->bOverrideVelocityAfterTeleport = YanInputs->bWorldTeleportOverridesVelocity;
	TeleportEffect->VelocityAfterTeleport          = YanInputs->WorldTeleportExitVelocity;
	Simulation->QueueInstantMovementEffect_Internal(TeleportEffect);
}
