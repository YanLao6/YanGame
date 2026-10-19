// 局内开局配装定义声明。

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"
#include "InGameLoadoutDefinition.generated.h"

#define UE_API EQUIPMANAGER_API

class UInventoryItemDefinition;

/** 开局发放的一件物品。 */
USTRUCT(BlueprintType)
struct FInGameLoadoutGrant
{
	GENERATED_BODY()

	/** 发放的物品定义。 */
	UPROPERTY(EditDefaultsOnly, Category=Loadout)
	TSoftClassPtr<UInventoryItemDefinition> ItemDefinition;

	/** 堆叠数量。 */
	UPROPERTY(EditDefaultsOnly, Category=Loadout, meta=(ClampMin=1))
	int32 StackCount = 1;

	/** 目标快捷栏槽位；负数表示只进背包不入槽。 */
	UPROPERTY(EditDefaultsOnly, Category=Loadout)
	int32 QuickBarSlot = INDEX_NONE;
};

/** 玩家配装能覆盖哪些槽位。 */
UENUM(BlueprintType)
enum class ELoadoutOverridePolicy : uint8
{
	/** 玩家配装不参与，开局内容完全由本资产决定。 */
	None,
	/** 只有列出的槽位可被玩家覆盖。 */
	ListedSlots,
	/** 全部槽位都可被玩家覆盖。 */
	AllSlots,
};

/**
 * 局内开局配装。
 *
 * 表达「这一局至少有什么」，是开局内容的兜底；玩家从仓库带来的配装在本资产允许的范围内覆盖它。
 * 玩家配装为空、或其中的条目通不过准入时，兜底内容原样生效。
 */
UCLASS(MinimalAPI, BlueprintType, Const)
class UInGameLoadoutDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 判断某个槽位是否允许被玩家配装覆盖。 */
	UE_API bool IsSlotOverridable(int32 SlotIndex) const;

	/** 判断某个物品标识是否在准入名单内；名单为空表示不限制。 */
	UE_API bool IsItemAllowed(FName ItemId) const;

	/** 兜底发放内容。 */
	UPROPERTY(EditDefaultsOnly, Category=Loadout)
	TArray<FInGameLoadoutGrant> BaseGrants;

	/** 发放完成后是否激活首个被占用的槽位。 */
	UPROPERTY(EditDefaultsOnly, Category=Loadout)
	bool bActivateFirstOccupiedSlot = true;

	/** 玩家配装的覆盖范围。 */
	UPROPERTY(EditDefaultsOnly, Category=Override)
	ELoadoutOverridePolicy OverridePolicy = ELoadoutOverridePolicy::None;

	/** 允许被覆盖的槽位，仅在覆盖范围为 ListedSlots 时生效。 */
	UPROPERTY(EditDefaultsOnly, Category=Override, meta=(EditCondition="OverridePolicy == ELoadoutOverridePolicy::ListedSlots", EditConditionHides))
	TArray<int32> OverridableSlots;

	/**
	 * 玩家可带入的物品标识名单；留空表示不限制。
	 *
	 * 这是服务器侧的硬约束：玩家改写本地存档也带不进名单之外的物品。
	 */
	UPROPERTY(EditDefaultsOnly, Category=Override, meta=(EditCondition="OverridePolicy != ELoadoutOverridePolicy::None", EditConditionHides))
	TArray<FName> AllowedItemIds;
};

#undef UE_API
