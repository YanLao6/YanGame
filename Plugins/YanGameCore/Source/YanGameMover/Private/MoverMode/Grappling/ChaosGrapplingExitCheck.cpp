#include "MoverMode/Grappling/ChaosGrapplingExitCheck.h"

#include "YanMoverDataModelTypes.h"
#include "MoverDataModelTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosGrapplingExitCheck)

UChaosGrapplingExitCheck::UChaosGrapplingExitCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;
}

FTransitionEvalResult UChaosGrapplingExitCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	// 钩锁激活状态从输入包读取，保证服务器 re-simulation 与客户端一致
	const FYanCharacterInputs* YanInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();
	if (!YanInputs || !YanInputs->bIsGrapplingActive || IsHookOutsideForwardHemisphere(Params.StartState.SyncState, Params.StartState.InputCmd))
	{
		return FTransitionEvalResult(TransitionToMode);
	}

	return FTransitionEvalResult::NoTransition;
}

bool UChaosGrapplingExitCheck::IsHookOutsideForwardHemisphere(const FMoverSyncState& SyncState, const FMoverInputCmdContext& InputCmd)
{
	const FMoverDefaultSyncState* DefaultSyncState = SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const FYanCharacterInputs* YanInputs = InputCmd.InputCollection.FindDataByType<FYanCharacterInputs>();
	const FCharacterDefaultInputs* CharacterInputs = InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	if (!DefaultSyncState || !YanInputs || !CharacterInputs)
	{
		return true;
	}

	const FVector ToHook = (YanInputs->GrappleHookLocation - DefaultSyncState->GetLocation_WorldSpace()).GetSafeNormal2D();
	const FVector ViewForward = FRotator(0.0, CharacterInputs->ControlRotation.Yaw, 0.0).Vector();
	// Pitch 与高度不参与判定；钩子处于玩家正上方或正下方时位于分界面上。
	return FVector::DotProduct(ViewForward, ToHook) <= UE_DOUBLE_SMALL_NUMBER;
}
