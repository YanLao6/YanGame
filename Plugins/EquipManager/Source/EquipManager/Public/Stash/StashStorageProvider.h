// 仓库存储提供者声明。

#pragma once

#include "CoreMinimal.h"
#include "Stash/StashTypes.h"
#include "UObject/Object.h"
#include "StashStorageProvider.generated.h"

#define UE_API EQUIPMANAGER_API

class ULocalPlayer;

/** 读取完成回调，失败时快照为默认值。 */
DECLARE_DELEGATE_TwoParams(FStashLoadComplete, bool /*bSuccess*/, const FStashSnapshot& /*Snapshot*/);
/** 写入完成回调。 */
DECLARE_DELEGATE_OneParam(FStashSaveComplete, bool /*bSuccess*/);

/**
 * 仓库存储提供者。
 *
 * 把「仓库数据存在哪」与「仓库数据是什么」隔开：仓库层只经本接口读写整份快照，
 * 换成服务端存储时上层无须改动。读写一律异步，同步实现也应经回调返回结果。
 *
 * 刻意不提供单件增删接口——当前只有一个写入方，事务语义留到接入服务端时再定。
 */
UCLASS(MinimalAPI, Abstract)
class UStashStorageProvider : public UObject
{
	GENERATED_BODY()

public:
	/** 读取指定玩家的仓库快照。 */
	UE_API virtual void LoadStash(const ULocalPlayer* LocalPlayer, FStashLoadComplete OnComplete);

	/** 写入指定玩家的仓库快照。 */
	UE_API virtual void SaveStash(const ULocalPlayer* LocalPlayer, const FStashSnapshot& Snapshot, FStashSaveComplete OnComplete);

	/**
	 * 本存储是否为权威数据源。
	 *
	 * 本地存档可被玩家改写，故本地实现返回 false。决议层据此得知「玩家确实拥有」
	 * 这一条当前只是软约束，而模式白名单之类的硬约束不受影响。
	 */
	UE_API virtual bool IsAuthoritative() const;
};

#undef UE_API
