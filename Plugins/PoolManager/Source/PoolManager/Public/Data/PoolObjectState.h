// Copyright (c) Yevhenii Selivanov

#pragma once

#include "PoolObjectState.generated.h"

/**
 * 对象在池中的状态
 */
UENUM(BlueprintType)
enum class EPoolObjectState : uint8
{
	///< 不由 Pool Manager 管理
	None,
	///< 位于池中，空闲且可被取出
	Inactive,
	///< 已从池中取出，可归还
	Active
};
