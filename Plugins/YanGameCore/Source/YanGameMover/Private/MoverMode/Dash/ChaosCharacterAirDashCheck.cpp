#include "MoverMode/Dash/ChaosCharacterAirDashCheck.h"

#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/Character/Effects/ChaosCharacterApplyVelocityEffect.h"
#include "MoverDataModelTypes.h"
#include "YanMoverDataModelTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosCharacterAirDashCheck)

UChaosCharacterAirDashCheck::UChaosCharacterAirDashCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;

	// 冲刺后通常仍处于同一空中模式，需允许重入才能触发 Trigger；
	// 一次按键只应施加一次冲量，故只在首个子步求值
	bAllowModeReentry = true;
	bFirstSubStepOnly = true;
}

FTransitionEvalResult UChaosCharacterAirDashCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	const FYanCharacterInputs* YanInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();
	if (YanInputs && YanInputs->bWantsToAirDash)
	{
		return FTransitionEvalResult(TransitionToMode);
	}

	return FTransitionEvalResult::NoTransition;
}

void UChaosCharacterAirDashCheck::Trigger_Implementation(const FSimulationTickParams& Params)
{
	if (!Simulation)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosCharacterAirDashCheck 缺少 Simulation"));
		return;
	}

	const FCharacterDefaultInputs* CharacterInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	if (!CharacterInputs)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosCharacterAirDashCheck 缺少解算冲刺方向所需的输入"));
		return;
	}

	// 方向一律取自输入包：Actor 朝向属 game-thread 状态，两端不保证一致
	FVector DashDir = CharacterInputs->GetMoveInput_WorldSpace();
	if (DashDir.IsNearlyZero())
	{
		DashDir = CharacterInputs->ControlRotation.Vector();
	}

	DashDir = DashDir.GetSafeNormal();
	if (DashDir.IsNearlyZero())
	{
		return;
	}

	TSharedPtr<FChaosCharacterApplyVelocityEffect> DashEffect = MakeShared<FChaosCharacterApplyVelocityEffect>();
	DashEffect->VelocityOrImpulseToApply                      = DashDir * DashMagnitude;
	DashEffect->Mode                                          = EChaosMoverVelocityEffectMode::AdditiveVelocity;
	Simulation->QueueInstantMovementEffect_Internal(DashEffect);
}
