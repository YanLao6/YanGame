// 物品实例声明。

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagStack.h"
#include "Templates/SubclassOf.h"
#include "InventoryItemInstance.generated.h"

#define UE_API EQUIPMANAGER_API

class UInventoryItemFragment;
class UInventoryItemDefinition;
struct FGameplayTagStackContainer;

/**
 * 物品运行时实例。
 *
 * 保存物品定义引用、GameplayTag 堆叠状态和运行时片段查询能力，并支持网络复制。
 */
UCLASS(MinimalAPI)
class UInventoryItemInstance : public UObject
{
	GENERATED_BODY()

public:
	/** 构造物品实例对象。 */
	UE_API UInventoryItemInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UObject Interface
	// 声明该对象支持网络复制。
	UE_API virtual bool IsSupportedForNetworking() const override { return true; }
	//~End UObject Interface

	/** 为指定 GameplayTag 增加层数；当数量小于 1 时不会产生有效变化。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	UE_API void AddStatTagStack(FGameplayTag Tag, int32 StackCount);

	/** 为指定 GameplayTag 移除层数；当数量小于 1 时不会产生有效变化。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category= Inventory)
	UE_API void RemoveStatTagStack(FGameplayTag Tag, int32 StackCount);

	/** 返回指定 GameplayTag 当前的堆叠层数；若不存在则返回 0。 */
	UFUNCTION(BlueprintCallable, Category=Inventory)
	UE_API int32 GetStatTagStackCount(FGameplayTag Tag) const;

	/** 判断指定 GameplayTag 是否至少拥有一层堆叠。 */
	UFUNCTION(BlueprintCallable, Category=Inventory)
	UE_API bool HasStatTag(FGameplayTag Tag) const;

	/** 返回该实例关联的物品定义类型。 */
	UE_API TSubclassOf<UInventoryItemDefinition> GetItemDef() const
	{
		return ItemDef;
	}

	/** 在当前物品定义上查找第一个匹配类型的片段。 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false, meta=(DeterminesOutputType=FragmentClass))
	UE_API const UInventoryItemFragment* FindFragmentByClass(TSubclassOf<UInventoryItemFragment> FragmentClass) const;

	/** 模板便捷接口，用于按具体片段类型查询物品片段。 */
	template <typename ResultClass>
	const ResultClass* FindFragmentByClass() const
	{
		return (ResultClass*)FindFragmentByClass(ResultClass::StaticClass());
	}

private:
	//~Begin UObject Interface
	// 注册 Iris 复制所需的属性片段。
	UE_API virtual void RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context, UE::Net::EFragmentRegistrationFlags RegistrationFlags) override;
	//~End UObject Interface

	// 设置当前实例关联的物品定义，仅供库存系统内部写入。
	void SetItemDef(TSubclassOf<UInventoryItemDefinition> InDef);

	friend struct FInventoryList;

private:
	// 记录物品实例上的标签堆叠状态。
	UPROPERTY(Replicated)
	FGameplayTagStackContainer StatTags;

	// 当前实例所对应的静态物品定义。
	UPROPERTY(Replicated)
	TSubclassOf<UInventoryItemDefinition> ItemDef;
};

#undef UE_API
