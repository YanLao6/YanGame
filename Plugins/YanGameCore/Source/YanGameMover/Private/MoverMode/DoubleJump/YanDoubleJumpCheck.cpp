#include "MoverMode/DoubleJump/YanDoubleJumpCheck.h"

#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "DefaultMovementSet/CharacterMoverSimulationTypes.h"
#include "MoverDataModelTypes.h"
#include "MoverEffect/YanFatigueEffects.h"
#include "MoverMode/DoubleJump/YanJumpPulseState.h"
#include "MoverMode/Fatigue/YanFatigueState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanDoubleJumpCheck)

UYanDoubleJumpCheck::UYanDoubleJumpCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;

	// 二段跳后仍处于同一空中模式，需允许重入才能触发 Trigger；
	// 一次按键只应施加一次速度，故只在首个子步求值
	bAllowModeReentry = true;
	bFirstSubStepOnly = true;
}

FTransitionEvalResult UYanDoubleJumpCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	const FCharacterDefaultInputs* CharacterInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	if (!CharacterInputs || !CharacterInputs->bIsJumpJustPressed)
	{
		return FTransitionEvalResult::NoTransition;
	}

	const FMoverSyncState& SyncState = Params.StartState.SyncState;
	if (FYanJumpPulseState::WasJumpPressedLastStep(SyncState))
	{
		return FTransitionEvalResult::NoTransition;
	}

	if (FYanFatigueState::GetFatigue(SyncState, EYanFatigueChannel::DoubleJump) > 0.f)
	{
		return FTransitionEvalResult::NoTransition;
	}

	return FTransitionEvalResult(TransitionToMode);
}

void UYanDoubleJumpCheck::Trigger_Implementation(const FSimulationTickParams& Params)
{
	// 冷却为 0 时落地即完全恢复
	constexpr float DoubleJumpInstantRecoveryPerSecond = 1.e6f;

	if (!Simulation)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UYanDoubleJumpCheck 缺少 Simulation，请确认使用 Chaos 后端"));
		return;
	}

	const FChaosMoverSimulationDefaultInputs* SimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>();
	if (!SimInputs)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UYanDoubleJumpCheck 需要 FChaosMoverSimulationDefaultInputs"));
		return;
	}

	FVector JumpVelocity = JumpUpwardsSpeed * SimInputs->UpDir;

	// 扣掉当前竖直分量，使二段跳高度不受下落速度影响
	const FMoverDefaultSyncState* StartingSyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (StartingSyncState)
	{
		JumpVelocity -= StartingSyncState->GetVelocity_WorldSpace().Dot(SimInputs->UpDir) * SimInputs->UpDir;
	}

	TSharedPtr<FYanFatigueVelocityEffect> JumpMove = MakeShared<FYanFatigueVelocityEffect>();
	JumpMove->VelocityOrImpulseToApply = JumpVelocity;
	JumpMove->Mode                     = EChaosMoverVelocityEffectMode::AdditiveVelocity;
	JumpMove->Channel                  = EYanFatigueChannel::DoubleJump;
	JumpMove->FatigueCost              = 1.f;
	JumpMove->RecoveryPerSecond        = (CooldownSeconds > 0.f) ? 1.f / CooldownSeconds : DoubleJumpInstantRecoveryPerSecond;
	JumpMove->bRecoverAfterLanding     = true;
	Simulation->QueueInstantMovementEffect_Internal(JumpMove);

	const float StartingHeight = StartingSyncState ? StartingSyncState->GetLocation_WorldSpace().Z : 0.0f;
	Simulation->AddEvent(MakeShared<FJumpedEventData>(Params.TimeStep, StartingHeight));
}
