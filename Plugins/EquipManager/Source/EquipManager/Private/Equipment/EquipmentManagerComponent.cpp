// 装备管理组件实现。


#include "Equipment/EquipmentManagerComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/ActorChannel.h"
#include "Equipment/EquipmentDefinition.h"
#include "Equipment/EquipmentInstance.h"
#include "Net/UnrealNetwork.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipmentManagerComponent)

//////////////////////////////////////////////////////////////////////
// 已装备条目

FString FAppliedEquipmentEntry::GetDebugString() const
{
	// 调试输出中同时展示实例名和定义名，便于定位当前装备来源。
	return FString::Printf(TEXT("%s of %s"), *GetNameSafe(Instance), *GetNameSafe(EquipmentDefinition.Get()));
}

//////////////////////////////////////////////////////////////////////
// 装备列表

//~Begin FFastArraySerializer Contract
void FEquipmentList::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	for (int32 Index : RemovedIndices)
	{
		const FAppliedEquipmentEntry& Entry = Entries[Index];
		if (Entry.Instance != nullptr)
		{
			// 客户端收到删除同步时，触发卸下回调。
			Entry.Instance->OnUnequipped();
		}
	}
}

void FEquipmentList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	for (int32 Index : AddedIndices)
	{
		const FAppliedEquipmentEntry& Entry = Entries[Index];
		if (Entry.Instance != nullptr)
		{
			// 客户端首次收到该装备时，触发装备完成回调。
			Entry.Instance->OnEquipped();
		}
	}
}

void FEquipmentList::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	// 当前装备条目没有需要在普通变更时执行的额外客户端逻辑，保留扩展点。
}
//~End FFastArraySerializer Contract

UAbilitySystemComponent* FEquipmentList::GetAbilitySystemComponent() const
{
	check(OwnerComponent);
	AActor* OwningActor = OwnerComponent->GetOwner();
	return Cast<UAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningActor));
}

UEquipmentInstance* FEquipmentList::AddEntry(TSubclassOf<UEquipmentDefinition> EquipmentDefinition)
{
	UEquipmentInstance* Result = nullptr;

	check(EquipmentDefinition != nullptr);
	check(OwnerComponent);
	check(OwnerComponent->GetOwner()->HasAuthority());

	// 读取装备定义默认对象，确定应实例化的运行时装备类型。
	const UEquipmentDefinition* EquipmentCDO = GetDefault<UEquipmentDefinition>(EquipmentDefinition);

	TSubclassOf<UEquipmentInstance> InstanceType = EquipmentCDO->InstanceType;
	if (InstanceType == nullptr)
	{
		InstanceType = UEquipmentInstance::StaticClass();
	}

	FAppliedEquipmentEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.EquipmentDefinition     = EquipmentDefinition;
	// 这里仍以 Actor 作为 Outer，用于规避当前已知引擎问题。
	NewEntry.Instance                = NewObject<UEquipmentInstance>(OwnerComponent->GetOwner(), InstanceType); // 待办：受 UE-127172 影响，暂时不能以组件作为 Outer。
	Result                           = NewEntry.Instance;

	/*
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		for (const TObjectPtr<const UAbilitySet>& AbilitySet : EquipmentCDO->AbilitySetsToGrant)
		{
			AbilitySet->GiveToAbilitySystem(ASC, /*输入输出#1# &NewEntry.GrantedHandles, Result);
		}
	}
	else
	{
	// 待办：这里后续可以补充警告日志。
	}

	*/
	// 按装备定义生成附属 Actor，例如武器模型或挂件。
	Result->SpawnEquipmentActors(EquipmentCDO->ActorsToSpawn);


	// 标记该条目已变化，使快速数组能够复制新增内容。
	MarkItemDirty(NewEntry);

	return Result;
}

void FEquipmentList::RemoveEntry(UEquipmentInstance* Instance)
{
	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FAppliedEquipmentEntry& Entry = *EntryIt;
		if (Entry.Instance == Instance)
		{
			/*if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
			{
				Entry.GrantedHandles.TakeFromAbilitySystem(ASC);
			}*/

			// 移除记录前先销毁该装备实例创建出的附属 Actor。
			Instance->DestroyEquipmentActors();


			// 从快速数组中删除条目，并通知复制系统整个数组发生结构变化。
			EntryIt.RemoveCurrent();
			MarkArrayDirty();
		}
	}
}

//////////////////////////////////////////////////////////////////////
// 装备管理组件

UEquipmentManagerComponent::UEquipmentManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	  , EquipmentList(this)
{
	// 装备列表需要在服务器和客户端之间保持同步。
	SetIsReplicatedByDefault(true);
	bWantsInitializeComponent = true;
}

//~Begin UObject Interface
void UEquipmentManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 同步整个快速数组结构，条目内部对象再由子对象复制补充。
	DOREPLIFETIME(ThisClass, EquipmentList);
}
//~End UObject Interface

UEquipmentInstance* UEquipmentManagerComponent::EquipItem(TSubclassOf<UEquipmentDefinition> EquipmentClass)
{
	UEquipmentInstance* Result = nullptr;
	if (EquipmentClass != nullptr)
	{
		// 先向内部列表添加条目并创建对应装备实例。
		Result = EquipmentList.AddEntry(EquipmentClass);
		if (Result != nullptr)
		{
			// 服务器本地立即执行装备回调，客户端会在复制到达时自行触发。
			Result->OnEquipped();

			if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
			{
				// 启用注册式子对象复制时，需要显式登记新装备实例。
				AddReplicatedSubObject(Result);
			}
		}
	}
	return Result;
}

void UEquipmentManagerComponent::UnequipItem(UEquipmentInstance* ItemInstance)
{
	if (ItemInstance != nullptr)
	{
		if (IsUsingRegisteredSubObjectList())
		{
			// 先从子对象复制列表中移除，避免继续向客户端同步该实例。
			RemoveReplicatedSubObject(ItemInstance);
		}

		// 执行卸下回调后，再从内部列表中移除该装备。
		ItemInstance->OnUnequipped();
		EquipmentList.RemoveEntry(ItemInstance);
	}
}

//~Begin UObject Interface
bool UEquipmentManagerComponent::ReplicateSubobjects(UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	// 显式复制每个装备实例，保证其运行时状态可被客户端接收。
	for (FAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		UEquipmentInstance* Instance = Entry.Instance;

		if (IsValid(Instance))
		{
			WroteSomething |= Channel->ReplicateSubobject(Instance, *Bunch, *RepFlags);
		}
	}

	return WroteSomething;
}
//~End UObject Interface

//~Begin UActorComponent Interface
void UEquipmentManagerComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UEquipmentManagerComponent::UninitializeComponent()
{
	TArray<UEquipmentInstance*> AllEquipmentInstances;

	// 先收集全部实例，再统一卸下，避免遍历过程中直接改动列表。
	for (const FAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		AllEquipmentInstances.Add(Entry.Instance);
	}

	for (UEquipmentInstance* EquipInstance : AllEquipmentInstances)
	{
		UnequipItem(EquipInstance);
	}

	Super::UninitializeComponent();
}

void UEquipmentManagerComponent::ReadyForReplication()
{
	Super::ReadyForReplication();

	// 在复制系统就绪后补注册已有装备实例，防止遗漏早于该时机创建的对象。
	if (IsUsingRegisteredSubObjectList())
	{
		for (const FAppliedEquipmentEntry& Entry : EquipmentList.Entries)
		{
			UEquipmentInstance* Instance = Entry.Instance;

			if (IsValid(Instance))
			{
				AddReplicatedSubObject(Instance);
			}
		}
	}
}
//~End UActorComponent Interface

UEquipmentInstance* UEquipmentManagerComponent::GetFirstInstanceOfType(TSubclassOf<UEquipmentInstance> InstanceType)
{
	// 返回找到的第一个匹配实例，适合查询当前主武器等唯一装备。
	for (FAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		if (UEquipmentInstance* Instance = Entry.Instance)
		{
			if (Instance->IsA(InstanceType))
			{
				return Instance;
			}
		}
	}

	return nullptr;
}

TArray<UEquipmentInstance*> UEquipmentManagerComponent::GetEquipmentInstancesOfType(TSubclassOf<UEquipmentInstance> InstanceType) const
{
	TArray<UEquipmentInstance*> Results;
	// 遍历全部装备条目，筛出满足类型要求的实例集合。
	for (const FAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		if (UEquipmentInstance* Instance = Entry.Instance)
		{
			if (Instance->IsA(InstanceType))
			{
				Results.Add(Instance);
			}
		}
	}
	return Results;
}
