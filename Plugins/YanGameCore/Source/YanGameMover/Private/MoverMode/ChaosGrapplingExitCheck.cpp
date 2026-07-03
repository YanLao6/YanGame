#include "MoverMode/ChaosGrapplingExitCheck.h"

#include "YanMoverDataModelTypes.h"

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
	if (!YanInputs || !YanInputs->bIsGrapplingActive)
	{
		return FTransitionEvalResult(TransitionToMode);
	}

	return FTransitionEvalResult::NoTransition;
}
