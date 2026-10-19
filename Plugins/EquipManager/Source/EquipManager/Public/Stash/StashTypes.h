// 仓库数据结构声明。

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StashTypes.generated.h"

/**
 * 仓库中的一件物品。
 *
 * 物品一律以 ItemId 指代，绝不记录类路径——资产改名会让路径失效，
 * 而存档中的旧路径无从恢复。ItemId 与物品定义类的对应关系由物品目录资产维护。
 */
USTRUCT(BlueprintType)
struct FStashItemRecord
{
	GENERATED_BODY()

	/** 物品的稳定标识，对应目录资产中的一条登记。 */
	UPROPERTY(BlueprintReadOnly, Category=Stash)
	FName ItemId;

	/** 区分同一物品定义的不同个体，配装按本标识指代具体某一件。 */
	UPROPERTY(BlueprintReadOnly, Category=Stash)
	FGuid InstanceId;

	/** 堆叠数量。 */
	UPROPERTY(BlueprintReadOnly, Category=Stash)
	int32 StackCount = 1;

	/** 物品的可变状态，对应运行时实例上的 StatTags。 */
	UPROPERTY(BlueprintReadOnly, Category=Stash)
	TMap<FGameplayTag, int32> StatTags;
};

/** 配装中的一个槽位指派。 */
USTRUCT(BlueprintType)
struct FPlayerLoadoutSlot
{
	GENERATED_BODY()

	/** 目标快捷栏槽位下标。 */
	UPROPERTY(BlueprintReadOnly, Category=Stash)
	int32 SlotIndex = INDEX_NONE;

	/** 放入该槽位的仓库物品个体。 */
	UPROPERTY(BlueprintReadOnly, Category=Stash)
	FGuid InstanceId;
};

/**
 * 一套配装。
 *
 * 只记录玩家的意图，能否带进对局由模式的准入规则在服务器侧决定。
 */
USTRUCT(BlueprintType)
struct FPlayerLoadout
{
	GENERATED_BODY()

	/** 玩家自定的名称；非本地化内容，故不用 FText。 */
	UPROPERTY(BlueprintReadOnly, Category=Stash)
	FString LoadoutName;

	/** 槽位指派；未列出的槽位沿用模式配置。 */
	UPROPERTY(BlueprintReadOnly, Category=Stash)
	TArray<FPlayerLoadoutSlot> Slots;
};

/**
 * 仓库的完整快照，也是存档的内容本身。
 *
 * 读写以整份快照为单位：当前没有并发写入方，事务语义留到接入服务端时再谈。
 */
USTRUCT(BlueprintType)
struct FStashSnapshot
{
	GENERATED_BODY()

	/** 数据版本，结构变更时递增并在加载处迁移。 */
	UPROPERTY(BlueprintReadOnly, Category=Stash)
	int32 SchemaVersion = 1;

	/**
	 * 玩家拥有的全部物品。
	 *
	 * 当前解析不出定义类的记录同样保留在此：物品可能来自尚未启用的 GameFeature 插件，
	 * 清理掉就等于让玩家永久丢失该物品。
	 */
	UPROPERTY(BlueprintReadOnly, Category=Stash)
	TArray<FStashItemRecord> Items;

	/** 玩家保存的配装。 */
	UPROPERTY(BlueprintReadOnly, Category=Stash)
	TArray<FPlayerLoadout> Loadouts;

	/** 当前选中的配装下标，越界表示没有选中。 */
	UPROPERTY(BlueprintReadOnly, Category=Stash)
	int32 SelectedLoadoutIndex = INDEX_NONE;
};

/** 客户端上行的配装条目，以 ItemId 表达——服务器没有客户端的仓库，个体标识对它没有意义。 */
USTRUCT()
struct FLoadoutRequestEntry
{
	GENERATED_BODY()

	UPROPERTY()
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY()
	FName ItemId;
};

/** 客户端上行的配装请求。 */
USTRUCT()
struct FLoadoutRequest
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FLoadoutRequestEntry> Entries;
};
