// 回放子系统实现。

#include "YanReplaySubsystem.h"

#include "Algo/Sort.h"
#include "CommonUISettings.h"
#include "Engine/DemoNetDriver.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "ICommonUIModule.h"
#include "Internationalization/Text.h"
#include "Misc/DateTime.h"
#include "Player/YanLocalPlayer.h"
#include "Settings/YanSettingsLocal.h"
#include "Templates/Greater.h"
#include "YanGameSessionLogs.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanReplaySubsystem)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Platform_Trait_ReplaySupport, "Platform.Trait.ReplaySupport");

UYanReplaySubsystem::UYanReplaySubsystem()
{
}

bool UYanReplaySubsystem::DoesPlatformSupportReplays()
{
	return ICommonUIModule::GetSettings().GetPlatformTraits().HasTag(GetPlatformSupportTraitTag());
}

FGameplayTag UYanReplaySubsystem::GetPlatformSupportTraitTag()
{
	return TAG_Platform_Trait_ReplaySupport.GetTag();
}

void UYanReplaySubsystem::PlayReplay(UYanReplayListEntry* Replay)
{
	if (Replay != nullptr)
	{
		GetGameInstance()->PlayReplay(Replay->StreamInfo.Name);
	}
}

bool UYanReplaySubsystem::IsAutoRecordEnabledFor(const APlayerController* PlayerController)
{
	const UYanLocalPlayer* const YanLocalPlayer = PlayerController ? Cast<UYanLocalPlayer>(PlayerController->GetLocalPlayer()) : nullptr;

	return YanLocalPlayer && YanLocalPlayer->GetLocalSettings()->ShouldAutoRecordReplays();
}

void UYanReplaySubsystem::RecordClientReplay(APlayerController* PlayerController)
{
	if (ensure(DoesPlatformSupportReplays() && PlayerController))
	{
		const FText FriendlyNameText = FText::Format(
			NSLOCTEXT("Yan", "YanReplayName_Format", "客户端回放 {0}"),
			FText::AsDateTime(FDateTime::UtcNow(), EDateTimeStyle::Short, EDateTimeStyle::Short));
		GetGameInstance()->StartRecordingReplay(FString(), FriendlyNameText.ToString());

		if (UYanLocalPlayer* YanLocalPlayer = Cast<UYanLocalPlayer>(PlayerController->GetLocalPlayer()))
		{
			CleanupLocalReplays(YanLocalPlayer, YanLocalPlayer->GetLocalSettings()->GetNumberOfReplaysToKeep());
		}
	}
}

void UYanReplaySubsystem::CleanupLocalReplays(ULocalPlayer* LocalPlayer, int32 NumReplaysToKeep)
{
	if (LocalPlayer != nullptr && LocalPlayerDeletingReplays == nullptr && NumReplaysToKeep != 0)
	{
		LocalPlayerDeletingReplays = LocalPlayer;
		DeletingReplaysNumberToKeep = NumReplaysToKeep;

		CurrentReplayStreamer = FNetworkReplayStreaming::Get().GetFactory().CreateReplayStreamer();
		if (CurrentReplayStreamer.IsValid())
		{
			// 用默认版本枚举，以便把旧版本录下的回放也纳入清理
			const FNetworkReplayVersion EnumerateStreamsVersion;

			CurrentReplayStreamer->EnumerateStreams(
				EnumerateStreamsVersion, LocalPlayer->GetPlatformUserIndex(), FString(), TArray<FString>(),
				FEnumerateStreamsCallback::CreateUObject(this, &ThisClass::OnEnumerateStreamsCompleteForDelete));
		}
	}
}

void UYanReplaySubsystem::OnEnumerateStreamsCompleteForDelete(const FEnumerateStreamsResult& Result)
{
	if (!CurrentReplayStreamer.IsValid() || !IsValid(LocalPlayerDeletingReplays))
	{
		return;
	}

	TArray<FNetworkReplayStreamInfo> StreamsToDelete;
	for (const FNetworkReplayStreamInfo& StreamInfo : Result.FoundStreams)
	{
		// 标记为保留的回放不参与清理
		if (!StreamInfo.bShouldKeep)
		{
			StreamsToDelete.Add(StreamInfo);
		}
	}

	Algo::SortBy(StreamsToDelete, [](const FNetworkReplayStreamInfo& Data) { return Data.Timestamp.GetTicks(); }, TGreater<>());

	if (const UDemoNetDriver* DemoDriver = GetDemoDriver())
	{
		if (DemoDriver->IsRecording())
		{
			// 正在录制时，进行中的那一份未必出现在枚举结果里，缺失则补一个占位以保证留存计数正确
			if (StreamsToDelete.Num() > 0 && !StreamsToDelete[0].bIsLive)
			{
				StreamsToDelete.Insert(FNetworkReplayStreamInfo(), 0);
			}
		}
	}

	if (StreamsToDelete.Num() > DeletingReplaysNumberToKeep)
	{
		const FString ReplayName = StreamsToDelete[DeletingReplaysNumberToKeep].Name;
		UE_LOG(LogYanSession, Log, TEXT("回放子系统请求删除回放 %s。"), *ReplayName);
		CurrentReplayStreamer->DeleteFinishedStream(
			ReplayName, LocalPlayerDeletingReplays->GetPlatformUserIndex(),
			FDeleteFinishedStreamCallback::CreateUObject(this, &ThisClass::OnDeleteReplay));
	}
	else
	{
		CurrentReplayStreamer = nullptr;
		LocalPlayerDeletingReplays = nullptr;
		DeletingReplaysNumberToKeep = 0;
	}
}

void UYanReplaySubsystem::OnDeleteReplay(const FDeleteFinishedStreamResult& DeleteResult)
{
	if (!CurrentReplayStreamer.IsValid() || !IsValid(LocalPlayerDeletingReplays))
	{
		return;
	}

	if (DeleteResult.WasSuccessful())
	{
		// 重新枚举，确认是否已降到留存份数之内
		const FNetworkReplayVersion EnumerateStreamsVersion;

		CurrentReplayStreamer->EnumerateStreams(
			EnumerateStreamsVersion, LocalPlayerDeletingReplays->GetPlatformUserIndex(), FString(), TArray<FString>(),
			FEnumerateStreamsCallback::CreateUObject(this, &ThisClass::OnEnumerateStreamsCompleteForDelete));
	}
	else
	{
		UE_LOG(LogYanSession, Warning, TEXT("删除回放失败，错误码 %d。"), (int32)DeleteResult.Result);

		CurrentReplayStreamer = nullptr;
		LocalPlayerDeletingReplays = nullptr;
		DeletingReplaysNumberToKeep = 0;
	}
}

void UYanReplaySubsystem::SeekInActiveReplay(float TimeInSeconds)
{
	if (UDemoNetDriver* DemoDriver = GetDemoDriver())
	{
		DemoDriver->GotoTimeInSeconds(TimeInSeconds);
	}
}

float UYanReplaySubsystem::GetReplayLengthInSeconds() const
{
	if (const UDemoNetDriver* DemoDriver = GetDemoDriver())
	{
		return DemoDriver->GetDemoTotalTime();
	}
	return 0.0f;
}

float UYanReplaySubsystem::GetReplayCurrentTime() const
{
	if (const UDemoNetDriver* DemoDriver = GetDemoDriver())
	{
		return DemoDriver->GetDemoCurrentTime();
	}
	return 0.0f;
}

UDemoNetDriver* UYanReplaySubsystem::GetDemoDriver() const
{
	if (const UWorld* World = GetGameInstance()->GetWorld())
	{
		return World->GetDemoNetDriver();
	}
	return nullptr;
}
