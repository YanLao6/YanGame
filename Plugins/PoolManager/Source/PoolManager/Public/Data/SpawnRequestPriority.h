// Copyright (c) Yevhenii Selivanov

#pragma once

#include "SpawnRequestPriority.generated.h"

/**
 * 影响请求处理顺序的优先级。
 * 优先级越高越先被处理。
 */
UENUM(BlueprintType)
enum class ESpawnRequestPriority : uint8
{
	None UMETA(Hidden),
	///< 追加到队列末尾，适用于绝大多数场景
	Normal,
	///< 排在所有 High 请求之后、Normal 请求之前，可加快处理速度
	Medium,
	///< 保证请求插入队列头部，下一帧即被处理；适用于对延迟极敏感的紧急激活，但会推迟其他请求
	High,
	///< 当前帧立即处理，不入队；由于是同步处理，不应用于批量请求，调用过多可能导致 CPU 卡顿
	Critical,
};
