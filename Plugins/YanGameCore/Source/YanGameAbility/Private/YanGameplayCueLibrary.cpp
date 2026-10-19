#include "YanGameplayCueLibrary.h"

#include "GameFramework/Pawn.h"
#include "GameplayCueManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanGameplayCueLibrary)

namespace
{
	// 预测份的标记值；服务器多播的那份保持 RawMagnitude 默认的 0
	constexpr float PredictedCueMagnitude = 1.0f;
}

void UYanGameplayCueLibrary::ExecuteGameplayCuePredicted(AActor* Target, FGameplayTag GameplayCueTag, FGameplayCueParameters Parameters)
{
	if (Target == nullptr || !GameplayCueTag.IsValid())
	{
		return;
	}

	Parameters.RawMagnitude = PredictedCueMagnitude;
	UGameplayCueManager::ExecuteGameplayCue_NonReplicated(Target, GameplayCueTag, Parameters);
}

bool UYanGameplayCueLibrary::ShouldHandlePredictableCue(const AActor* Target, const FGameplayCueParameters& Parameters)
{
	if (Parameters.RawMagnitude == PredictedCueMagnitude)
	{
		return true;
	}

	const APawn* TargetPawn = Cast<APawn>(Target);
	return TargetPawn == nullptr || !TargetPawn->IsLocallyControlled();
}
