// 背包物品发放 PawnData Fragment 声明。

#pragma once

#include "DataAsset/PawnDataFragment.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"

#include "PawnDataFragment_AddInventoryItems.generated.h"

#define UE_API EQUIPMANAGER_API

class UInventoryItemDefinition;
class UInventoryItemInstance;

/**
 * 单条物品发放配置。
 *
 * 描述一件要写入背包的物品，以及它是否需要同时占用快捷栏槽位。
 */
USTRUCT(BlueprintType)
struct FPawnDataInventoryItemGrant
{
	GENERATED_BODY()

	/** 要发放的物品定义。 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory", Meta = (AssetBundles = "Client,Server"))
	TSoftClassPtr<UInventoryItemDefinition> ItemDefinition;

	/** 发放的堆叠数量。 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory", Meta = (ClampMin = 1))
	int32 StackCount = 1;

	/** 物品要占用的快捷栏槽位；保持 `-1` 表示只进背包，不进入快捷栏也不会被装备。 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory", Meta = (ClampMin = -1))
	int32 QuickBarSlot = INDEX_NONE;
};

/**
 * 记录本次发放的物品与占用槽位，供撤销时精确回收。
 */
UCLASS(MinimalAPI)
class UPawnDataFragmentState_AddInventoryItems : public UPawnDataFragmentState
{
	GENERATED_BODY()

public:
	// 使用弱引用，避免物品在别处被销毁后产生悬垂指针。
	UPROPERTY()
	TArray<TWeakObjectPtr<UInventoryItemInstance>> Items;

	UPROPERTY()
	TArray<int32> OccupiedSlots;
};

/**
 * 背包物品发放 Fragment。
 *
 * 对应 GameFeatureAction 的 Add Inventory Items，但作用域为单个玩家的角色配置。
 * 背包与快捷栏组件挂在 Controller 上，物品却应随角色更替，故取 Pawn 作用域并经
 * 上下文中的 Controller 写入：Pawn 作用域在 Pawn 就绪后才应用，天然满足装备链路
 * 对「Controller 已控制 Pawn」的要求，无需等待 Pawn 变更通知。
 */
UCLASS(MinimalAPI, Const, DisplayName = "Add Inventory Items")
class UPawnDataFragment_AddInventoryItems : public UPawnDataFragment
{
	GENERATED_BODY()

public:
	//~Begin UPawnDataFragment Interface
	virtual EPawnDataFragmentScope  GetScope() const override { return EPawnDataFragmentScope::Pawn; }
	virtual bool                    RequiresAuthority() const override { return true; }
	UE_API virtual UPawnDataFragmentState* Apply(const FPawnDataFragmentContext& Context) const override;
	UE_API virtual void                    Revoke(const FPawnDataFragmentContext& Context, UPawnDataFragmentState* State) const override;
#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsFragmentValid(FDataValidationContext& Context) const override;
#endif
	//~End UPawnDataFragment Interface

	/** 该角色的初始物品列表。 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory", Meta = (TitleProperty = "{ItemDefinition}"))
	TArray<FPawnDataInventoryItemGrant> GrantedItems;

	/** 发放完成后是否自动激活序号最小的已占用槽位，使对应物品立即装备到 Pawn 上。 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	bool bActivateFirstOccupiedSlot = true;
};

#undef UE_API
