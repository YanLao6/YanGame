// 回放子系统声明。

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NetworkReplayStreaming.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "YanReplaySubsystem.generated.h"

#define UE_API YANGAMESESSION_API

class APlayerController;
class ULocalPlayer;
class UDemoNetDriver;

/** 一条可供界面展示的回放记录。 */
UCLASS(MinimalAPI, BlueprintType)
class UYanReplayListEntry : public UObject
{
	GENERATED_BODY()

public:
	FNetworkReplayStreamInfo StreamInfo;

	/** 展示用名称。 */
	UFUNCTION(BlueprintPure, Category=Replays)
	FString GetFriendlyName() const { return StreamInfo.FriendlyName; }

	/** 录制时间。 */
	UFUNCTION(BlueprintPure, Category=Replays)
	FDateTime GetTimestamp() const { return StreamInfo.Timestamp; }

	/** 时长。 */
	UFUNCTION(BlueprintPure, Category=Replays)
	FTimespan GetDuration() const { return FTimespan::FromMilliseconds(StreamInfo.LengthInMS); }

	/** 当前观看人数。 */
	UFUNCTION(BlueprintPure, Category=Replays)
	int32 GetNumViewers() const { return StreamInfo.NumViewers; }

	/** 对局尚未结束、仍在录制中时为真。 */
	UFUNCTION(BlueprintPure, Category=Replays)
	bool GetIsLive() const { return StreamInfo.bIsLive; }
};

/** 一次回放查询的结果集。 */
UCLASS(MinimalAPI, BlueprintType)
class UYanReplayList : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category=Replays)
	TArray<TObjectPtr<UYanReplayListEntry>> Results;
};

/**
 * 回放子系统。
 *
 * 负责客户端回放的录制、播放与本地留存清理。
 */
UCLASS(MinimalAPI)
class UYanReplaySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UE_API UYanReplaySubsystem();

	/** 当前平台是否支持回放。 */
	UFUNCTION(BlueprintCallable, Category = Replays, BlueprintPure = false)
	static UE_API bool DoesPlatformSupportReplays();

	/** 返回用于判定平台回放支持的特性 Tag。 */
	static UE_API FGameplayTag GetPlatformSupportTraitTag();

	/** 加载对应关卡并播放指定回放。 */
	UFUNCTION(BlueprintCallable, Category=Replays)
	UE_API void PlayReplay(UYanReplayListEntry* Replay);

	/** 玩家是否在设置中开启了自动录制对局回放 */
	static UE_API bool IsAutoRecordEnabledFor(const APlayerController* PlayerController);

	/** 开始录制客户端回放，并顺带清理超出留存份数的旧回放。 */
	UFUNCTION(BlueprintCallable, Category = Replays)
	UE_API void RecordClientReplay(APlayerController* PlayerController);

	/** 从最旧一份开始删除本地回放，直至剩余份数不超过 NumReplaysToKeep。 */
	UFUNCTION(BlueprintCallable, Category = Replays)
	UE_API void CleanupLocalReplays(ULocalPlayer* LocalPlayer, int32 NumReplaysToKeep);

	/** 在当前播放的回放中跳转到指定时间点。 */
	UFUNCTION(BlueprintCallable, Category=Replays)
	UE_API void SeekInActiveReplay(float TimeInSeconds);

	/** 当前回放的总时长，单位为秒。 */
	UFUNCTION(BlueprintCallable, Category = Replays, BlueprintPure = false)
	UE_API float GetReplayLengthInSeconds() const;

	/** 当前回放的播放进度，单位为秒。 */
	UFUNCTION(BlueprintCallable, Category=Replays, BlueprintPure=false)
	UE_API float GetReplayCurrentTime() const;

private:
	// 每次只删一份：单次删除可能触发服务器或存档查询，使回放列表失效，故删完需重新枚举
	void OnEnumerateStreamsCompleteForDelete(const FEnumerateStreamsResult& Result);
	void OnDeleteReplay(const FDeleteFinishedStreamResult& DeleteResult);

	UDemoNetDriver* GetDemoDriver() const;

	TSharedPtr<INetworkReplayStreamer> CurrentReplayStreamer;

	UPROPERTY()
	TObjectPtr<ULocalPlayer> LocalPlayerDeletingReplays;

	int32 DeletingReplaysNumberToKeep = 0;
};

#undef UE_API
