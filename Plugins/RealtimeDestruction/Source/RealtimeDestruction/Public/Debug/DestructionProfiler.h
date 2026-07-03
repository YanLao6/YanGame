// Copyright (c) 2026 LazyDevelopers <lazydeveloper24@gmail.com>. All rights reserved.
// This plugin is distributed under the Fab Standard License.
//
// This product was independently developed by us while participating in the Epic Project, a developer-support
// program of the KRAFTON JUNGLE GameTech Lab. All rights, title, and interest in and to the product are exclusively
// vested in us. Krafton, Inc. was not involved in its development and distribution and disclaims all representations
// and warranties, express or implied, and assumes no responsibility or liability for any consequences arising from
// the use of this product.

// DestructionProfiler.h
// 破坏系统性能 Profiling 宏与统计收集
//
// 功能：
// - DESTRUCTION_SCOPE_TIMER：超过 16ms 时发出警告 + 统计收集
// - DESTRUCTION_PROFILE_SCOPE：Unreal Insights 集成 + 统计收集
// - 统计输出/重置/CSV 导出

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"

#if !UE_BUILD_SHIPPING
#include "ProfilingDebugging/CpuProfilerTrace.h"
#endif

//=============================================================================
// 日志分类
//=============================================================================
DECLARE_LOG_CATEGORY_EXTERN(LogDestructionProfiler, Log, All);

//=============================================================================
// 全局统计收集器（单例）
//=============================================================================
class REALTIMEDESTRUCTION_API FDestructionProfilerStats
{
public:
	/** 单例实例 */
	static FDestructionProfilerStats& Get();

	/** 单个 Scope 的统计数据 */
	struct FScopeStats
	{
		int32 Count = 0;
		double TotalTimeMs = 0.0;
		double AvgTimeMs = 0.0;
		double MaxTimeMs = 0.0;
		double MinTimeMs = DBL_MAX;
		int32 OverThresholdCount = 0;  // 超过 16ms 的次数
	};

	//-------------------------------------------------------------------
	// 统计记录
	//-------------------------------------------------------------------

	/** 记录 Scope 耗时 */
	void RecordScopeTime(const FString& ScopeName, double TimeMs);

	/** 记录 Boolean 操作耗时 */
	void RecordBooleanOp(double TimeMs);

	/** 记录碰撞更新耗时 */
	void RecordCollisionUpdate(double TimeMs);

	/** 记录网络操作耗时 */
	void RecordNetworkOp(double TimeMs);

	//-------------------------------------------------------------------
	// 统计查询
	//-------------------------------------------------------------------

	/** 获取指定 Scope 的统计数据 */
	FScopeStats GetScopeStats(const FString& ScopeName) const;

	/** 获取所有统计数据 */
	TMap<FString, FScopeStats> GetAllStats() const;

	/** 检查指定 Scope 是否已有记录 */
	bool HasStats(const FString& ScopeName) const;

	//-------------------------------------------------------------------
	// 统计管理
	//-------------------------------------------------------------------

	/** 重置统计数据 */
	void ResetStats();

	/** 导出为 CSV 文件 */
	bool ExportToCSV(const FString& FilePath = TEXT("")) const;

	/** 将统计数据输出到 Console */
	void PrintStats() const;

	/** 输出指定 Scope 的统计数据 */
	void PrintScopeStats(const FString& ScopeName) const;

	//-------------------------------------------------------------------
	// 设置
	//-------------------------------------------------------------------

	/** 设置警告阈值（默认 16ms） */
	void SetWarningThreshold(double ThresholdMs) { WarningThresholdMs = ThresholdMs; }

	/** 获取警告阈值 */
	double GetWarningThreshold() const { return WarningThresholdMs; }

private:
	FDestructionProfilerStats() = default;

	mutable FCriticalSection StatsLock;
	TMap<FString, FScopeStats> ScopeStatsMap;

	// 超过 1 帧（60fps = 16ms）的警告阈值
	double WarningThresholdMs = 16.0;
};

//=============================================================================
// Scope 计时器类
//=============================================================================
class REALTIMEDESTRUCTION_API FDestructionScopeTimer
{
public:
	/**
	 * 构造函数
	 * @param InScopeName - Scope 名称
	 * @param bLogWarning - 超过 16ms 时是否输出警告日志
	 */
	FDestructionScopeTimer(const TCHAR* InScopeName, bool bLogWarning = true);

	/** 析构函数：测量经过时间并记录统计数据 */
	~FDestructionScopeTimer();

	// 禁止拷贝和移动
	FDestructionScopeTimer(const FDestructionScopeTimer&) = delete;
	FDestructionScopeTimer& operator=(const FDestructionScopeTimer&) = delete;

private:
	FString ScopeName;
	double StartTime;
	bool bLogWarningOnThreshold;
};

//=============================================================================
// 宏定义
//=============================================================================

#if !UE_BUILD_SHIPPING

/**
 * DESTRUCTION_SCOPE_TIMER(Name)
 * - Scope 退出时测量耗时
 * - 超过 16ms 时记录警告日志
 * - 自动统计收集
 *
 * 用法：
 * void MyFunction()
 * {
 *     DESTRUCTION_SCOPE_TIMER(MyFunction);
 *     // ... 代码 ...
 * }
 */
#define DESTRUCTION_SCOPE_TIMER(Name) \
	FDestructionScopeTimer _ScopeTimer_##Name(TEXT(#Name), true)

/**
 * DESTRUCTION_SCOPE_TIMER_NO_WARNING(Name)
 * - Scope 退出时测量耗时
 * - 仅收集统计，不输出警告日志
 */
#define DESTRUCTION_SCOPE_TIMER_NO_WARNING(Name) \
	FDestructionScopeTimer _ScopeTimer_##Name(TEXT(#Name), false)

/**
 * DESTRUCTION_PROFILE_SCOPE(Name)
 * - 集成 Unreal Insights（TRACE_CPUPROFILER_EVENT_SCOPE）
 * - 同时收集计时统计
 *
 * 在 Unreal Insights 中显示为 "Destruction_Name"
 */
#define DESTRUCTION_PROFILE_SCOPE(Name) \
	TRACE_CPUPROFILER_EVENT_SCOPE(Destruction_##Name); \
	FDestructionScopeTimer _ProfileTimer_##Name(TEXT(#Name), true)

/**
 * DESTRUCTION_PROFILE_SCOPE_VERBOSE(Name)
 * - Insights 集成 + 进入/退出日志记录
 */
#define DESTRUCTION_PROFILE_SCOPE_VERBOSE(Name) \
	TRACE_CPUPROFILER_EVENT_SCOPE(Destruction_##Name); \
	FDestructionScopeTimer _ProfileTimer_##Name(TEXT(#Name), true); \
	UE_LOG(LogDestructionProfiler, Verbose, TEXT(">>> Enter: %s"), TEXT(#Name))

//=============================================================================
// 专用宏（Boolean 操作、碰撞等）
//=============================================================================

/** Boolean 操作 Profiling */
#define DESTRUCTION_PROFILE_BOOLEAN() \
	TRACE_CPUPROFILER_EVENT_SCOPE(Destruction_BooleanOp); \
	FDestructionScopeTimer _BooleanTimer(TEXT("BooleanOp"), true)

/** 碰撞更新 Profiling */
#define DESTRUCTION_PROFILE_COLLISION() \
	TRACE_CPUPROFILER_EVENT_SCOPE(Destruction_CollisionUpdate); \
	FDestructionScopeTimer _CollisionTimer(TEXT("CollisionUpdate"), true)

/** 网络操作 Profiling */
#define DESTRUCTION_PROFILE_NETWORK() \
	TRACE_CPUPROFILER_EVENT_SCOPE(Destruction_NetworkOp); \
	FDestructionScopeTimer _NetworkTimer(TEXT("NetworkOp"), false)

/** 网格构建 Profiling */
#define DESTRUCTION_PROFILE_MESH_BUILD() \
	TRACE_CPUPROFILER_EVENT_SCOPE(Destruction_MeshBuild); \
	FDestructionScopeTimer _MeshBuildTimer(TEXT("MeshBuild"), true)

#else // UE_BUILD_SHIPPING

// Shipping 构建中禁用所有宏
#define DESTRUCTION_SCOPE_TIMER(Name)
#define DESTRUCTION_SCOPE_TIMER_NO_WARNING(Name)
#define DESTRUCTION_PROFILE_SCOPE(Name)
#define DESTRUCTION_PROFILE_SCOPE_VERBOSE(Name)
#define DESTRUCTION_PROFILE_BOOLEAN()
#define DESTRUCTION_PROFILE_COLLISION()
#define DESTRUCTION_PROFILE_NETWORK()
#define DESTRUCTION_PROFILE_MESH_BUILD()

#endif // !UE_BUILD_SHIPPING
