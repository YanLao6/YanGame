// 等待体验就绪的异步动作声明。

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "AsyncAction_ExperienceReady.generated.h"

#define UE_API MODULARGAMEPLAYEXPERIENCES_API

class AGameStateBase;
class UModularExperienceDefinition;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FExperienceReadyAsyncDelegate);

/**
 * 等待体验就绪的异步动作。
 *
 * 依次等待 GameState 出现与体验加载完成，随后触发 OnReady。
 * 体验若已加载，仍会延后一帧再触发，以免调用方写出依赖同步返回的逻辑。
 */
UCLASS(MinimalAPI)
class UAsyncAction_ExperienceReady : public UBlueprintAsyncActionBase
{
	GENERATED_UCLASS_BODY()

public:
	/** 等待体验确定并加载完成。 */
	UFUNCTION(BlueprintCallable, meta=(WorldContext = "WorldContextObject", BlueprintInternalUseOnly="true"))
	static UE_API UAsyncAction_ExperienceReady* WaitForExperienceReady(UObject* WorldContextObject);

	//~Begin UBlueprintAsyncActionBase Interface
	UE_API virtual void Activate() override;
	//~End UBlueprintAsyncActionBase Interface

	/** 体验已确定并加载完成时触发。 */
	UPROPERTY(BlueprintAssignable)
	FExperienceReadyAsyncDelegate OnReady;

private:
	void HandleGameStateSet(AGameStateBase* GameState);
	void ListenToExperienceLoading(AGameStateBase* GameState);
	void HandleExperienceLoaded(const UModularExperienceDefinition* CurrentExperience);
	void BroadcastReady();

	TWeakObjectPtr<UWorld> WorldPtr;
};

#undef UE_API
