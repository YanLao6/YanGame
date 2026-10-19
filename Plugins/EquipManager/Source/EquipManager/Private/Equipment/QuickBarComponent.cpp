// 快捷栏组件实现。


#include "Equipment/QuickBarComponent.h"

#include "ActorComponent/ModularAbilitySystemComponent.h"
#include "Equipment/EquipmentDefinition.h"
#include "Equipment/EquipmentInstance.h"
#include "Equipment/EquipmentManagerComponent.h"
#include "Equipment/QuickBarConfigComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Inventory/InventoryFragment_EquippableItem.h"
#include "Inventory/InventoryItemInstance.h"
#include "Inventory/InventoryManagerComponent.h"
#include "ItemDisplay/ItemDisplayTypes.h"
#include "Messages/VerbMessageHelpers.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(QuickBarComponent)


UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EM_QuickBar_Message_SlotsChanged, "EM.QuickBar.Message.SlotsChanged");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EM_QuickBar_Message_ActiveIndexChanged, "EM.QuickBar.Message.ActiveIndexChanged");

// 槽位按键 Tag 的约定前缀，拼上槽位索引即为该槽位的输入 Tag。
static const TCHAR* QuickBarSlotInputTagPrefix = TEXT("InputTag.QuickBar.Slot");

// 槽位门禁 Tag 的约定前缀，拼上槽位索引即为该槽位的门禁 Tag。
static const TCHAR* QuickBarSlotGateTagPrefix = TEXT("Ability.Gate.QuickBar.Slot");

UQuickBarComponent::UQuickBarComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 快捷栏槽位与激活状态都需要同步到客户端。
	SetIsReplicatedByDefault(true);
}

//~Begin UObject Interface
void UQuickBarComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 同步槽位内容和当前激活索引。
	DOREPLIFETIME(ThisClass, Slots);
	DOREPLIFETIME(ThisClass, ActiveSlotIndex);
}
//~End UObject Interface

//~Begin UActorComponent Interface
void UQuickBarComponent::BeginPlay()
{
	// 配置组件可能先于本组件挂入，此时它推送的配置已错过，故启动时再主动拉取一次。
	if (const AActor* OwningActor = GetOwner())
	{
		if (const UQuickBarConfigComponent* Config = OwningActor->FindComponentByClass<UQuickBarConfigComponent>())
		{
			ApplyConfigOverride(Config->GetNumSlots(), Config->GetSlotRules());
		}
	}

	EnsureSlotArrays();

	// 快捷栏随 Controller 存续而装备实例绑在 Pawn 上，故每次改换 Pawn 都要重放一次。
	if (AController* OwningController = Cast<AController>(GetOwner()))
	{
		OwningController->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessedPawnChanged);

		// 本组件由 GameFeature 动态挂入，可能晚于开局那次 Possess，其广播因而收不到，须补一次；
		// 重生时组件已在监听，走的是上面的委托。
		if (OwningController->GetPawn() != nullptr)
		{
			SyncSlotsToCurrentPawn();
		}
	}

	Super::BeginPlay();
}

void UQuickBarComponent::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (NewPawn != nullptr)
	{
		SyncSlotsToCurrentPawn();
	}
}

void UQuickBarComponent::SyncSlotsToCurrentPawn()
{
	const AController* OwningController = Cast<AController>(GetOwner());
	if ((OwningController == nullptr) || (OwningController->GetPawn() == nullptr))
	{
		// Pawn 已不在，留待下一次 Possess 时重放。
		return;
	}

	// 技能授予与门禁登记都依赖 PlayerState 上的 ASC，而它在两端都可能晚于本组件到达：
	// 服务器上其初始化晚于 Possess，客户端上 PlayerState 本身就是复制过来的。
	// 此时若照常往下走，装备实例会建起来而技能与门禁被静默跳过，
	// 表现为「武器在手却打不响」，故推迟到下一帧重试。
	if (FindAbilitySystemComponent() == nullptr)
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::SyncSlotsToCurrentPawn);
		return;
	}

	// 本组件是槽位门禁的唯一解释者，登记须早于任何一次激活尝试。
	// 借用上面这道 ASC 就绪等待：ASC 随 PlayerState 存续，故一次登记可跨多具 Pawn 生效。
	EnsureGateProviderRegistered();

	if (GetOwner()->HasAuthority())
	{
		EnsureSlotArrays();

		for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
		{
			if (Slots[SlotIndex] == nullptr)
			{
				continue;
			}

			// ASC 挂在 PlayerState 上，跨 Pawn 存续，重复授予会让同一技能叠加多份，故先收回。
			TakeBackSlotAbilities(SlotIndex);
			GrantSlotAbilities(SlotIndex);

			// 旧实例已随上一具 Pawn 一同销毁，其管理组件也不复存在，因此只丢弃引用而不走卸下流程。
			SlotEquipmentInstances[SlotIndex] = nullptr;
			CreateSlotEquipment(SlotIndex);
		}
	}

	// 生效意图按当前激活槽位重新落一次，两端各自执行；门禁无须重放，它在激活时才求值。
	RefreshActiveSlotEquipment();
}

void UQuickBarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UModularAbilitySystemComponent* AbilitySystem = RegisteredGateAbilitySystem.Get())
	{
		AbilitySystem->UnregisterAbilityGateProvider(this);
	}
	RegisteredGateAbilitySystem.Reset();

	Super::EndPlay(EndPlayReason);
}
//~End UActorComponent Interface

void UQuickBarComponent::ApplyConfigOverride(int32 InNumSlots, const TArray<FQuickBarSlotRule>& InSlotRules)
{
	NumSlots = FMath::Max(0, InNumSlots);
	SlotRules = InSlotRules;

	// 槽位数被调小时，只有在全部槽位都还空着的情况下才收缩数组，避免丢弃已入槽的物品。
	if (Slots.Num() > NumSlots)
	{
		const bool bAllSlotsEmpty = !Slots.ContainsByPredicate(
			[](const UInventoryItemInstance* Item) { return Item != nullptr; });
		if (bAllSlotsEmpty)
		{
			Slots.SetNum(NumSlots);
			SlotGrantedHandles.SetNum(NumSlots);
			SlotEquipmentInstances.SetNum(NumSlots);
		}
	}

	EnsureSlotArrays();
}

void UQuickBarComponent::EnsureSlotArrays()
{
	if (Slots.Num() < NumSlots)
	{
		// 补足固定槽位数量，确保后续按索引访问安全。
		Slots.AddDefaulted(NumSlots - Slots.Num());
	}

	// 授予句柄与装备实例均按索引与槽位一一对应，故与槽位数组等长。
	if (SlotGrantedHandles.Num() < Slots.Num())
	{
		SlotGrantedHandles.AddDefaulted(Slots.Num() - SlotGrantedHandles.Num());
	}
	if (SlotEquipmentInstances.Num() < Slots.Num())
	{
		SlotEquipmentInstances.AddDefaulted(Slots.Num() - SlotEquipmentInstances.Num());
	}
}

void UQuickBarComponent::CycleActiveSlotForward()
{
	if (Slots.Num() < 2)
	{
		return;
	}

	// 依本地生效索引推进，连续快速切换因此不会因等待权威值而原地打转。
	const int32 CurrentIndex = GetActiveSlotIndex();
	const int32 OldIndex     = (CurrentIndex < 0 ? Slots.Num()-1 : CurrentIndex);
	int32       NewIndex     = CurrentIndex;
	// 循环查找下一个非空槽位，跳过空槽。
	do
	{
		NewIndex = (NewIndex + 1) % Slots.Num();
		if (Slots[NewIndex] != nullptr)
		{
			SetActiveSlotIndex(NewIndex);
			return;
		}
	} 
	while (NewIndex != OldIndex);
}

void UQuickBarComponent::CycleActiveSlotBackward()
{
	if (Slots.Num() < 2)
	{
		return;
	}

	// 依本地生效索引推进，连续快速切换因此不会因等待权威值而原地打转。
	const int32 CurrentIndex = GetActiveSlotIndex();
	const int32 OldIndex     = (CurrentIndex < 0 ? Slots.Num()-1 : CurrentIndex);
	int32       NewIndex     = CurrentIndex;
	// 循环查找上一个非空槽位，跳过空槽。
	do
	{
		NewIndex = (NewIndex - 1 + Slots.Num()) % Slots.Num();
		if (Slots[NewIndex] != nullptr)
		{
			SetActiveSlotIndex(NewIndex);
			return;
		}
	} while (NewIndex != OldIndex);
}

void UQuickBarComponent::CreateSlotEquipment(int32 SlotIndex)
{
	if (!SlotEquipmentInstances.IsValidIndex(SlotIndex) || !Slots.IsValidIndex(SlotIndex))
	{
		return;
	}

	UInventoryItemInstance* SlotItem = Slots[SlotIndex];
	if (SlotItem == nullptr)
	{
		return;
	}

	const UInventoryFragment_EquippableItem* EquipInfo = SlotItem->FindFragmentByClass<UInventoryFragment_EquippableItem>();
	if ((EquipInfo == nullptr) || (EquipInfo->EquipmentDefinition == nullptr))
	{
		return;
	}

	UEquipmentManagerComponent* EquipmentManager = FindEquipmentManager();
	if (EquipmentManager == nullptr)
	{
		return;
	}

	// 物品入槽即创建实例并生成附属 Actor（此时隐藏），使之后的切槽只需改显隐而不再增删 Actor。
	UEquipmentInstance* NewInstance = EquipmentManager->EquipItem(EquipInfo->EquipmentDefinition);
	if (NewInstance != nullptr)
	{
		// 来源物品随槽位复制，两端据此把实例对应回槽位，故生效意图可以只用它表达。
		NewInstance->SetInstigator(SlotItem);
		SlotEquipmentInstances[SlotIndex] = NewInstance;
	}
}

void UQuickBarComponent::DestroySlotEquipment(int32 SlotIndex)
{
	if (!SlotEquipmentInstances.IsValidIndex(SlotIndex) || (SlotEquipmentInstances[SlotIndex] == nullptr))
	{
		return;
	}

	if (UEquipmentManagerComponent* EquipmentManager = FindEquipmentManager())
	{
		EquipmentManager->UnequipItem(SlotEquipmentInstances[SlotIndex]);
	}
	SlotEquipmentInstances[SlotIndex] = nullptr;
}

void UQuickBarComponent::RefreshActiveSlotEquipment()
{
	if (UEquipmentManagerComponent* EquipmentManager = FindEquipmentManager())
	{
		// 只表达「哪件物品该生效」，实例的解析与显隐交由管理组件处理，两端各自依本地激活索引调用。
		EquipmentManager->SetActiveInstigator(GetActiveSlotItem());
	}
}

UEquipmentManagerComponent* UQuickBarComponent::FindEquipmentManager() const
{
	if (AController* OwnerController = Cast<AController>(GetOwner()))
	{
		if (APawn* Pawn = OwnerController->GetPawn())
		{
			// 快捷栏挂在控制器上，真正的装备管理组件位于当前控制的 Pawn 上。
			return Pawn->FindComponentByClass<UEquipmentManagerComponent>();
		}
	}
	return nullptr;
}

UModularAbilitySystemComponent* UQuickBarComponent::FindAbilitySystemComponent() const
{
	// 由持有者解析到 PlayerState 上的 ASC；技能因此随 PlayerState 存续，不随 Pawn 销毁而丢失。
	return Cast<UModularAbilitySystemComponent>(UVerbMessageHelpers::GetAbilitySystemComponentFromObject(GetOwner()));
}

void UQuickBarComponent::GrantSlotAbilities(int32 SlotIndex)
{
	if (!SlotGrantedHandles.IsValidIndex(SlotIndex) || !Slots.IsValidIndex(SlotIndex))
	{
		return;
	}

	UModularAbilitySystemComponent* AbilitySystem = FindAbilitySystemComponent();
	UInventoryItemInstance*         SlotItem      = Slots[SlotIndex];
	if ((AbilitySystem == nullptr) || (SlotItem == nullptr))
	{
		return;
	}

	const UInventoryFragment_EquippableItem* EquipInfo = SlotItem->FindFragmentByClass<UInventoryFragment_EquippableItem>();
	if ((EquipInfo == nullptr) || (EquipInfo->EquipmentDefinition == nullptr))
	{
		return;
	}

	// 技能在入槽时即授予，其 SpecHandle 因此早于装备生效抵达客户端，切槽后可立即激活。
	// 被动部分留到装备真正生效时再授予，避免物品仅仅在槽位里就产生数值影响。
	const UEquipmentDefinition* EquipmentCDO = GetDefault<UEquipmentDefinition>(EquipInfo->EquipmentDefinition);
	const FGameplayTag          GateTag      = GetSlotGateTag(SlotIndex);

	for (const TObjectPtr<const UModularAbilitySet>& AbilitySet : EquipmentCDO->AbilitySetsToGrant)
	{
		if (AbilitySet != nullptr)
		{
			AbilitySet->GiveToAbilitySystem(AbilitySystem, &SlotGrantedHandles[SlotIndex], SlotItem, EModularAbilitySetGrantParts::AbilitiesOnly, GateTag);
		}
	}
}

void UQuickBarComponent::TakeBackSlotAbilities(int32 SlotIndex)
{
	if (!SlotGrantedHandles.IsValidIndex(SlotIndex))
	{
		return;
	}

	if (UModularAbilitySystemComponent* AbilitySystem = FindAbilitySystemComponent())
	{
		SlotGrantedHandles[SlotIndex].TakeFromAbilitySystem(AbilitySystem);
	}
}

void UQuickBarComponent::EnsureGateProviderRegistered()
{
	UModularAbilitySystemComponent* AbilitySystem = FindAbilitySystemComponent();
	if ((AbilitySystem == nullptr) || (RegisteredGateAbilitySystem == AbilitySystem))
	{
		return;
	}

	// PlayerState 改换后旧 ASC 上的登记已无意义，先摘除再登记到新的一个。
	if (UModularAbilitySystemComponent* Previous = RegisteredGateAbilitySystem.Get())
	{
		Previous->UnregisterAbilityGateProvider(this);
	}

	AbilitySystem->RegisterAbilityGateProvider(this);
	RegisteredGateAbilitySystem = AbilitySystem;
}

int32 UQuickBarComponent::ParseSlotIndexFromGateTag(const FGameplayTag& GateTag)
{
	if (!GateTag.IsValid())
	{
		return INDEX_NONE;
	}

	const FString TagString = GateTag.ToString();
	const FString Prefix    = FString::Printf(TEXT("%s."), QuickBarSlotGateTagPrefix);
	if (!TagString.StartsWith(Prefix, ESearchCase::CaseSensitive))
	{
		// 别的系统的门禁 Tag，交回给后续提供方解释。
		return INDEX_NONE;
	}

	const FString IndexPart = TagString.RightChop(Prefix.Len());
	return IndexPart.IsNumeric() ? FCString::Atoi(*IndexPart) : INDEX_NONE;
}

EModularAbilityGateResult UQuickBarComponent::QueryAbilityGate(FGameplayTag GateTag) const
{
	const int32 SlotIndex = ParseSlotIndexFromGateTag(GateTag);
	if (SlotIndex == INDEX_NONE)
	{
		return EModularAbilityGateResult::NotHandled;
	}

	// 直接读本地生效索引：服务器读到的是权威值，客户端读到的是含预测的本地值，
	// 两端因此天然各自成立，不存在需要同步的中间状态。
	return (SlotIndex == GetActiveSlotIndex()) ? EModularAbilityGateResult::Satisfied : EModularAbilityGateResult::Blocked;
}

bool UQuickBarComponent::CanActivateSlot(int32 SlotIndex) const
{
	// 空槽不可激活：切过去只会卸下当前装备而拿不到新的，等同于把玩家切成空手。
	return Slots.IsValidIndex(SlotIndex) && (Slots[SlotIndex] != nullptr);
}

void UQuickBarComponent::SetActiveSlotIndex(int32 NewIndex)
{
	if (!CanActivateSlot(NewIndex) || (GetActiveSlotIndex() == NewIndex))
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		// 权威端直接落位；装备实例在入槽时已创建，切换只需改索引。
		ApplyAuthoritativeSlot(NewIndex, /*RequestId=*/0);
		return;
	}

	// 序号循环于 1..255 并跳过 0，使回执能区分「本次预测的确认」与「更早请求的回执」。
	LastSentRequestId = (LastSentRequestId % 255) + 1;

	// 先本地生效再发请求：切槽表现与技能可用性因此不必等待往返。
	PredictedActiveSlotIndex = NewIndex;
	bHasPredictedActiveSlot  = true;
	ApplyActiveSlotChanged();

	ServerSetActiveSlotIndex(NewIndex, LastSentRequestId);
}

void UQuickBarComponent::ServerSetActiveSlotIndex_Implementation(int32 NewIndex, uint8 RequestId)
{
	// 与客户端预测同一份规则，再补一条权威侧才能查的：物品必须确实在该玩家背包中。
	const bool bItemInInventory = CanActivateSlot(NewIndex) && [this, NewIndex]
	{
		const UInventoryManagerComponent* Inventory = GetOwner()->FindComponentByClass<UInventoryManagerComponent>();
		return (Inventory != nullptr) && Inventory->GetAllItems().Contains(Slots[NewIndex]);
	}();

	// 无论接受还是拒绝都带着请求序号回执，客户端据此判断这是不是最新预测的结果。
	// 拒绝时回推的是未改动的权威索引，客户端因此可见地退回原槽位。
	ApplyAuthoritativeSlot(bItemInInventory ? NewIndex : ActiveSlotIndex, RequestId);
}

void UQuickBarComponent::ApplyAuthoritativeSlot(int32 NewIndex, uint8 RequestId)
{
	const bool bChanged = (ActiveSlotIndex != NewIndex);
	ActiveSlotIndex     = NewIndex;

	if (bChanged)
	{
		ApplyActiveSlotChanged();
	}

	// 本地控制时权威变更已就地生效，无需回执；远端拥有者则必须收到，
	// 因为权威值与其请求值相同时属性复制不会触发，且复制不携带请求序号。
	if (!IsOwnerLocallyControlled())
	{
		ClientSyncActiveSlotIndex(ActiveSlotIndex, RequestId);
	}
}

void UQuickBarComponent::ClientSyncActiveSlotIndex_Implementation(int32 AuthoritativeIndex, uint8 RequestId)
{
	ActiveSlotIndex = AuthoritativeIndex;

	// 更早请求的回执不清除预测：其后还有更新的请求在途，此时清除会让表现倒退一次再跳回。
	const bool bResolvesPrediction = (RequestId == 0) || (RequestId == LastSentRequestId);
	if (bResolvesPrediction)
	{
		bHasPredictedActiveSlot  = false;
		PredictedActiveSlotIndex = -1;
	}

	ApplyActiveSlotChanged();
}

bool UQuickBarComponent::IsOwnerLocallyControlled() const
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	return (OwnerController != nullptr) && OwnerController->IsLocalController();
}

void UQuickBarComponent::ApplyActiveSlotChanged()
{
	RefreshActiveSlotEquipment();

	FQuickBarActiveIndexChangedMessage Message;
	Message.Owner       = GetOwner();
	Message.ActiveIndex = GetActiveSlotIndex();

	// 将激活索引变化广播给监听系统，例如武器栏高亮界面。
	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(this);
	MessageSystem.BroadcastMessage(TAG_EM_QuickBar_Message_ActiveIndexChanged, Message);
}

UInventoryItemInstance* UQuickBarComponent::GetActiveSlotItem() const
{
	const int32 EffectiveIndex = GetActiveSlotIndex();
	return Slots.IsValidIndex(EffectiveIndex) ? Slots[EffectiveIndex] : nullptr;
}

FGameplayTag UQuickBarComponent::GetSlotInputTag(int32 SlotIndex)
{
	if (SlotIndex < 0)
	{
		return FGameplayTag();
	}

	// 未定义该槽位按键时返回空 Tag，故不报错。
	const FName SlotTagName(*FString::Printf(TEXT("%s.%d"), QuickBarSlotInputTagPrefix, SlotIndex));
	return FGameplayTag::RequestGameplayTag(SlotTagName, /*ErrorIfNotFound=*/false);
}

FGameplayTag UQuickBarComponent::GetSlotGateTag(int32 SlotIndex)
{
	if (SlotIndex < 0)
	{
		return FGameplayTag();
	}

	// 未注册该槽位门禁 Tag 时返回空 Tag，该槽位技能因此不设门禁，故不报错。
	const FName SlotTagName(*FString::Printf(TEXT("%s.%d"), QuickBarSlotGateTagPrefix, SlotIndex));
	return FGameplayTag::RequestGameplayTag(SlotTagName, /*ErrorIfNotFound=*/false);
}

int32 UQuickBarComponent::GetNextFreeItemSlot() const
{
	int32 SlotIndex = 0;
	// 依次寻找第一个空槽位。
	for (const TObjectPtr<UInventoryItemInstance>& ItemPtr : Slots)
	{
		if (ItemPtr == nullptr)
		{
			return SlotIndex;
		}
		++SlotIndex;
	}

	return INDEX_NONE;
}

void UQuickBarComponent::GatherItemSlotCategoryTags(const UInventoryItemInstance* Item, FGameplayTagContainer& OutTags)
{
	if (Item == nullptr)
	{
		return;
	}

	// 类别 Tag 配置在展示片段上，随物品定义读取，客户端与服务器读到的内容一致。
	if (const UInventoryFragment_ItemDisplay* DisplayFragment = Item->FindFragmentByClass<UInventoryFragment_ItemDisplay>())
	{
		OutTags.AppendTags(DisplayFragment->SlotCategoryTags);
	}
}

int32 UQuickBarComponent::GetNextFreeItemSlotForItem(const UInventoryItemInstance* Item) const
{
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		if (CanAddItemToSlot(SlotIndex, Item))
		{
			return SlotIndex;
		}
	}

	return INDEX_NONE;
}

bool UQuickBarComponent::CanAddItemToSlot(int32 SlotIndex, const UInventoryItemInstance* Item) const
{
	if (!Slots.IsValidIndex(SlotIndex) || (Item == nullptr) || (Slots[SlotIndex] != nullptr))
	{
		return false;
	}

	FGameplayTagContainer ItemCategoryTags;
	GatherItemSlotCategoryTags(Item, ItemCategoryTags);

	const bool bSlotHasRule = SlotRules.IsValidIndex(SlotIndex) && !SlotRules[SlotIndex].AcceptQuery.IsEmpty();
	if (!bSlotHasRule)
	{
		// 未配置类别的槽位只接收同样未配置类别的物品，避免有归属的物品挤占通用槽位。
		return ItemCategoryTags.IsEmpty();
	}

	return SlotRules[SlotIndex].AcceptQuery.Matches(ItemCategoryTags);
}

void UQuickBarComponent::AddItemToSlot(int32 SlotIndex, UInventoryItemInstance* Item)
{
	if (Slots.IsValidIndex(SlotIndex) && (Item != nullptr))
	{
		if (Slots[SlotIndex] == nullptr)
		{
			// 仅允许写入空槽位，写入后主动触发本地通知。
			Slots[SlotIndex] = Item;
			GrantSlotAbilities(SlotIndex);
			CreateSlotEquipment(SlotIndex);
			OnRep_Slots();
		}
	}
}

bool UQuickBarComponent::TryAddItem(UInventoryItemInstance* Item)
{
	const int32 SlotIndex = GetNextFreeItemSlotForItem(Item);
	if (SlotIndex == INDEX_NONE)
	{
		return false;
	}

	AddItemToSlot(SlotIndex, Item);

	return true;
}

UInventoryItemInstance* UQuickBarComponent::RemoveItemFromSlot(int32 SlotIndex)
{
	UInventoryItemInstance* Result = nullptr;

	if (ActiveSlotIndex == SlotIndex)
	{
		// 移除的是当前激活槽位：清空激活索引使该装备失效。
		// 走序号 0 的回执，使客户端即便正预测着本槽位也会被无条件拉回。
		ApplyAuthoritativeSlot(-1, /*RequestId=*/0);
	}

	if (Slots.IsValidIndex(SlotIndex))
	{
		Result = Slots[SlotIndex];

		if (Result != nullptr)
		{
			// 清空槽位并广播槽位变化。
			Slots[SlotIndex] = nullptr;
			TakeBackSlotAbilities(SlotIndex);
			DestroySlotEquipment(SlotIndex);
			OnRep_Slots();
		}
	}

	return Result;
}

void UQuickBarComponent::OnRep_Slots()
{
	// 与激活索引的复制先后无保证，故槽位到达时也补一次生效意图；该调用幂等。
	RefreshActiveSlotEquipment();

	FQuickBarSlotsChangedMessage Message;
	Message.Owner = GetOwner();
	Message.Slots = Slots;

	// 将最新槽位数组广播给监听系统，例如快捷栏界面。
	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(this);
	MessageSystem.BroadcastMessage(TAG_EM_QuickBar_Message_SlotsChanged, Message);
}

void UQuickBarComponent::OnRep_ActiveSlotIndex()
{
	// 预测的确认与纠正一律走 ClientSyncActiveSlotIndex，因为只有它携带请求序号。
	// 复制到达时若仍有预测在途，说明更新的请求尚未回执，此时不得让权威值覆盖本地表现。
	if (bHasPredictedActiveSlot)
	{
		return;
	}

	ApplyActiveSlotChanged();
}

