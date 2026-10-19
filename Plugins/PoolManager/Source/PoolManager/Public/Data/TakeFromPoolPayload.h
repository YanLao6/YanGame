// Copyright (c) Yevhenii Selivanov

#pragma once

#include "Math/Transform.h"

#include "TakeFromPoolPayload.generated.h"

/**
 * 从池中取出对象时携带的负载（Payload）数据。
 */
USTRUCT(BlueprintType)
struct FTakeFromPoolPayload
{
	GENERATED_BODY()

	/** 从池中取出的对象的 Transform。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient)
	FTransform Transform = FTransform::Identity;

	/** 对象是新生成的（而非从已有池中取出）时为 true。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient)
	bool bIsNewSpawned = false;
};
