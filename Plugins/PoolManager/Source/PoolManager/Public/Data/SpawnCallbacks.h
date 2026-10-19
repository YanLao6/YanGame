// Copyright (c) Yevhenii Selivanov

#pragma once

struct FPoolObjectData;

typedef TFunction<void(const FPoolObjectData&)> FOnSpawnCallback;
typedef TFunction<void(const TArray<FPoolObjectData>&)> FOnSpawnAllCallback;

/**
 * 对象生成过程中触发的回调集合。
 */
struct POOLMANAGER_API FSpawnCallbacks
{
	/** 在注册到池之前返回完整的对象数据。 */
	FOnSpawnCallback OnPreRegistered = nullptr;

	/** 返回已生成并已注册的对象。 */
	FOnSpawnCallback OnPostSpawned = nullptr;
};
