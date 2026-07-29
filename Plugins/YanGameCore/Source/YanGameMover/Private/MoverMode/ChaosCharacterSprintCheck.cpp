#include "MoverMode/ChaosCharacterSprintCheck.h"

#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "MoverModifier/YanSprintModifier.h"
#include "YanMoverDataModelTypes.h"
#include "ChaosMover/ChaosMoverSimulation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosCharacterSprintCheck)

UChaosCharacterSprintCheck::UChaosCharacterSprintCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;

	// 重入当前模式以便触发 Trigger()；只在首个子步求值，避免一次 Mover tick 内重复排队 Modifier
	bAllowModeReentry = true;
	bFirstSubStepOnly = true;
}

bool UChaosCharacterSprintCheck::WantsToStartSprint(const FSimulationTickParams& Params) const
{
	const FYanCharacterInputs* YanInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();
	return YanInputs && YanInputs->bIsSprintJustPressed;
}

bool UChaosCharacterSprintCheck::CanSustainSprint(const FSimulationTickParams& Params) const
{
	// 蹲伏与疾跑互斥：两者都以 Modifier 覆盖同一模式的速度，同时生效结果不可预期
	if (Simulation->HasGameplayTag(Mover_IsCrouching, true))
	{
		return false;
	}

	const UMoverBlackboard* Blackboard = Simulation->GetBlackboard_Mutable();
	if (!Blackboard)
	{
		return false;
	}

	FFloorCheckResult FloorResult;
	if (!Blackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult) || !FloorResult.IsWalkableFloor())
	{
		return false;
	}

	const FCharacterDefaultInputs* DefaultInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	if (!DefaultInputs)
	{
		return false;
	}

	// 前进意愿与视线均投影到水平面比较，斜坡与俯仰角不参与判定
	FVector MoveDir = DefaultInputs->GetMoveInput_WorldSpace();
	MoveDir.Z       = 0.f;
	FVector ViewDir = DefaultInputs->ControlRotation.Vector();
	ViewDir.Z       = 0.f;

	// 无移动输入即无前进意愿，视为退出疾跑
	if (MoveDir.IsNearlyZero() || ViewDir.IsNearlyZero())
	{
		return false;
	}

	return FVector::DotProduct(MoveDir.GetSafeNormal(), ViewDir.GetSafeNormal()) > MinFacingDot;
}

FTransitionEvalResult UChaosCharacterSprintCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	bTriggerSprint = bTriggerStopSprint = false;

	if (!Simulation)
	{
		return FTransitionEvalResult::NoTransition;
	}

	const bool bIsSprinting = Simulation->HasGameplayTag(Mover_IsSprinting, true);
	const bool bCanSustain  = CanSustainSprint(Params);

	if (bIsSprinting)
	{
		// 已在疾跑：不再过问按键，只要维持条件仍成立就继续
		if (bCanSustain)
		{
			return FTransitionEvalResult::NoTransition;
		}
		bTriggerStopSprint = true;
	}
	else
	{
		// 未在疾跑：脉冲与维持条件须同帧成立，避免在空中或背身按键后一落地就起跑
		if (!bCanSustain || !WantsToStartSprint(Params))
		{
			return FTransitionEvalResult::NoTransition;
		}
		bTriggerSprint = true;
	}

	// 疾跑不改变移动模式，仅需重入当前模式以获得一次 Trigger 调用
	return FTransitionEvalResult(Params.StartState.SyncState.MovementMode);
}

void UChaosCharacterSprintCheck::Trigger_Implementation(const FSimulationTickParams& Params)
{
	if (!Simulation)
	{
		return;
	}

	if (bTriggerSprint)
	{
		TSharedPtr<FYanSprintModifier> Modifier = MakeShared<FYanSprintModifier>();
		Modifier->MaxSpeedOverride              = SprintMaxSpeed;
		Modifier->AccelerationOverride          = SprintAcceleration;
		Simulation->QueueMovementModifier(MoveTemp(Modifier));
	}
	else if (bTriggerStopSprint)
	{
		if (const FYanSprintModifier* Modifier = Simulation->FindMovementModifierByType<FYanSprintModifier>())
		{
			Simulation->CancelModifierFromHandle(Modifier->GetHandle());
		}
	}

	bTriggerSprint = bTriggerStopSprint = false;
}
