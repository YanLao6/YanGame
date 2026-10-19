#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumRange.h"

#include "YanPerformanceStatTypes.generated.h"

/** 单项性能统计的展示形式。 */
UENUM(BlueprintType)
enum class EYanStatDisplayMode : uint8
{
	// 不显示
	Hidden,

	// 仅文字
	TextOnly,

	// 仅曲线图
	GraphOnly,

	// 文字与曲线图同时显示
	TextAndGraph
};

/** 可显示在屏幕上的性能统计项。 */
UENUM(BlueprintType)
enum class EYanDisplayablePerformanceStat : uint8
{
	// 客户端帧率（Hz）
	ClientFPS,

	// 服务器 tick 频率（Hz）
	ServerFPS,

	// 等待垂直同步或帧率上限的空闲时间（秒）
	IdleTime,

	// 整帧耗时（秒）
	FrameTime,

	// 游戏线程耗时（秒）
	FrameTime_GameThread,

	// 渲染线程耗时（秒）
	FrameTime_RenderThread,

	// RHI 线程耗时（秒）
	FrameTime_RHIThread,

	// GPU 推算耗时（秒）
	FrameTime_GPU,

	// 网络延迟（毫秒）
	Ping,

	// 入向丢包率（%）
	PacketLoss_Incoming,

	// 出向丢包率（%）
	PacketLoss_Outgoing,

	// 最近一秒收到的包数
	PacketRate_Incoming,

	// 最近一秒发出的包数
	PacketRate_Outgoing,

	// 收到的包平均大小（字节）
	PacketSize_Incoming,

	// 发出的包平均大小（字节）
	PacketSize_Outgoing,

	// 端到端总延迟（毫秒）
	Latency_Total,

	// 游戏模拟开始到驱动提交结束
	Latency_Game,

	// 系统渲染队列开始到 GPU 渲染结束
	Latency_Render,

	// 本地玩家 Pawn 的三维移动速度（cm/s），取自 Mover 的同步状态，含垂直分量
	MovementSpeed,

	// 新增统计项加在此行之上
	Count UMETA(Hidden)
};

ENUM_RANGE_BY_COUNT(EYanDisplayablePerformanceStat, EYanDisplayablePerformanceStat::Count);
