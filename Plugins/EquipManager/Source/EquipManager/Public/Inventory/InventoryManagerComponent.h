// 背包管理组件声明。

#pragma once

#include "CoreMinimal.h"
#include "Components/ControllerComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "InventoryManagerComponent.generated.h"

#define UE_API EQUIPMANAGER_API

class UInventoryItemDefinition;
class UInventoryItemInstance;
class UInventoryManagerComponent;
class UObject;
struct FFrame;
struct FInventoryList;
struct FNetDeltaSerializeInfo;
struct FReplicationFlags;

/**
 * 背包数量变化消息。
 *
 * 既用于新增、移除，也用于堆叠数量发生变化时向外部系统广播通知。
 */
USTRUCT(BlueprintType)
struct FInventoryChangeMessage
{
	GENERATED_BODY()

	// 后续可改为基于标签和拥有者的抽象标识，而不是直接暴露组件引用。
	UPROPERTY(BlueprintReadOnly, Category=Inventory)
	TObjectPtr<UActorComponent> InventoryOwner = nullptr;

	/** 本次发生变化的物品实例。 */
	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	TObjectPtr<UInventoryItemInstance> Instance = nullptr;

	/** 变化完成后该物品实例在背包中的最新数量。 */
	UPROPERTY(BlueprintReadOnly, Category=Inventory)
	int32 NewCount = 0;

	/** 相比变化前的数量增量，可为负数。 */
	UPROPERTY(BlueprintReadOnly, Category=Inventory)
	int32 Delta = 0;
};

/**
 * 背包中的单条物品记录。
 *
 * 记录物品实例本身以及该实例对应的堆叠数量。
 */
USTRUCT(BlueprintType)
struct FInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FInventoryEntry()
	{}

	/** 返回便于调试输出的字符串描述。 */
	FString GetDebugString() const;

private:
	friend FInventoryList;
	friend UInventoryManagerComponent;

	// 当前记录对应的物品实例。
	UPROPERTY()
	TObjectPtr<UInventoryItemInstance> Instance = nullptr;

	// 当前物品实例的堆叠数量。
	UPROPERTY()
	int32 StackCount = 0;

	// 客户端上一次观察到的数量，用于计算变化增量。
	UPROPERTY(NotReplicated)
	int32 LastObservedCount = INDEX_NONE;
};

/**
 * 背包物品快速数组。
 *
 * 负责库存条目的增量复制、增删改操作以及变化消息广播。
 */
USTRUCT(BlueprintType)
struct FInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	FInventoryList()
		: OwnerComponent(nullptr)
	{}

	FInventoryList(UActorComponent* InOwnerComponent)
		: OwnerComponent(InOwnerComponent)
	{}

	/** 返回背包中当前全部有效的物品实例。 */
	TArray<UInventoryItemInstance*> GetAllItems() const;

public:
	//~Begin FFastArraySerializer Contract
	// 元素从复制数组中移除前触发，用于广播数量归零消息。
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	// 元素首次复制到客户端后触发，用于广播新增消息。
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	// 已有元素复制发生变化后触发，用于广播数量变化消息。
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	//~End FFastArraySerializer Contract

	/** 执行快速数组的网络增量序列化。 */
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FInventoryEntry, FInventoryList>(Entries, DeltaParms, *this);
	}

	/** 根据物品定义创建新的物品实例并加入背包。 */
	UInventoryItemInstance* AddEntry(TSubclassOf<UInventoryItemDefinition> ItemClass, int32 StackCount);
	/** 将已有物品实例加入背包。 */
	void                    AddEntry(UInventoryItemInstance* Instance);

	/** 按实例从背包中移除物品记录。 */
	void RemoveEntry(UInventoryItemInstance* Instance);

private:
	// 向消息系统广播某个条目的数量变化。
	void BroadcastChangeMessage(FInventoryEntry& Entry, int32 OldCount, int32 NewCount);

private:
	friend UInventoryManagerComponent;

private:
	// 参与复制的背包条目数组。
	UPROPERTY()
	TArray<FInventoryEntry> Entries;

	// 列表所属组件，不参与网络复制。
	UPROPERTY(NotReplicated)
	TObjectPtr<UActorComponent> OwnerComponent;
};

template <>
struct TStructOpsTypeTraits<FInventoryList> : public TStructOpsTypeTraitsBase2<FInventoryList>
{
	enum { WithNetDeltaSerializer = true };
};


/**
 * 背包管理组件。
 *
 * 提供物品的添加、移除、查询、消耗与网络复制能力，可视为控制器上的背包容器。
 */
UCLASS(MinimalAPI, BlueprintType)
class UInventoryManagerComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	/** 构造背包管理组件并启用复制。 */
	UE_API UInventoryManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 判断指定物品定义及数量当前是否允许加入背包。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	UE_API bool CanAddItemDefinition(TSubclassOf<UInventoryItemDefinition> ItemDef, int32 StackCount = 1);

	/** 根据物品定义创建实例并加入背包。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	UE_API UInventoryItemInstance* AddItemDefinition(TSubclassOf<UInventoryItemDefinition> ItemDef, int32 StackCount = 1);

	/** 将已有物品实例加入背包。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	UE_API void AddItemInstance(UInventoryItemInstance* ItemInstance);

	/** 从背包中移除指定物品实例。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	UE_API void RemoveItemInstance(UInventoryItemInstance* ItemInstance);

	/** 返回当前背包中的全部有效物品实例。 */
	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure=false)
	UE_API TArray<UInventoryItemInstance*> GetAllItems() const;

	/** 返回第一个匹配指定物品定义的实例。 */
	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure)
	UE_API UInventoryItemInstance* FindFirstItemStackByDefinition(TSubclassOf<UInventoryItemDefinition> ItemDef) const;

	/** 统计背包中匹配指定物品定义的总数量。 */
	UE_API int32 GetTotalItemCountByDefinition(TSubclassOf<UInventoryItemDefinition> ItemDef) const;
	/** 尝试按物品定义消耗指定数量的物品，成功时返回真。 */
	UE_API bool  ConsumeItemsByDefinition(TSubclassOf<UInventoryItemDefinition> ItemDef, int32 NumToConsume);

	//~Begin UObject Interface
	// 手动复制背包中的物品子对象。
	UE_API virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	// 复制系统准备就绪后注册已有物品实例。
	UE_API virtual void ReadyForReplication() override;
	//~End UObject Interface

private:
	// 当前组件维护的背包条目列表，会参与网络复制。
	UPROPERTY(Replicated)
	FInventoryList InventoryList;
};

#undef UE_API
