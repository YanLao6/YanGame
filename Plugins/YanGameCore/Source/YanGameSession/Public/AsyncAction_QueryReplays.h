// 回放查询异步动作声明。

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "UObject/ObjectPtr.h"
#include "AsyncAction_QueryReplays.generated.h"

#define UE_API YANGAMESESSION_API

class APlayerController;
class INetworkReplayStreamer;
class UYanReplayList;
struct FEnumerateStreamsResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FQueryReplayAsyncDelegate, UYanReplayList*, Results);

/**
 * 回放查询异步动作。
 *
 * 枚举本地可用的回放并按录制时间倒序返回。
 */
UCLASS(MinimalAPI)
class UAsyncAction_QueryReplays : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UE_API UAsyncAction_QueryReplays(const FObjectInitializer& ObjectInitializer);

	/** 发起一次回放查询。 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static UE_API UAsyncAction_QueryReplays* QueryReplays(APlayerController* PlayerController);

	//~Begin UBlueprintAsyncActionBase Interface
	UE_API virtual void Activate() override;
	//~End UBlueprintAsyncActionBase Interface

	/** 查询完成时触发。 */
	UPROPERTY(BlueprintAssignable)
	FQueryReplayAsyncDelegate QueryComplete;

private:
	void OnEnumerateStreamsComplete(const FEnumerateStreamsResult& Result);

	UPROPERTY()
	TObjectPtr<UYanReplayList> ResultList;

	TWeakObjectPtr<APlayerController> PlayerController;

	TSharedPtr<INetworkReplayStreamer> ReplayStreamer;
};

#undef UE_API
