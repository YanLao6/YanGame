#include "MoverMode/WallClimb/ChaosWallClimbJumpCheck.h"

#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "ChaosMover/Character/Effects/ChaosCharacterApplyVelocityEffect.h"
#include "DefaultMovementSet/CharacterMoverSimulationTypes.h"
#include "MoverDataModelTypes.h"
#include "MoverMode/WallClimb/YanWallClimbState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosWallClimbJumpCheck)

UChaosWallClimbJumpCheck::UChaosWallClimbJumpCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;
	// 一次按键只应施加一次起跳速度，故只在首个子步求值
	bFirstSubStepOnly = true;
}

FTransitionEvalResult UChaosWallClimbJumpCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	const FCharacterDefaultInputs* CharacterInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	if (CharacterInputs && CharacterInputs->bIsJumpJustPressed)
	{
		return FTransitionEvalResult(TransitionToMode);
	}

	return FTransitionEvalResult::NoTransition;
}

void UChaosWallClimbJumpCheck::Trigger_Implementation(const FSimulationTickParams& Params)
{
	if (!Simulation)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosWallClimbJumpCheck 缺少 Simulation"));
		return;
	}

	const FChaosMoverSimulationDefaultInputs* DefaultSimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>();
	const FCharacterDefaultInputs*            CharacterInputs  = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FMoverDefaultSyncState*             SyncState        = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!DefaultSimInputs || !CharacterInputs || !SyncState)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosWallClimbJumpCheck 缺少求解起跳所需的输入或状态"));
		return;
	}

	const FVector UpDir    = DefaultSimInputs->UpDir;
	const FVector Velocity = SyncState->GetVelocity_WorldSpace();

	// 先抵消既有竖直速度，使起跳高度与当前爬升速度无关
	FVector JumpVelocity = (JumpUpwardsSpeed - FVector::DotProduct(Velocity, UpDir)) * UpDir;

	const FYanWallClimbState* ClimbState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FYanWallClimbState>();
	const FVector             WallNormalHoriz = (ClimbState && ClimbState->IsCurrentFor(Params.TimeStep.ServerFrame))
	                                                ? FVector::VectorPlaneProject(ClimbState->WallNormal, UpDir).GetSafeNormal()
	                                                : FVector::ZeroVector;

	if (!WallNormalHoriz.IsNearlyZero())
	{
		// 清除贴墙吸附带来的朝墙速度，否则起跳会被压回墙面
		JumpVelocity -= FVector::DotProduct(Velocity, WallNormalHoriz) * WallNormalHoriz;

		const FVector ViewHoriz = FVector::VectorPlaneProject(CharacterInputs->ControlRotation.Vector(), UpDir).GetSafeNormal();
		if (!ViewHoriz.IsNearlyZero())
		{
			const float ViewDotNormal = FVector::DotProduct(ViewHoriz, WallNormalHoriz);
			// 视角朝墙外：整体取最大水平速度并随视角偏转；
			// 视角朝墙内：取视角在墙面切向的分量，其模长为 sinθ，正对墙面时归零
			const FVector HorizontalDir = (ViewDotNormal >= 0.f) ? ViewHoriz : (ViewHoriz - ViewDotNormal * WallNormalHoriz);
			JumpVelocity += MaxHorizontalSpeed * HorizontalDir;
		}
	}

	TSharedPtr<FChaosCharacterApplyVelocityEffect> JumpMove = MakeShared<FChaosCharacterApplyVelocityEffect>();
	JumpMove->VelocityOrImpulseToApply                      = JumpVelocity;
	JumpMove->Mode                                          = EChaosMoverVelocityEffectMode::AdditiveVelocity;
	Simulation->QueueInstantMovementEffect_Internal(JumpMove);

	// 与普通跳跃发出同一事件，动画与游戏线程无需区分来源
	Simulation->AddEvent(MakeShared<FJumpedEventData>(Params.TimeStep, SyncState->GetLocation_WorldSpace().Z));
}
