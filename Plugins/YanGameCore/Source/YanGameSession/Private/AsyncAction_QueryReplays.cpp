// 回放查询异步动作实现。

#include "AsyncAction_QueryReplays.h"

#include "Algo/Sort.h"
#include "GameFramework/PlayerController.h"
#include "Templates/Greater.h"
#include "YanReplaySubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AsyncAction_QueryReplays)

UAsyncAction_QueryReplays::UAsyncAction_QueryReplays(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAsyncAction_QueryReplays* UAsyncAction_QueryReplays::QueryReplays(APlayerController* InPlayerController)
{
	UAsyncAction_QueryReplays* Action = nullptr;

	if (InPlayerController != nullptr)
	{
		Action = NewObject<UAsyncAction_QueryReplays>();
		Action->PlayerController = InPlayerController;
	}

	return Action;
}

void UAsyncAction_QueryReplays::Activate()
{
	ReplayStreamer = FNetworkReplayStreaming::Get().GetFactory().CreateReplayStreamer();

	ResultList = NewObject<UYanReplayList>();
	if (ReplayStreamer.IsValid())
	{
		const FNetworkReplayVersion EnumerateStreamsVersion = FNetworkVersion::GetReplayVersion();

		ReplayStreamer->EnumerateStreams(
			EnumerateStreamsVersion, INDEX_NONE, FString(), TArray<FString>(),
			FEnumerateStreamsCallback::CreateUObject(this, &ThisClass::OnEnumerateStreamsComplete));
	}
	else
	{
		QueryComplete.Broadcast(ResultList);
	}
}

void UAsyncAction_QueryReplays::OnEnumerateStreamsComplete(const FEnumerateStreamsResult& Result)
{
	for (const FNetworkReplayStreamInfo& StreamInfo : Result.FoundStreams)
	{
		UYanReplayListEntry* NewReplayEntry = NewObject<UYanReplayListEntry>(ResultList);
		NewReplayEntry->StreamInfo = StreamInfo;
		ResultList->Results.Add(NewReplayEntry);
	}

	// 按录制时间倒序，最新的排在前面
	Algo::SortBy(ResultList->Results,
		[](const TObjectPtr<UYanReplayListEntry>& Data) { return Data->StreamInfo.Timestamp.GetTicks(); },
		TGreater<>());

	QueryComplete.Broadcast(ResultList);
}
