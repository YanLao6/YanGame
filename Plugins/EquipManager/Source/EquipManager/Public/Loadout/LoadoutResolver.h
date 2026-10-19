// 配装决议声明。

#pragma once

#include "CoreMinimal.h"
#include "Stash/StashTypes.h"
#include "Templates/SubclassOf.h"
#include "LoadoutResolver.generated.h"

class UInGameLoadoutDefinition;
class UInventoryItemCatalog;
class UInventoryItemDefinition;

/** 决议结果中的一条发放。 */
USTRUCT()
struct FResolvedLoadoutEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TSoftClassPtr<UInventoryItemDefinition> ItemDefinition;

	UPROPERTY()
	int32 StackCount = 1;

	UPROPERTY()
	int32 QuickBarSlot = INDEX_NONE;
};

/** 本局最终要发放的内容。 */
USTRUCT()
struct FResolvedLoadout
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FResolvedLoadoutEntry> Entries;

	/**
	 * 发放完成后应当激活的槽位，INDEX_NONE 表示不激活。
	 *
	 * 由声明要激活的那一层自己占用的首个槽位决定，而不是所有层合起来的最小槽位——
	 * 英雄专属配装往往占用更靠前的槽位，按全局最小算会把激活权从模式手里抢走。
	 */
	UPROPERTY()
	int32 SlotToActivate = INDEX_NONE;
};

/**
 * 配装决议。
 *
 * 以模式配置为底，用玩家请求在允许范围内覆盖，得出本局实际发放的内容。
 * 无状态且不接触任何组件：输入输出都是结构体，可离线驱动以验证准入规则。
 */
namespace LoadoutResolver
{
	/**
	 * 决议本局的发放内容。
	 *
	 * 配装按传入顺序叠加：同一槽位由后者接管，不同槽位并存，只进背包的条目一律保留。
	 * 这样模式给全员的那份与英雄专属的那份可以分开配置，互不知情。
	 *
	 * 通不过准入的请求条目被丢弃并记下日志，对应槽位沿用配装本身的内容；
	 * 丢弃从不静默，否则玩家只会看到「我的配装没生效」而无从查起。
	 * 须在服务器侧调用：请求来自客户端，准入判断不能交给请求方自己做。
	 */
	EQUIPMANAGER_API FResolvedLoadout Resolve(const TArray<const UInGameLoadoutDefinition*>& Definitions, const FLoadoutRequest& Request, const UInventoryItemCatalog* Catalog);
}
