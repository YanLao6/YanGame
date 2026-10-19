#include "MoverMode/Clinch/ChaosCharacterClinchCheck.h"

#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "MoverDataModelTypes.h"
#include "MoverLayeredMove/YanClinchMove.h"
#include "YanMoverDataModelTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosCharacterClinchCheck)

UChaosCharacterClinchCheck::UChaosCharacterClinchCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;

	// 抓取在空中冲刺途中发起，目标模式与当前模式相同，需允许重入才能触发 Trigger；
	// 一次命中只应入队一个效果，故只在首个子步求值
	bAllowModeReentry = true;
	bFirstSubStepOnly = true;
}

FTransitionEvalResult UChaosCharacterClinchCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	// 抓取意图是持续标志而非脉冲（服务器须在 game thread 采样到它），
	// 故已在贴身中时不得再次入队，否则每帧都会叠一个效果
	if (!Simulation || Simulation->HasGameplayTag(Mover_IsClinched, /*bExactMatch=*/true))
	{
		return FTransitionEvalResult::NoTransition;
	}

	const FYanCharacterInputs* YanInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();
	if (YanInputs && YanInputs->bWantsToClinch && !YanInputs->ClinchAnchorPoint.IsZero())
	{
		return FTransitionEvalResult(TransitionToMode);
	}

	return FTransitionEvalResult::NoTransition;
}

void UChaosCharacterClinchCheck::Trigger_Implementation(const FSimulationTickParams& Params)
{
	if (!Simulation)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosCharacterClinchCheck 缺少 Simulation"));
		return;
	}

	const FChaosMoverSimulationDefaultInputs* DefaultSimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>();
	const FYanCharacterInputs*                YanInputs        = Params.StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();
	const FMoverDefaultSyncState*             SyncState        = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!DefaultSimInputs || !YanInputs || !SyncState)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UChaosCharacterClinchCheck 缺少解算贴身轨迹所需的输入或状态"));
		return;
	}

	// 起点取仿真状态而非 Actor 位置：后者属 game-thread 状态，重模拟时两端不保证一致
	TSharedPtr<FYanLayeredMove_Clinch> ClinchMove = MakeShared<FYanLayeredMove_Clinch>();
	ClinchMove->StartLocation                     = SyncState->GetLocation_WorldSpace();
	ClinchMove->AnchorLocation                    = YanInputs->ClinchAnchorPoint;
	ClinchMove->FallStartSimTimeMs                = YanInputs->ClinchFallStartSimTimeMs;
	// 重力取自仿真本帧输入，与被抓方下发的效果同源，双方的下落解算据此一致
	ClinchMove->Gravity                           = DefaultSimInputs->Gravity;
	ClinchMove->PullDurationMs                    = PullDurationMs;
	ClinchMove->EaseExponent                      = EaseExponent;
	ClinchMove->MaxHoldSpeed                      = MaxHoldSpeed;
	ClinchMove->DurationMs                        = MaxDurationMs;
	// 发起方保留自由视角，朝向仍归移动模式处理
	ClinchMove->bControlOrientation = false;
	// 发起方松手即停：抓取意图本就是持续标志，无须再占用被抓方那个挣脱位——
	// 双方的技能若同帧写同一字段会互相覆盖
	ClinchMove->bCanBreakFree             = false;
	ClinchMove->bEndOnClinchIntentCleared = true;

	Simulation->QueueLayeredMove_Internal(ClinchMove);
}
