#include "MoverMode/WallGrind/ChaosCharacterWallGrindCheck.h"

#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "MoverDataModelTypes.h"
#include "MoverLayeredMove/YanWallGrindMove.h"
#include "YanMoverDataModelTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosCharacterWallGrindCheck)

UChaosCharacterWallGrindCheck::UChaosCharacterWallGrindCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;

	// 推进在撞墙途中发起，目标模式与当前模式可能相同，需允许重入才能触发 Trigger；
	// 一次命中只应入队一个效果，故只在首个子步求值
	bAllowModeReentry = true;
	bFirstSubStepOnly = true;
}

FTransitionEvalResult UChaosCharacterWallGrindCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	// 推进意图是持续标志而非脉冲（服务器须在 game thread 采样到它），
	// 故已在推进中时不得再次入队，否则每帧都会叠一个效果
	if (!Simulation || Simulation->HasGameplayTag(Mover_IsWallGrinding, /*bExactMatch=*/true))
	{
		return FTransitionEvalResult::NoTransition;
	}

	const FYanCharacterInputs* YanInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();
	if (YanInputs && YanInputs->bWantsToWallGrind && YanInputs->WallGrindPath.Num() >= 2 && YanInputs->WallGrindEntrySpeed > 0.f)
	{
		return FTransitionEvalResult(TransitionToMode);
	}

	return FTransitionEvalResult::NoTransition;
}

void UChaosCharacterWallGrindCheck::Trigger_Implementation(const FSimulationTickParams& Params)
{
	if (!Simulation)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosCharacterWallGrindCheck 缺少 Simulation"));
		return;
	}

	const FYanCharacterInputs*    YanInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();
	const FMoverDefaultSyncState* SyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!YanInputs || !SyncState)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosCharacterWallGrindCheck 缺少解算推进轨迹所需的输入或状态"));
		return;
	}

	// 起点取仿真状态而非 Actor 位置：后者属 game-thread 状态，重模拟时两端不保证一致
	TSharedPtr<FYanLayeredMove_WallGrind> GrindMove = MakeShared<FYanLayeredMove_WallGrind>();
	GrindMove->StartLocation                        = SyncState->GetLocation_WorldSpace();
	GrindMove->Path                                 = YanInputs->WallGrindPath;
	GrindMove->EntrySpeed                           = YanInputs->WallGrindEntrySpeed;
	GrindMove->GrindStartSimTimeMs                  = YanInputs->WallGrindStartSimTimeMs;
	// 上传的折线取的正是发起方的行进位置，故无须再偏移
	GrindMove->PathFrameOffset = FVector::ZeroVector;
	GrindMove->Deceleration    = Deceleration;
	GrindMove->PullDurationMs  = PullDurationMs;
	GrindMove->EaseExponent    = EaseExponent;
	GrindMove->MaxHoldSpeed    = MaxHoldSpeed;
	GrindMove->DurationMs      = MaxDurationMs;
	// 发起方保留自由视角，朝向仍归移动模式处理
	GrindMove->bControlOrientation = false;
	// 发起方松手即停：推进意图本就是持续标志，无须再占用被推方那个挣脱位——
	// 双方的技能若同帧写同一字段会互相覆盖
	GrindMove->bCanBreakFree             = false;
	GrindMove->bEndOnGrindIntentCleared = true;

	Simulation->QueueLayeredMove_Internal(GrindMove);
}
