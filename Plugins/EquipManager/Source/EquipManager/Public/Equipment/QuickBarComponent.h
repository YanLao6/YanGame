// 快捷栏组件声明。

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "QuickBarComponent.generated.h"

#define UE_API EQUIPMANAGER_API

class UEquipmentManagerComponent;
class UEquipmentInstance;
class UInventoryItemInstance;
class FLifetimeProperty;

/**
 * 单个快捷栏槽位的准入规则。
 *
 * 规则数组按索引对应槽位；未配置到的槽位与查询为空的槽位均不作限制。
 */
USTRUCT(BlueprintType)
struct FQuickBarSlotRule
{
	GENERATED_BODY()

	/** 该槽位接受的物品类别查询；留空表示接受任意物品。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=QuickBar)
	FGameplayTagQuery AcceptQuery;
};

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

	/**
	 * 返回激活该槽位的输入 Tag，按 `InputTag.QuickBar.Slot.<Index>` 的命名约定由索引拼出。
	 *
	 * 约定而非配置：切槽技能在 AbilitySet 中已按同名 Tag 绑定输入，此处不再重复声明映射。
	 * 索引非法或该 Tag 未注册（如槽位数多于已定义的按键）时返回空 Tag，调用方据此隐藏按键提示。
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false)
	static UE_API FGameplayTag GetSlotInputTag(int32 SlotIndex);

	/** 返回当前激活槽位中的物品实例。 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false)
	UE_API UInventoryItemInstance* GetActiveSlotItem() const;

	/** 返回第一个空闲槽位索引，若没有空位则返回 `INDEX_NONE`。 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false)
	UE_API int32 GetNextFreeItemSlot() const;

	/** 返回第一个既空闲又接受该物品的槽位索引，若不存在则返回 `INDEX_NONE`。 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false)
	UE_API int32 GetNextFreeItemSlotForItem(const UInventoryItemInstance* Item) const;

	/**
	 * 判断物品当前能否放入指定槽位。
	 *
	 * 纯查询且不依赖服务器权威，客户端可直接调用以预判拖拽结果。
	 * 判定包含索引合法、槽位空闲与类别匹配三项；物品与槽位的类别必须同时存在或同时缺省。
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false)
	UE_API bool CanAddItemToSlot(int32 SlotIndex, const UInventoryItemInstance* Item) const;

	/** 将物品放入指定槽位，仅服务器可调用。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	UE_API void AddItemToSlot(int32 SlotIndex, UInventoryItemInstance* Item);

	/**
	 * 从头查找第一个接受该物品的空闲槽位并放入，仅服务器可调用，返回是否写入成功。
	 *
	 * 未配置类别的物品只会落入同样未配置类别的槽位。
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	UE_API bool TryAddItem(UInventoryItemInstance* Item);

	/** 从指定槽位移除物品，仅服务器可调用，并返回被移除的物品。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	UE_API UInventoryItemInstance* RemoveItemFromSlot(int32 SlotIndex);

	//~Begin UActorComponent Interface
	// 组件开始运行时补齐槽位数组长度。
	UE_API virtual void BeginPlay() override;
	//~End UActorComponent Interface

private:
	// 汇总物品定义上各片段贡献的类别 Tag。
	static void GatherItemSlotCategoryTags(const UInventoryItemInstance* Item, FGameplayTagContainer& OutTags);

	// 卸下当前槽位对应的装备实例。
	void UnequipItemInSlot();
	// 根据当前激活槽位尝试装备对应物品。
	void EquipItemInSlot();

	// 在拥有者控制的 Pawn 上查找装备管理组件。
	UEquipmentManagerComponent* FindEquipmentManager() const;

protected:
	/** 快捷栏默认可容纳的槽位数量。 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	int32 NumSlots = 4;

	/** 各槽位的准入规则，按索引对应槽位；长度可短于槽位数，未覆盖的槽位不作限制。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=QuickBar)
	TArray<FQuickBarSlotRule> SlotRules;

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
