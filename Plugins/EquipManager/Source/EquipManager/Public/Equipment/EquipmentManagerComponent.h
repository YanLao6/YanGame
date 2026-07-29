// 装备管理组件声明。

#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "GameplayAbilities/ModularAbilitySet.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "EquipmentManagerComponent.generated.h"

#define UE_API EQUIPMANAGER_API

class UAbilitySystemComponent;
class UEquipmentManagerComponent;
struct FEquipmentList;
class UEquipmentDefinition;
class UEquipmentInstance;

/**
 * 单条已装备记录。
 *
 * 用于在快速数组复制中同步装备定义与装备实例之间的对应关系。
 */
USTRUCT(BlueprintType)
struct FAppliedEquipmentEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FAppliedEquipmentEntry()
	{}

	/** 返回便于调试输出的字符串描述。 */
	FString GetDebugString() const;

private:
	friend FEquipmentList;
	friend UEquipmentManagerComponent;

	// 当前记录对应的装备定义类型。
	UPROPERTY()
	TSubclassOf<UEquipmentDefinition> EquipmentDefinition;

	// 当前记录在运行时创建出的装备实例。
	UPROPERTY()
	TObjectPtr<UEquipmentInstance> Instance = nullptr;

	// 仅服务器持有的能力授予句柄列表，当前实现暂未启用。
	UPROPERTY(NotReplicated)
	FModularAbilitySet_GrantedHandles GrantedHandles;
};

/**
 * 已装备项目列表。
 *
 * 通过 `FFastArraySerializer` 负责装备实例的增量复制，以及客户端侧的生命周期回调。
 */
USTRUCT(BlueprintType)
struct FEquipmentList : public FFastArraySerializer
{
	GENERATED_BODY()

	FEquipmentList()
		: OwnerComponent(nullptr)
	{}

	FEquipmentList(UActorComponent* InOwnerComponent)
		: OwnerComponent(InOwnerComponent)
	{}

public:
	//~Begin FFastArraySerializer Contract
	// 元素从复制数组中移除前触发，用于执行卸下回调。
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	// 元素首次复制到客户端后触发，用于执行装备完成回调。
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	// 元素复制内容变化后触发，当前未使用。
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	//~End FFastArraySerializer Contract

	/** 执行快速数组的网络增量序列化。 */
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FAppliedEquipmentEntry, FEquipmentList>(Entries, DeltaParms, *this);
	}

	/** 创建并登记一个新的装备实例。 */
	UEquipmentInstance* AddEntry(TSubclassOf<UEquipmentDefinition> EquipmentDefinition);
	/** 按实例移除一条已装备记录。 */
	void RemoveEntry(UEquipmentInstance* Instance);

private:
	// 获取拥有者上的能力系统组件，供后续装备能力扩展使用。
	UAbilitySystemComponent* GetAbilitySystemComponent() const;

	friend UEquipmentManagerComponent;

private:
	// 参与复制的已装备记录数组。
	UPROPERTY()
	TArray<FAppliedEquipmentEntry> Entries;

	// 列表所属组件，不参与网络复制。
	UPROPERTY(NotReplicated)
	TObjectPtr<UActorComponent> OwnerComponent;
};

template <>
struct TStructOpsTypeTraits<FEquipmentList> : public TStructOpsTypeTraitsBase2<FEquipmentList>
{
	enum { WithNetDeltaSerializer = true };
};


/**
 * 装备管理组件。
 *
 * 挂在 Pawn 上，负责创建、移除和复制角色当前持有的装备实例。
 */
UCLASS(MinimalAPI, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UEquipmentManagerComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	/** 构造装备管理组件并启用复制。 */
	UE_API UEquipmentManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 根据装备定义创建并装备一个新的装备实例，仅服务器可调用。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	UE_API UEquipmentInstance* EquipItem(TSubclassOf<UEquipmentDefinition> EquipmentDefinition);

	/** 卸下并移除指定装备实例，仅服务器可调用。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	UE_API void UnequipItem(UEquipmentInstance* ItemInstance);

	//~Begin UObject Interface
	// 手动复制由本组件持有的装备子对象。
	UE_API virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	//~End UObject Interface

	//~Begin UActorComponent Interface
	//virtual void EndPlay() override;
	// 初始化组件运行时状态。
	UE_API virtual void InitializeComponent() override;
	// 组件卸载时清理全部已装备实例。
	UE_API virtual void UninitializeComponent() override;
	// 复制系统准备就绪后注册已有装备实例。
	UE_API virtual void ReadyForReplication() override;
	//~End UActorComponent Interface

	/** 返回第一个匹配指定类型的装备实例，若不存在则返回空。 */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	UE_API UEquipmentInstance* GetFirstInstanceOfType(TSubclassOf<UEquipmentInstance> InstanceType);

	/** 返回所有匹配指定类型的装备实例，若不存在则返回空数组。 */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	UE_API TArray<UEquipmentInstance*> GetEquipmentInstancesOfType(TSubclassOf<UEquipmentInstance> InstanceType) const;

	/** 模板便捷接口，用于按具体实例类型获取第一个已装备对象。 */
	template <typename T>
	T* GetFirstInstanceOfType()
	{
		return (T*)GetFirstInstanceOfType(T::StaticClass());
	}

private:
	// 当前组件维护的已装备列表，会参与网络复制。
	UPROPERTY(Replicated)
	FEquipmentList EquipmentList;
};

#undef UE_API
