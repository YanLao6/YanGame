// 战斗嘉奖宿主控件实现。

#include "Accolades/YanAccoladeHostWidget.h"

#include "DataRegistrySubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Messages/ModularNotificationMessage.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "YanShooterLogs.h"
#include "YanShooterTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanAccoladeHostWidget)

// 嘉奖数据所在的 DataRegistry
static const FName NAME_AccoladeRegistryID("Accolades");

void UYanAccoladeHostWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
	ListenerHandle = MessageSubsystem.RegisterListener(TAG_Modular_AddNotification_Message, this, &ThisClass::OnNotificationMessage);
}

void UYanAccoladeHostWidget::NativeDestruct()
{
	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
	MessageSubsystem.UnregisterListener(ListenerHandle);

	CancelAsyncLoading();

	Super::NativeDestruct();
}

void UYanAccoladeHostWidget::OnNotificationMessage(FGameplayTag Channel, const FModularNotificationMessage& Notification)
{
	if (Notification.TargetChannel == YanShooterTags::TAG_Message_Accolade)
	{
		// 指定了目标玩家的通知只在该玩家本地显示
		if (Notification.TargetPlayer != nullptr)
		{
			const APlayerController* PC = GetOwningPlayer();
			if ((PC == nullptr) || (PC->PlayerState != Notification.TargetPlayer))
			{
				return;
			}
		}

		const int32 NextID = AllocatedSequenceID;
		++AllocatedSequenceID;

		const FDataRegistryId ItemID(NAME_AccoladeRegistryID, Notification.PayloadTag.GetTagName());
		if (!UDataRegistrySubsystem::Get()->AcquireItem(ItemID, FDataRegistryItemAcquiredCallback::CreateUObject(this, &ThisClass::OnRegistryLoadCompleted, NextID)))
		{
			UE_LOG(LogYanShooter, Error, TEXT("未能取到 Tag %s 对应的嘉奖数据，该嘉奖将不会展示。"), *Notification.PayloadTag.GetTagName().ToString());
			--AllocatedSequenceID;
		}
	}
}

void UYanAccoladeHostWidget::OnRegistryLoadCompleted(const FDataRegistryAcquireResult& AccoladeHandle, int32 SequenceID)
{
	if (const FYanAccoladeDefinitionRow* AccoladeRow = AccoladeHandle.GetItem<FYanAccoladeDefinitionRow>())
	{
		FPendingAccoladeEntry& PendingEntry = PendingAccoladeLoads.AddDefaulted_GetRef();
		PendingEntry.Row = *AccoladeRow;
		PendingEntry.SequenceID = SequenceID;

		TArray<FSoftObjectPath> AssetsToLoad;
		AssetsToLoad.Add(AccoladeRow->Sound.ToSoftObjectPath());
		AssetsToLoad.Add(AccoladeRow->Icon.ToSoftObjectPath());
		AsyncLoad(AssetsToLoad, [this, SequenceID]
		{
			FPendingAccoladeEntry* EntryThatFinishedLoading = PendingAccoladeLoads.FindByPredicate(
				[SequenceID](const FPendingAccoladeEntry& Entry) { return Entry.SequenceID == SequenceID; });
			if (ensure(EntryThatFinishedLoading))
			{
				EntryThatFinishedLoading->Sound = EntryThatFinishedLoading->Row.Sound.Get();
				EntryThatFinishedLoading->Icon = EntryThatFinishedLoading->Row.Icon.Get();
				EntryThatFinishedLoading->bFinishedLoading = true;
				ConsiderLoadedAccolades();
			}
		});
		StartAsyncLoading();
	}
	else
	{
		ensure(false);
	}
}

void UYanAccoladeHostWidget::ConsiderLoadedAccolades()
{
	// 只按请求顺序放行，避免乱序完成的加载打乱展示次序
	int32 PendingIndexToDisplay;
	do
	{
		PendingIndexToDisplay = PendingAccoladeLoads.IndexOfByPredicate(
			[DesiredID = NextDisplaySequenceID](const FPendingAccoladeEntry& Entry)
			{
				return Entry.bFinishedLoading && Entry.SequenceID == DesiredID;
			});
		if (PendingIndexToDisplay != INDEX_NONE)
		{
			FPendingAccoladeEntry Entry = MoveTemp(PendingAccoladeLoads[PendingIndexToDisplay]);
			PendingAccoladeLoads.RemoveAtSwap(PendingIndexToDisplay);

			ProcessLoadedAccolade(Entry);
			++NextDisplaySequenceID;
		}
	}
	while (PendingIndexToDisplay != INDEX_NONE);
}

void UYanAccoladeHostWidget::ProcessLoadedAccolade(const FPendingAccoladeEntry& Entry)
{
	if (Entry.Row.LocationTag == LocationName)
	{
		bool bRecreateWidget = PendingAccoladeDisplays.Num() == 0;
		for (int32 Index = 0; Index < PendingAccoladeDisplays.Num(); )
		{
			if (PendingAccoladeDisplays[Index].Row.AccoladeTags.HasAny(Entry.Row.CancelAccoladesWithTag))
			{
				if (UUserWidget* OldWidget = PendingAccoladeDisplays[Index].AllocatedWidget)
				{
					DestroyAccoladeWidget(OldWidget);
					bRecreateWidget = true;
				}
				PendingAccoladeDisplays.RemoveAt(Index);
			}
			else
			{
				++Index;
			}
		}

		PendingAccoladeDisplays.Add(Entry);

		if (bRecreateWidget)
		{
			DisplayNextAccolade();
		}
	}
}

void UYanAccoladeHostWidget::DisplayNextAccolade()
{
	if (PendingAccoladeDisplays.Num() > 0)
	{
		FPendingAccoladeEntry& Entry = PendingAccoladeDisplays[0];

		GetWorld()->GetTimerManager().SetTimer(NextTimeToReconsiderHandle, this, &ThisClass::PopDisplayedAccolade, Entry.Row.DisplayDuration);
		Entry.AllocatedWidget = CreateAccoladeWidget(Entry);
	}
}

void UYanAccoladeHostWidget::PopDisplayedAccolade()
{
	if (PendingAccoladeDisplays.Num() > 0)
	{
		if (UUserWidget* OldWidget = PendingAccoladeDisplays[0].AllocatedWidget)
		{
			DestroyAccoladeWidget(OldWidget);
		}
		PendingAccoladeDisplays.RemoveAt(0);
	}

	DisplayNextAccolade();
}
