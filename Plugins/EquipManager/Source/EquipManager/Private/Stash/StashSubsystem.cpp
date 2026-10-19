// 仓库子系统实现。

#include "Stash/StashSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "ModularGameplayAbilitiesLogChannels.h"
#include "Stash/EquipManagerSettings.h"
#include "Stash/StashStorageProvider.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StashSubsystem)

namespace
{
	// 改动到写盘之间的节流窗口，单位秒。
	constexpr float StashFlushDelaySeconds = 2.0f;
}

//~Begin USubsystem Interface
void UStashSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const TSubclassOf<UStashStorageProvider> ProviderClass = UEquipManagerSettings::GetStorageProviderClass();
	StorageProvider                                        = NewObject<UStashStorageProvider>(this, ProviderClass);

	RequestLoad();
}

void UStashSubsystem::Deinitialize()
{
	// 关闭前把未落盘的改动写出去；本次写入未必来得及完成，但比直接丢弃可取。
	if (bDirty)
	{
		Flush();
	}

	if (FlushTickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FlushTickHandle);
		FlushTickHandle.Reset();
	}

	Super::Deinitialize();
}
//~End USubsystem Interface

void UStashSubsystem::RequestLoad()
{
	if (StorageProvider == nullptr)
	{
		return;
	}

	TWeakObjectPtr<UStashSubsystem> WeakThis(this);

	StorageProvider->LoadStash(GetLocalPlayer<ULocalPlayer>(), FStashLoadComplete::CreateLambda(
		[WeakThis](bool bSuccess, const FStashSnapshot& LoadedSnapshot)
		{
			UStashSubsystem* Subsystem = WeakThis.Get();
			if (Subsystem == nullptr)
			{
				return;
			}

			if (bSuccess)
			{
				Subsystem->Snapshot = LoadedSnapshot;
			}

			// 读取失败时保留空仓库，玩家仍可游玩，只是这一程的改动不会写回。
			Subsystem->bLoaded = bSuccess;
			Subsystem->OnStashChanged.Broadcast();
		}));
}

const FStashItemRecord* UStashSubsystem::FindItem(const FGuid& InstanceId) const
{
	return Snapshot.Items.FindByPredicate(
		[&InstanceId](const FStashItemRecord& Record) { return Record.InstanceId == InstanceId; });
}

bool UStashSubsystem::HasItem(FName ItemId) const
{
	return Snapshot.Items.ContainsByPredicate(
		[ItemId](const FStashItemRecord& Record) { return Record.ItemId == ItemId; });
}

FGuid UStashSubsystem::AddItem(FName ItemId, int32 StackCount)
{
	if (ItemId.IsNone())
	{
		return FGuid();
	}

	FStashItemRecord NewRecord;
	NewRecord.ItemId     = ItemId;
	NewRecord.InstanceId = FGuid::NewGuid();
	NewRecord.StackCount = FMath::Max(1, StackCount);

	Snapshot.Items.Add(NewRecord);

	MarkDirty();
	OnStashChanged.Broadcast();

	return NewRecord.InstanceId;
}

bool UStashSubsystem::RemoveItem(const FGuid& InstanceId)
{
	const int32 RemovedCount = Snapshot.Items.RemoveAll(
		[&InstanceId](const FStashItemRecord& Record) { return Record.InstanceId == InstanceId; });

	if (RemovedCount == 0)
	{
		return false;
	}

	// 配装引用的个体已不存在，留着会让决议反复丢弃同一条；就地清掉更省事。
	for (FPlayerLoadout& Loadout : Snapshot.Loadouts)
	{
		Loadout.Slots.RemoveAll(
			[&InstanceId](const FPlayerLoadoutSlot& Slot) { return Slot.InstanceId == InstanceId; });
	}

	MarkDirty();
	OnStashChanged.Broadcast();

	return true;
}

const FPlayerLoadout* UStashSubsystem::GetSelectedLoadout() const
{
	if (!Snapshot.Loadouts.IsValidIndex(Snapshot.SelectedLoadoutIndex))
	{
		return nullptr;
	}

	return &Snapshot.Loadouts[Snapshot.SelectedLoadoutIndex];
}

bool UStashSubsystem::SelectLoadout(int32 LoadoutIndex)
{
	if (!Snapshot.Loadouts.IsValidIndex(LoadoutIndex))
	{
		return false;
	}

	Snapshot.SelectedLoadoutIndex = LoadoutIndex;

	MarkDirty();
	OnStashChanged.Broadcast();

	return true;
}

int32 UStashSubsystem::SaveLoadout(int32 LoadoutIndex, const FPlayerLoadout& Loadout)
{
	int32 WrittenIndex = LoadoutIndex;

	if (Snapshot.Loadouts.IsValidIndex(LoadoutIndex))
	{
		Snapshot.Loadouts[LoadoutIndex] = Loadout;
	}
	else
	{
		WrittenIndex = Snapshot.Loadouts.Add(Loadout);
	}

	MarkDirty();
	OnStashChanged.Broadcast();

	return WrittenIndex;
}

FLoadoutRequest UStashSubsystem::MakeLoadoutRequest() const
{
	FLoadoutRequest Request;

	const FPlayerLoadout* SelectedLoadout = GetSelectedLoadout();
	if (SelectedLoadout == nullptr)
	{
		return Request;
	}

	for (const FPlayerLoadoutSlot& Slot : SelectedLoadout->Slots)
	{
		const FStashItemRecord* Record = FindItem(Slot.InstanceId);
		if (Record == nullptr)
		{
			// 配装存下之后物品被移除，该槽位沿用模式配置。
			continue;
		}

		FLoadoutRequestEntry Entry;
		Entry.SlotIndex = Slot.SlotIndex;
		Entry.ItemId    = Record->ItemId;

		Request.Entries.Add(Entry);
	}

	return Request;
}

void UStashSubsystem::MarkDirty()
{
	bDirty = true;

	if (FlushTickHandle.IsValid())
	{
		return;
	}

	FlushTickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UStashSubsystem::HandleFlushTick), StashFlushDelaySeconds);
}

bool UStashSubsystem::HandleFlushTick(float DeltaTime)
{
	FlushTickHandle.Reset();
	Flush();

	// 单次触发即可，后续改动会重新安排。
	return false;
}

void UStashSubsystem::Flush()
{
	if ((StorageProvider == nullptr) || !bDirty)
	{
		return;
	}

	if (!bLoaded)
	{
		// 读取未完成就写回，会把空仓库盖到玩家已有的存档上。
		UE_LOG(LogModularGameplayAbilities, Warning, TEXT("仓库尚未读取完成，本次写盘被跳过以免覆盖已有存档。"));
		return;
	}

	bDirty = false;

	StorageProvider->SaveStash(GetLocalPlayer<ULocalPlayer>(), Snapshot, FStashSaveComplete::CreateLambda(
		[](bool bSuccess)
		{
			if (!bSuccess)
			{
				UE_LOG(LogModularGameplayAbilities, Warning, TEXT("仓库写盘失败。"));
			}
		}));
}
