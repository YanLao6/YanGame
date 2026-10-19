// 装备管理组件实现。


#include "Equipment/EquipmentManagerComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/ActorChannel.h"
#include "Equipment/EquipmentDefinition.h"
#include "Equipment/EquipmentInstance.h"
#include "GameFramework/Pawn.h"
#include "Messages/VerbMessageHelpers.h"
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
		if ((Entry.Instance != nullptr) && OwnerComponent && (OwnerComponent->GetActiveInstance() == Entry.Instance))
		{
			// 生效中的实例被移除时才需要失效；未生效的实例本就没有生效过。
			OwnerComponent->SetActiveInstigator(nullptr);
		}
	}
}

void FEquipmentList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	// 装备实例到达只代表被持有，是否生效由快捷栏的生效意图决定，故此处不触发 OnEquipped。
	// 实例可能晚于该意图到达，因此到达时重新解析一次。
	if (OwnerComponent)
	{
		OwnerComponent->ResolveActiveInstance();
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
	return Cast<UAbilitySystemComponent>(UVerbMessageHelpers::GetAbilitySystemComponentFromObject(OwningActor));
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
	NewEntry.Instance = NewObject<UEquipmentInstance>(OwnerComponent->GetOwner(), InstanceType); // 待办：受 UE-127172 影响，暂时不能以组件作为 Outer。
	Result            = NewEntry.Instance;

	// 按装备定义生成附属 Actor，例如武器模型或挂件。实例创建即持有但尚未生效，故先隐藏。
	Result->SpawnEquipmentActors(EquipmentCDO->ActorsToSpawn);
	Result->SetEquipmentActorsHidden(true);

	// 技能由持有方在物品入槽时预授予，被动留待实例生效时再授予，故此处不授予任何内容。

	// 标记该条目已变化，使快速数组能够复制新增内容。
	MarkItemDirty(NewEntry);

	return Result;
}

void FEquipmentList::GrantEntryPassives(UEquipmentInstance* Instance)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}

	for (FAppliedEquipmentEntry& Entry : Entries)
	{
		if (Entry.Instance != Instance)
		{
			continue;
		}

		const UEquipmentDefinition* EquipmentCDO = GetDefault<UEquipmentDefinition>(Entry.EquipmentDefinition);
		for (const TObjectPtr<const UModularAbilitySet>& AbilitySet : EquipmentCDO->AbilitySetsToGrant)
		{
			if (AbilitySet != nullptr)
			{
				AbilitySet->GiveToAbilitySystem(Cast<UModularAbilitySystemComponent>(ASC), /*输入输出*/ &Entry.GrantedHandles, Instance, EModularAbilitySetGrantParts::PassivesOnly);
			}
		}
		return;
	}
}

void FEquipmentList::TakeBackEntryPassives(UEquipmentInstance* Instance)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}

	for (FAppliedEquipmentEntry& Entry : Entries)
	{
		if (Entry.Instance == Instance)
		{
			Entry.GrantedHandles.TakeFromAbilitySystem(Cast<UModularAbilitySystemComponent>(ASC));
			return;
		}
	}
}

void FEquipmentList::RemoveEntry(UEquipmentInstance* Instance)
{
	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FAppliedEquipmentEntry& Entry = *EntryIt;
		if (Entry.Instance == Instance)
		{
			if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
			{
				Entry.GrantedHandles.TakeFromAbilitySystem(Cast<UModularAbilitySystemComponent>(ASC));
			}

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
	// 生效结果须让没有快捷栏的其余客户端也能得知。
	DOREPLIFETIME(ThisClass, ReplicatedActiveInstance);
}

//~End UObject Interface

UEquipmentInstance* UEquipmentManagerComponent::EquipItem(TSubclassOf<UEquipmentDefinition> EquipmentClass)
{
	UEquipmentInstance* Result = nullptr;
	if (EquipmentClass != nullptr)
	{
		// 仅创建实例并生成隐藏的附属 Actor；生效由 ActivateInstance 单独驱动。
		Result = EquipmentList.AddEntry(EquipmentClass);
		if (Result != nullptr)
		{
			if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
			{
				// 启用注册式子对象复制时，需要显式登记新装备实例。
				AddReplicatedSubObject(Result);
			}
		}
	}
	return Result;
}

void UEquipmentManagerComponent::SetActiveInstigator(UObject* Instigator)
{
	ActiveInstigator = Instigator;
	ResolveActiveInstance();
}

void UEquipmentManagerComponent::ResolveActiveInstance()
{
	UEquipmentInstance* Resolved = nullptr;

	if (HasLocalEquipIntent())
	{
		if (ActiveInstigator != nullptr)
		{
			for (const FAppliedEquipmentEntry& Entry : EquipmentList.Entries)
			{
				if ((Entry.Instance != nullptr) && (Entry.Instance->GetInstigator() == ActiveInstigator))
				{
					Resolved = Entry.Instance;
					break;
				}
			}
		}
	}
	else if (ReplicatedActiveInstance != nullptr)
	{
		// 其余客户端没有快捷栏，无从推导意图，只能采用权威端的结论。
		// 条目移除与本字段的到达先后无保证，故校验它仍在列表中，避免应用一个正在销毁的实例。
		for (const FAppliedEquipmentEntry& Entry : EquipmentList.Entries)
		{
			if (Entry.Instance == ReplicatedActiveInstance)
			{
				Resolved = ReplicatedActiveInstance;
				break;
			}
		}
	}

	// 解析不到实例时保持全部失效，待其复制到达后由列表回调再解析一次。
	ApplyActiveInstance(Resolved);
}

bool UEquipmentManagerComponent::HasLocalEquipIntent() const
{
	// 快捷栏挂在控制器上：服务器持有全部控制器，拥有者客户端持有自己的那个，其余客户端都没有。
	if (GetOwner()->HasAuthority())
	{
		return true;
	}

	const APawn* OwningPawn = Cast<APawn>(GetOwner());
	return (OwningPawn != nullptr) && OwningPawn->IsLocallyControlled();
}

void UEquipmentManagerComponent::OnRep_ReplicatedActiveInstance()
{
	// 拥有者客户端仍以本地意图为准，其纠正由快捷栏的槽位回执驱动。
	ResolveActiveInstance();
}

void UEquipmentManagerComponent::ApplyActiveInstance(UEquipmentInstance* ItemInstance)
{
	if (ActiveInstance == ItemInstance)
	{
		return;
	}

	// 同一时刻只有一个实例生效，故先让旧实例失效。
	if (ActiveInstance != nullptr)
	{
		ActiveInstance->SetEquipmentActorsHidden(true);
		ActiveInstance->OnUnequipped();

		if (GetOwner()->HasAuthority())
		{
			EquipmentList.TakeBackEntryPassives(ActiveInstance);
		}
	}

	ActiveInstance = ItemInstance;

	if (GetOwner()->HasAuthority())
	{
		// 与已应用状态同步写入，卸下装备等不经由意图的路径因此也会同步到其余客户端。
		ReplicatedActiveInstance = ItemInstance;
	}

	if (ActiveInstance != nullptr)
	{
		// 被动只在服务器授予，其效果经由 ASC 自行复制到客户端。
		if (GetOwner()->HasAuthority())
		{
			EquipmentList.GrantEntryPassives(ActiveInstance);
		}

		ActiveInstance->SetEquipmentActorsHidden(false);
		ActiveInstance->OnEquipped();
	}
}

void UEquipmentManagerComponent::UnequipItem(UEquipmentInstance* ItemInstance)
{
	if (ItemInstance != nullptr)
	{
		// 销毁前先让其失效，确保被动被回收、OnUnequipped 被触发。
		if (ActiveInstance == ItemInstance)
		{
			ApplyActiveInstance(nullptr);
		}

		if (IsUsingRegisteredSubObjectList())
		{
			// 先从子对象复制列表中移除，避免继续向客户端同步该实例。
			RemoveReplicatedSubObject(ItemInstance);
		}

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

	BindOwnerMeshAnimInitialized();
}

void UEquipmentManagerComponent::BindOwnerMeshAnimInitialized()
{
	USkeletalMeshComponent* OwnerMesh = GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	if ((OwnerMesh == nullptr) || (BoundMesh.Get() == OwnerMesh))
	{
		return;
	}

	UnbindOwnerMeshAnimInitialized();

	OwnerMesh->OnAnimInitialized.AddDynamic(this, &ThisClass::OnOwnerMeshAnimInitialized);
	BoundMesh = OwnerMesh;
}

void UEquipmentManagerComponent::UnbindOwnerMeshAnimInitialized()
{
	if (USkeletalMeshComponent* PreviousMesh = BoundMesh.Get())
	{
		PreviousMesh->OnAnimInitialized.RemoveDynamic(this, &ThisClass::OnOwnerMeshAnimInitialized);
	}

	BoundMesh.Reset();
}

void UEquipmentManagerComponent::OnOwnerMeshAnimInitialized()
{
	// 未生效的实例其外观本就不该出现在网格上，重放只针对当前生效的那一件。
	if (ActiveInstance != nullptr)
	{
		ActiveInstance->RefreshCosmetics();
	}
}

void UEquipmentManagerComponent::UninitializeComponent()
{
	UnbindOwnerMeshAnimInitialized();

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
	// 只考察生效中的实例：其余槽位的装备虽被持有，但角色此刻并未拿着它们。
	if ((ActiveInstance != nullptr) && ActiveInstance->IsA(InstanceType))
	{
		return ActiveInstance;
	}

	return nullptr;
}

TArray<UEquipmentInstance*> UEquipmentManagerComponent::GetEquipmentInstancesOfType(TSubclassOf<UEquipmentInstance> InstanceType) const
{
	TArray<UEquipmentInstance*> Results;
	if ((ActiveInstance != nullptr) && ActiveInstance->IsA(InstanceType))
	{
		Results.Add(ActiveInstance);
	}
	return Results;
}
