// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "GameplayTagStack.generated.h"

struct FGameplayTagStackContainer;
struct FNetDeltaSerializeInfo;

/**
 * 单层 GameplayTag 计数（Tag + StackCount），用于 FastArraySerializer 条目。
 */
USTRUCT(BlueprintType)
struct MODULARGAMEPLAYEXPERIENCES_API FGameplayTagStack : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FGameplayTagStack()
	{}

	FGameplayTagStack(FGameplayTag InTag, int32 InStackCount)
		: Tag(InTag)
		, StackCount(InStackCount)
	{
	}

	/** 调试输出：TagxCount。 */
	FString GetDebugString() const;

private:
	friend FGameplayTagStackContainer;

	UPROPERTY()
	FGameplayTag Tag;

	UPROPERTY()
	int32 StackCount = 0;
};

/** GameplayTag 栈容器（支持 NetDeltaSerialize）。 */
USTRUCT(BlueprintType)
struct MODULARGAMEPLAYEXPERIENCES_API FGameplayTagStackContainer : public FFastArraySerializer
{
	GENERATED_BODY()

	FGameplayTagStackContainer()
	//	: Owner(nullptr)
	{
	}

public:
	/** 为指定 Tag 增加 StackCount（<=0 不生效）。 */
	void AddStack(FGameplayTag Tag, int32 StackCount);

	/** 为指定 Tag 减少 StackCount（<=0 不生效）。 */
	void RemoveStack(FGameplayTag Tag, int32 StackCount);

	/** 返回 Tag 的栈深度（不存在则为 0）。 */
	int32 GetStackCount(FGameplayTag Tag) const
	{
		return TagToCountMap.FindRef(Tag);
	}

	/** 是否至少有一层该 Tag。 */
	bool ContainsTag(FGameplayTag Tag) const
	{
		return TagToCountMap.Contains(Tag);
	}

	//~FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	//~End of FFastArraySerializer contract

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FGameplayTagStack, FGameplayTagStackContainer>(Stacks, DeltaParms, *this);
	}

private:
	// 复制的 Tag 栈数组
	UPROPERTY()
	TArray<FGameplayTagStack> Stacks;
	
	// Tag -> 合计计数，加速查询
	TMap<FGameplayTag, int32> TagToCountMap;
};

template<>
struct TStructOpsTypeTraits<FGameplayTagStackContainer> : public TStructOpsTypeTraitsBase2<FGameplayTagStackContainer>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
