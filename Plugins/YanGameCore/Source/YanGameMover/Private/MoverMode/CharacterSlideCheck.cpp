#include "MoverMode/CharacterSlideCheck.h"

#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "YanMoverDataModelTypes.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/Character/Effects/ChaosCharacterApplyVelocityEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CharacterSlideCheck)

UCharacterSlideCheck::UCharacterSlideCheck(const FObjectInitializer& InitializerModule)
	:Super(InitializerModule)
{
	bSupportsAsync = true;
}

FTransitionEvalResult UCharacterSlideCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	if (const UMoverBlackboard* Blackboard = Params.SimBlackboard)
	{
		FFloorCheckResult FloorResult;
		if (Blackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult) && FloorResult.IsWalkableFloor())
		{
			const FCharacterDefaultInputs* DefaultInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
			if (DefaultInputs && !DefaultInputs->bIsJumpJustPressed)
			{
				const FYanCharacterInputs* YanInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();
				if (YanInputs && YanInputs->bWantsToSlide)
				{
					if (const FMoverDefaultSyncState* SyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
					{
						const FVector Velocity = SyncState->GetVelocity_WorldSpace();
						if (FVector(Velocity.X, Velocity.Y, 0.f).Size() > MinSlideSpeed)
						{
							return FTransitionEvalResult(FName("Sliding"));
						}
					}
				}
			}
		}
	}

	return FTransitionEvalResult::NoTransition;
}

void UCharacterSlideCheck::Trigger_Implementation(const FSimulationTickParams& Params)
{
	if (SlideBoostSpeed <= 0.f || !Simulation)
	{
		return;
	}

	// 前冲方向优先取本帧移动输入（世界空间），无输入时回退到瞄准朝向
	FVector                        BoostDir      = FVector::ZeroVector;
	const FCharacterDefaultInputs* DefaultInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	if (DefaultInputs)
	{
		BoostDir = DefaultInputs->GetMoveInput_WorldSpace();
		if (BoostDir.IsNearlyZero())
		{
			BoostDir = DefaultInputs->ControlRotation.Vector();
		}
	}

	// 投影到水平面，避免在斜坡上获得竖直分量被弹起
	BoostDir.Z = 0.f;
	if (BoostDir.IsNearlyZero())
	{
		return;
	}
	BoostDir = BoostDir.GetSafeNormal();

	// 以 Additive 冲量一次性叠加到当前速度上（瞬时前冲，不覆盖原有速度）
	TSharedPtr<FChaosCharacterApplyVelocityEffect> Effect = MakeShared<FChaosCharacterApplyVelocityEffect>();
	Effect->VelocityOrImpulseToApply                      = BoostDir * SlideBoostSpeed;
	Effect->Mode                                          = EChaosMoverVelocityEffectMode::AdditiveVelocity;
	Simulation->QueueInstantMovementEffect_Internal(Effect);
}
