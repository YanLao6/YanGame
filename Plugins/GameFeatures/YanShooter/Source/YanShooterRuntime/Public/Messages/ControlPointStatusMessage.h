// 据点状态消息声明。

#pragma once

#include "CoreMinimal.h"
#include "ControlPointStatusMessage.generated.h"

/** 据点归属发生变化时广播。 */
USTRUCT(BlueprintType)
struct FControlPointStatusMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category=Gameplay)
	TObjectPtr<AActor> ControlPoint = nullptr;

	UPROPERTY(BlueprintReadWrite, Category=Gameplay)
	int32 OwnerTeamID = 0;
};
