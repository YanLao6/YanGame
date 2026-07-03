// 快捷栏组件声明。

#pragma once

#include "CoreMinimal.h"
#include "QuickBarComponent.generated.h"

#define UE_API EQUIPMANAGER_API

class UEquipmentManagerComponent;
class UEquipmentInstance;
class UInventoryItemInstance;
class FLifetimeProperty;

/**
 * 快捷栏组件。
 *
 * 负责维护固定槽位中的物品实例，并根据激活槽位自动向装备管理组件申请装备或卸下物品。
 */
UCLASS(Blueprintable, meta=(BlueprintSpawnableComponent))
class UQuickBarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 构造快捷栏组件并启用复制。 */
	UE_API UQuickBarComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UObject Interface
	// 注册快捷栏槽位与当前激活索引的复制属性。
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End UObject Interface

	/** 向前循环切换到下一个非空槽位。 */
	UFUNCTION(BlueprintCallable, Category="")
	UE_API void CycleActiveSlotForward();

	/** 向后循环切换到上一个非空槽位。 */
	UFUNCTION(BlueprintCallable, Category="")
	UE_API void CycleActiveSlotBackward();

	/** 设置当前激活的槽位索引，并处理旧装备卸下与新装备装备。 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category="")
	UE_API void SetActiveSlotIndex(int32 NewIndex);

	/** 返回全部快捷栏槽位中的物品实例。 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false)
	UE_API TArray<UInventoryItemInstance*> GetSlots() const
	{
		return Slots;
	}

	/** 返回当前激活的槽位索引，未激活时通常为 `-1`。 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false)
	UE_API int32 GetActiveSlotIndex() const { return ActiveSlotIndex; }

	/** 返回当前激活槽位中的物品实例。 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false)
	UE_API UInventoryItemInstance* GetActiveSlotItem() const;

	/** 返回第一个空闲槽位索引，若没有空位则返回 `INDEX_NONE`。 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false)
	UE_API int32 GetNextFreeItemSlot() const;

	/** 将物品放入指定槽位，仅服务器可调用。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	UE_API void AddItemToSlot(int32 SlotIndex, UInventoryItemInstance* Item);

	/** 从指定槽位移除物品，仅服务器可调用，并返回被移除的物品。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	UE_API UInventoryItemInstance* RemoveItemFromSlot(int32 SlotIndex);

	//~Begin UActorComponent Interface
	// 组件开始运行时补齐槽位数组长度。
	UE_API virtual void BeginPlay() override;
	//~End UActorComponent Interface

private:
	// 卸下当前槽位对应的装备实例。
	void UnequipItemInSlot();
	// 根据当前激活槽位尝试装备对应物品。
	void EquipItemInSlot();

	// 在拥有者控制的 Pawn 上查找装备管理组件。
	UEquipmentManagerComponent* FindEquipmentManager() const;

protected:
	/** 快捷栏默认可容纳的槽位数量。 */
	UPROPERTY()
	int32 NumSlots = 3;

	// 槽位数组复制更新后广播消息。
	UFUNCTION()
	void OnRep_Slots();

	// 激活槽位索引复制更新后广播消息。
	UFUNCTION()
	void OnRep_ActiveSlotIndex();

private:
	// 快捷栏中每个槽位存放的物品实例。
	UPROPERTY(ReplicatedUsing=OnRep_Slots)
	TArray<TObjectPtr<UInventoryItemInstance>> Slots;

	// 当前处于激活状态的槽位索引。
	UPROPERTY(ReplicatedUsing=OnRep_ActiveSlotIndex)
	int32 ActiveSlotIndex = -1;

	// 当前由快捷栏驱动装备出来的装备实例。
	UPROPERTY()
	TObjectPtr<UEquipmentInstance> EquippedItem;
};


/** 快捷栏槽位内容变化时广播的消息。 */
USTRUCT(BlueprintType)
struct FQuickBarSlotsChangedMessage
{
	GENERATED_BODY()

	/** 触发本次快捷栏变化的拥有者。 */
	UPROPERTY(BlueprintReadOnly, Category=Inventory)
	TObjectPtr<AActor> Owner = nullptr;

	/** 当前最新的全部槽位内容。 */
	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	TArray<TObjectPtr<UInventoryItemInstance>> Slots;
};


/** 快捷栏激活索引变化时广播的消息。 */
USTRUCT(BlueprintType)
struct FQuickBarActiveIndexChangedMessage
{
	GENERATED_BODY()

	/** 触发本次激活索引变化的拥有者。 */
	UPROPERTY(BlueprintReadOnly, Category=Inventory)
	TObjectPtr<AActor> Owner = nullptr;

	/** 当前最新的激活槽位索引。 */
	UPROPERTY(BlueprintReadOnly, Category=Inventory)
	int32 ActiveIndex = 0;
};

#undef UE_API
