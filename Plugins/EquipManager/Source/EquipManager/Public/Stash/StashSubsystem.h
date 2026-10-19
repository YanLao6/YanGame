// 仓库子系统声明。

#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Stash/StashTypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "StashSubsystem.generated.h"

#define UE_API EQUIPMANAGER_API

class UStashStorageProvider;

/**
 * 玩家仓库。
 *
 * 表达「这个玩家账号拥有什么」以及「配了哪几套装」，与对局无关：
 * 它不认识 World、Pawn 或 GameMode，在主菜单里同样可用。
 * 物品能不能带进某一局由决议层按模式规则判断，仓库本身不做准入。
 */
UCLASS(MinimalAPI)
class UStashSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnStashChanged);

	/** 仓库内容变化时广播；粒度为整体变化，取用方重新读一遍即可。 */
	FOnStashChanged OnStashChanged;

	//~Begin USubsystem Interface
	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UE_API virtual void Deinitialize() override;
	//~End USubsystem Interface

	/** 仓库是否已完成读取；未读取完成时内容为空，不代表玩家一无所有。 */
	UFUNCTION(BlueprintPure, Category=Stash)
	bool IsLoaded() const { return bLoaded; }

	/** 返回玩家拥有的全部物品。 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=Stash)
	TArray<FStashItemRecord> GetItems() const { return Snapshot.Items; }

	/** 按个体标识查找物品，未找到时返回空。 */
	UE_API const FStashItemRecord* FindItem(const FGuid& InstanceId) const;

	/** 判断玩家是否拥有至少一件该标识的物品。 */
	UFUNCTION(BlueprintPure, Category=Stash)
	UE_API bool HasItem(FName ItemId) const;

	/** 向仓库添加一件物品，返回新个体的标识。 */
	UFUNCTION(BlueprintCallable, Category=Stash)
	UE_API FGuid AddItem(FName ItemId, int32 StackCount = 1);

	/** 移除某个个体，移除成功时返回真。 */
	UFUNCTION(BlueprintCallable, Category=Stash)
	UE_API bool RemoveItem(const FGuid& InstanceId);

	/** 返回玩家保存的全部配装。 */
	const TArray<FPlayerLoadout>& GetLoadouts() const { return Snapshot.Loadouts; }

	/** 返回当前选中的配装，未选中时返回空。 */
	UE_API const FPlayerLoadout* GetSelectedLoadout() const;

	/** 选中某套配装，下标越界时返回假。 */
	UFUNCTION(BlueprintCallable, Category=Stash)
	UE_API bool SelectLoadout(int32 LoadoutIndex);

	/** 写入配装，下标越界时追加一套；返回写入位置。 */
	UFUNCTION(BlueprintCallable, Category=Stash)
	UE_API int32 SaveLoadout(int32 LoadoutIndex, const FPlayerLoadout& Loadout);

	/**
	 * 把当前选中的配装转成可上行的请求。
	 *
	 * 个体标识只在本机有意义，故转换为物品标识；仓库中已不存在的个体会被略去。
	 */
	UE_API FLoadoutRequest MakeLoadoutRequest() const;

	/** 立即写盘，不等待节流窗口。 */
	UFUNCTION(BlueprintCallable, Category=Stash)
	UE_API void Flush();

private:
	// 读取仓库；子系统初始化时自动发起一次。
	void RequestLoad();

	// 记下改动并安排一次延后写盘：逐次改动都落盘在配装界面上会造成频繁写入。
	void MarkDirty();

	// 节流窗口到期后的写盘。
	bool HandleFlushTick(float DeltaTime);

private:
	// 当前仓库内容。
	UPROPERTY(Transient)
	FStashSnapshot Snapshot;

	// 读写仓库的存储实现。
	UPROPERTY(Transient)
	TObjectPtr<UStashStorageProvider> StorageProvider;

	// 延后写盘的定时句柄。
	FTSTicker::FDelegateHandle FlushTickHandle;

	// 是否已完成读取。
	bool bLoaded = false;

	// 是否有尚未写盘的改动。
	bool bDirty = false;
};

#undef UE_API
