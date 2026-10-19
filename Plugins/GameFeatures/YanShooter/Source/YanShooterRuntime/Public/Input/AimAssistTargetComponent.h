// 瞄准辅助目标组件声明。

#pragma once

#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "GameplayTagContainer.h"
#include "Input/IAimAssistTargetInterface.h"
#include "AimAssistTargetComponent.generated.h"

#define UE_API YANSHOOTERRUNTIME_API

/**
 * 瞄准辅助目标组件。
 *
 * 挂到任意 Actor 上即可让其被瞄准辅助管理器登记为候选目标，胶囊体本身即命中范围。
 */
UCLASS(MinimalAPI, BlueprintType, meta=(BlueprintSpawnableComponent))
class UAimAssistTargetComponent : public UCapsuleComponent, public IAimAssistTaget
{
	GENERATED_BODY()

public:
	//~Begin IAimAssistTaget Interface
	UE_API virtual void GatherTargetOptions(OUT FAimAssistTargetOptions& TargetData) override;
	//~End IAimAssistTaget Interface

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FAimAssistTargetOptions TargetData {};
};

#undef UE_API
