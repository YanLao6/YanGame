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
class USkeletalMeshComponent;
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

	FEquipmentList(UEquipmentManagerComponent* InOwnerComponent)
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

	/** 创建并登记一个新的装备实例；本身不授予任何内容，被动留待实例生效时再授予。 */
	UEquipmentInstance* AddEntry(TSubclassOf<UEquipmentDefinition> EquipmentDefinition);
	/** 按实例移除一条已装备记录。 */
	void RemoveEntry(UEquipmentInstance* Instance);
	/** 向指定实例授予其装备定义中的被动部分，记入该条目的回收句柄。 */
	void GrantEntryPassives(UEquipmentInstance* Instance);
	/** 回收指定实例已授予的被动部分。 */
	void TakeBackEntryPassives(UEquipmentInstance* Instance);

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
	TObjectPtr<UEquipmentManagerComponent> OwnerComponent;
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

	/**
	 * 根据装备定义创建装备实例并生成其附属 Actor，仅服务器可调用。
	 *
	 * 实例创建即持有，但尚未生效：附属 Actor 处于隐藏状态、被动尚未授予、OnEquipped 尚未触发。
	 * 须再调用 ActivateInstance 才会真正生效。技能不在此授予，由持有方（快捷栏）在物品入槽时预授予。
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	UE_API UEquipmentInstance* EquipItem(TSubclassOf<UEquipmentDefinition> EquipmentDefinition);

	/** 销毁指定装备实例及其附属 Actor，仍生效时会先失效，仅服务器可调用。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	UE_API void UnequipItem(UEquipmentInstance* ItemInstance);

	/**
	 * 指定哪个来源物品对应的装备实例应当生效，传入 nullptr 表示全部失效。
	 *
	 * 以来源物品而非实例指针表达意图：来源物品随快捷栏槽位复制，两端都能表达同一个意图，
	 * 而实例本身可能尚未复制到达。意图会被记住，实例到达后自动解析生效。
	 * 服务器与拥有者客户端依本地意图推导，故切换无需等待确认；其余客户端看不到快捷栏，
	 * 改由权威端复制来的生效实例驱动。
	 */
	UE_API void SetActiveInstigator(UObject* Instigator);

	/** 依当前生效意图重新解析应生效的实例；实例复制到达后由列表回调触发。 */
	UE_API void ResolveActiveInstance();

	/** 返回当前生效的装备实例；无实例生效时返回空。 */
	UFUNCTION(BlueprintPure, Category=Equipment)
	UE_API UEquipmentInstance* GetActiveInstance() const { return ActiveInstance; }

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

	/**
	 * 返回当前生效的装备实例，类型不匹配或无实例生效时返回空。
	 *
	 * 只考察生效中的实例：未生效的装备虽然同样被持有（附属 Actor 已生成但隐藏），
	 * 但对「角色现在拿着什么」这一问题不成立，故不参与匹配。
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	UE_API UEquipmentInstance* GetFirstInstanceOfType(TSubclassOf<UEquipmentInstance> InstanceType);

	/** 返回生效中且类型匹配的装备实例，语义同 GetFirstInstanceOfType，无匹配时返回空数组。 */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	UE_API TArray<UEquipmentInstance*> GetEquipmentInstancesOfType(TSubclassOf<UEquipmentInstance> InstanceType) const;

	/** 模板便捷接口，用于按具体实例类型获取第一个已装备对象。 */
	template <typename T>
	T* GetFirstInstanceOfType()
	{
		return (T*)GetFirstInstanceOfType(T::StaticClass());
	}

private:
	// 切换生效实例：使旧实例失效、新实例生效，被动的授予与回收仅在服务器进行。
	void ApplyActiveInstance(UEquipmentInstance* ItemInstance);

	/**
	 * 主网格的动画实例重建后重放生效装备的外观。
	 *
	 * 换体型、换外观部件都会重建动画实例，其上已链接的动画层随之丢弃；
	 * 装备时链接过一次并不能保证之后仍然成立，故须在每次重建后补一次。
	 */
	UFUNCTION()
	void OnOwnerMeshAnimInitialized();

	// 监听主网格的动画初始化通知，重复调用安全。
	void BindOwnerMeshAnimInitialized();
	// 解除对主网格动画初始化通知的监听。
	void UnbindOwnerMeshAnimInitialized();

	// 本端能否自行推导生效意图：意图存放在控制器的快捷栏上，只有服务器与拥有者客户端看得到它。
	bool HasLocalEquipIntent() const;

	// 权威端的生效结果到达时，在其余客户端上应用同一结论。
	UFUNCTION()
	void OnRep_ReplicatedActiveInstance();

private:
	// 当前组件维护的已装备列表，会参与网络复制。
	UPROPERTY(Replicated)
	FEquipmentList EquipmentList;

	/**
	 * 权威端解析出的生效实例，复制给全部客户端。
	 *
	 * 生效意图源自控制器上的快捷栏，而控制器只复制给拥有者，其余客户端无从推导；
	 * 它们据本字段得知角色手上拿的是哪一件，武器显隐与动画层因此在三端一致。
	 */
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedActiveInstance)
	TObjectPtr<UEquipmentInstance> ReplicatedActiveInstance;

	// 本端已实际应用的生效实例；不复制。与 ReplicatedActiveInstance 分开保存，
	// 使 OnRep 到达时仍能与「已应用」状态比较——否则复制赋值会先行吞掉这次切换。
	UPROPERTY(Transient)
	TObjectPtr<UEquipmentInstance> ActiveInstance;

	// 期望生效的来源物品；不复制。实例可能晚于本意图到达，故须记住以便到达后解析。
	UPROPERTY(Transient)
	TObjectPtr<UObject> ActiveInstigator;

	// 已监听动画初始化通知的主网格，解绑时据其取回委托。
	TWeakObjectPtr<USkeletalMeshComponent> BoundMesh;
};

#undef UE_API
