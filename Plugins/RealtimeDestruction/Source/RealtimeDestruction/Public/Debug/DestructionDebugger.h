// Copyright (c) 2026 LazyDevelopers <lazydeveloper24@gmail.com>. All rights reserved.
// This plugin is distributed under the Fab Standard License.
//
// This product was independently developed by us while participating in the Epic Project, a developer-support
// program of the KRAFTON JUNGLE GameTech Lab. All rights, title, and interest in and to the product are exclusively
// vested in us. Krafton, Inc. was not involved in its development and distribution and disclaims all representations
// and warranties, express or implied, and assumes no responsibility or liability for any consequences arising from
// the use of this product.

// DestructionDebugger.h
// 破坏系统调试与可视化工具
//
// 功能：
// - 破坏位置可视化（DrawDebug）+ 基于网络模式的颜色区分
// - 统计追踪（每秒破坏次数、处理耗时等）
// - 网络统计（RPC 调用次数、验证失败次数、RTT）
// - 逐 Client 请求追踪
// - 历史记录（近期破坏请求）
// - 过滤（按 Actor 名称/半径）
// - 掉帧检测
// - CSV 导出
// - Console 命令支持
// - 屏幕 HUD 显示

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "DestructionDebugger.generated.h"

class URealtimeDestructibleMeshComponent;
class APlayerController;

/**
 * 破坏请求历史记录条目
 */
USTRUCT(BlueprintType)
struct FDestructionHistoryEntry
{
	GENERATED_BODY()

	/** 破坏发生时间戳 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug")
	float Timestamp = 0.0f;

	/** 破坏位置 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug")
	FVector ImpactPoint = FVector::ZeroVector;

	/** 碰撞法线方向 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug")
	FVector ImpactNormal = FVector::UpVector;

	/** 破坏半径 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug")
	float Radius = 0.0f;

	/** 发起者名称 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug")
	FString InstigatorName;

	/** 目标 Actor 名称 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug")
	FString TargetActorName;

	/** 网络模式 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug")
	FString NetMode;

	/** 处理耗时（毫秒） */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug")
	float ProcessingTimeMs = 0.0f;

	/** 是否由 Server 处理 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug")
	bool bFromServer = false;

	/** Client ID（仅在 Server 上有效） */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug")
	int32 ClientId = -1;
};

/**
 * 基础破坏统计数据
 */
USTRUCT(BlueprintType)
struct FDestructionStats
{
	GENERATED_BODY()

	/** 总破坏次数 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug")
	int32 TotalDestructions = 0;

	/** 每秒破坏次数 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug")
	float DestructionsPerSecond = 0.0f;

	/** 平均处理耗时（毫秒） */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug")
	float AverageProcessingTimeMs = 0.0f;

	/** 最大处理耗时（毫秒） */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug")
	float MaxProcessingTimeMs = 0.0f;

	/** 平均破坏半径 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug")
	float AverageRadius = 0.0f;

	/** 最近一秒内的破坏次数 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug")
	int32 DestructionsLastSecond = 0;
};

/**
 * 网络统计数据
 */
USTRUCT(BlueprintType)
struct FDestructionNetworkStats
{
	GENERATED_BODY()

	/** Server RPC 调用次数 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Network")
	int32 ServerRPCCount = 0;

	/** Multicast RPC 调用次数 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Network")
	int32 MulticastRPCCount = 0;

	/** 验证失败次数（Server 拒绝） */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Network")
	int32 ValidationFailures = 0;

	/** 平均 RTT（毫秒），仅 Client 有效 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Network")
	float AverageRTT = 0.0f;

	/** 最大 RTT（毫秒） */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Network")
	float MaxRTT = 0.0f;

	/** 最小 RTT（毫秒） */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Network")
	float MinRTT = 999999.0f;

	/** RTT 采样次数 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Network")
	int32 RTTSampleCount = 0;

	//--- 数据量统计 ---

	/** 总发送字节数 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Network")
	int64 TotalBytesSent = 0;

	/** 总接收字节数 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Network")
	int64 TotalBytesReceived = 0;

	/** 每次 RPC 平均发送字节数 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Network")
	float AvgBytesPerRPC = 0.0f;

	/** 压缩 RPC 调用次数 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Network")
	int32 CompactRPCCount = 0;

	/** 未压缩 RPC 调用次数 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Network")
	int32 UncompressedRPCCount = 0;

	/** 压缩节省的字节数（估算值） */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Network")
	int64 BytesSavedByCompression = 0;
};

/**
 * 逐 Client 请求统计（仅 Server）
 */
USTRUCT(BlueprintType)
struct FClientDestructionStats
{
	GENERATED_BODY()

	/** Client ID */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Network")
	int32 ClientId = -1;

	/** 玩家名称 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Network")
	FString PlayerName;

	/** 总请求次数 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Network")
	int32 TotalRequests = 0;

	/** 验证失败次数 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Network")
	int32 ValidationFailures = 0;

	/** 每秒请求次数 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Network")
	float RequestsPerSecond = 0.0f;

	/** 最近一次请求时间 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Network")
	float LastRequestTime = 0.0f;
};

/**
 * 性能统计数据
 */
USTRUCT(BlueprintType)
struct FDestructionPerformanceStats
{
	GENERATED_BODY()

	/** 掉帧次数（在破坏处理期间） */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Performance")
	int32 FrameDropCount = 0;

	/** 最大帧耗时（毫秒） */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Performance")
	float MaxFrameTimeMs = 0.0f;

	/** 单帧最大破坏处理次数 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Performance")
	int32 MaxDestructionsPerFrame = 0;

	/** 当前帧破坏次数 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Performance")
	int32 CurrentFrameDestructions = 0;

	//--- FPS 影响统计 ---

	/** 破坏前平均 FPS */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Performance")
	float AvgFPSBeforeDestruction = 0.0f;

	/** 破坏期间最低 FPS */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Performance")
	float MinFPSDuringDestruction = 999999.0f;

	/** 破坏期间平均 FPS 下降量 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Performance")
	float AvgFPSDrop = 0.0f;

	/** 最大 FPS 下降量 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Performance")
	float MaxFPSDrop = 0.0f;

	/** FPS 采样次数 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Performance")
	int32 FPSSampleCount = 0;

	//--- Boolean 操作统计 ---

	/** 平均 Boolean 操作耗时（毫秒） */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Performance")
	float AvgBooleanTimeMs = 0.0f;

	/** 最大 Boolean 操作耗时（毫秒） */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Performance")
	float MaxBooleanTimeMs = 0.0f;

	/** Boolean 操作采样次数 */
	UPROPERTY(BlueprintReadOnly, Category = "DestructionDebug|Performance")
	int32 BooleanSampleCount = 0;
};

/**
 * 破坏系统调试器
 *
 * 以 WorldSubsystem 形式实现，每个 World 自动创建
 */
UCLASS(ClassGroup = (RealtimeDestruction))
class REALTIMEDESTRUCTION_API UDestructionDebugger : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	//~ End USubsystem Interface

	/** Tick 函数（用于 FTSTicker，更新统计数据） */
	bool OnTick(float DeltaTime);

	//-------------------------------------------------------------------
	// 调试器控制
	//-------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category="Destruction|Debug")
	void SetEnabled(bool bEnable);

	UFUNCTION(BlueprintPure, Category="Destruction|Debug")
	bool IsEnabled() const { return bIsEnabled; }

	UFUNCTION(BlueprintCallable, Category="Destruction|Debug")
	void SetVisualizationEnabled(bool bEnable) { bShowVisualization = bEnable; }

	UFUNCTION(BlueprintPure, Category="Destruction|Debug")
	bool IsVisualizationEnabled() const { return bShowVisualization; }

	UFUNCTION(BlueprintCallable, Category="Destruction|Debug")
	void SetHUDEnabled(bool bEnable) { bShowHUD = bEnable; }

	UFUNCTION(BlueprintPure, Category="Destruction|Debug")
	bool IsHUDEnabled() const { return bShowHUD; }

	//-------------------------------------------------------------------
	// 破坏记录
	//-------------------------------------------------------------------

	/** 记录破坏请求 */
	UFUNCTION(BlueprintCallable, Category="Destruction|Debug")
	void RecordDestruction(
		const FVector& ImpactPoint,
		const FVector& ImpactNormal,
		float Radius,
		AActor* Instigator,
		AActor* TargetActor,
		float ProcessingTimeMs = 0.0f);

	/** 记录破坏请求（含网络信息） */
	void RecordDestructionEx(
		const FVector& ImpactPoint,
		const FVector& ImpactNormal,
		float Radius,
		AActor* Instigator,
		AActor* TargetActor,
		float ProcessingTimeMs,
		bool bFromServer,
		int32 ClientId);

	//-------------------------------------------------------------------
	// 网络统计记录（由 DestructionNetworkComponent 调用）
	//-------------------------------------------------------------------

	/** 记录 Server RPC 调用 */
	UFUNCTION(BlueprintCallable, Category="Destruction|Debug|Network")
	void RecordServerRPC();

	/** 记录 Multicast RPC 调用 */
	UFUNCTION(BlueprintCallable, Category="Destruction|Debug|Network")
	void RecordMulticastRPC();

	/** 记录验证失败 */
	UFUNCTION(BlueprintCallable, Category="Destruction|Debug|Network")
	void RecordValidationFailure(int32 ClientId = -1);

	/** 记录 RTT（由 Client 调用） */
	UFUNCTION(BlueprintCallable, Category="Destruction|Debug|Network")
	void RecordRTT(float RTTMs);

	/** 记录 Client 请求（由 Server 调用） */
	void RecordClientRequest(int32 ClientId, const FString& PlayerName, bool bValidationFailed = false);

	//-------------------------------------------------------------------
	// 网络数据量记录
	//-------------------------------------------------------------------

	/** 记录发送数据量（字节） */
	UFUNCTION(BlueprintCallable, Category="Destruction|Debug|Network")
	void RecordBytesSent(int32 Bytes, bool bIsCompact);

	/** 记录接收数据量（字节） */
	UFUNCTION(BlueprintCallable, Category="Destruction|Debug|Network")
	void RecordBytesReceived(int32 Bytes);

	/** 记录 Multicast RPC 调用（含数据量） */
	UFUNCTION(BlueprintCallable, Category="Destruction|Debug|Network")
	void RecordMulticastRPCWithSize(int32 OpCount, bool bIsCompact);

	/** 记录 Server RPC 调用（含数据量） */
	UFUNCTION(BlueprintCallable, Category="Destruction|Debug|Network")
	void RecordServerRPCWithSize(bool bIsCompact);

	//-------------------------------------------------------------------
	// 性能统计记录
	//-------------------------------------------------------------------

	/** 记录 FPS 影响（破坏前后的 FPS 差值） */
	UFUNCTION(BlueprintCallable, Category="Destruction|Debug|Performance")
	void RecordFPSImpact(float FPSBefore, float FPSAfter);

	/** 记录 Boolean 操作耗时（纯网格操作时间） */
	UFUNCTION(BlueprintCallable, Category="Destruction|Debug|Performance")
	void RecordBooleanOperationTime(float TimeMs);

	/** 获取当前 FPS（内部辅助方法） */
	UFUNCTION(BlueprintPure, Category="Destruction|Debug|Performance")
	float GetCurrentFPS() const;

	/** 保存请求时间戳用于 RTT 测量 */
	void StoreRequestTimestamp(uint32 RequestId, double Timestamp);

	/** 处理响应以计算 RTT（从 RequestId 反查时间戳） */
	void ProcessResponseForRTT(uint32 RequestId);

	//-------------------------------------------------------------------
	// 统计查询
	//-------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category="Destruction|Debug")
	FDestructionStats GetStats() const { return Stats; }

	UFUNCTION(BlueprintPure, Category="Destruction|Debug|Network")
	FDestructionNetworkStats GetNetworkStats() const { return NetworkStats; }

	UFUNCTION(BlueprintPure, Category="Destruction|Debug|Performance")
	FDestructionPerformanceStats GetPerformanceStats() const { return PerformanceStats; }

	UFUNCTION(BlueprintPure, Category="Destruction|Debug")
	TArray<FDestructionHistoryEntry> GetHistory() const { return History; }

	/** 获取逐 Client 统计（仅 Server） */
	UFUNCTION(BlueprintPure, Category="Destruction|Debug|Network")
	TArray<FClientDestructionStats> GetClientStats() const;

	UFUNCTION(BlueprintCallable, Category="Destruction|Debug")
	void ClearHistory();

	UFUNCTION(BlueprintCallable, Category="Destruction|Debug")
	void ResetStats();

	/** 重置所有统计（含网络和性能统计） */
	UFUNCTION(BlueprintCallable, Category="Destruction|Debug")
	void ResetAllStats();

	//-------------------------------------------------------------------
	// 过滤
	//-------------------------------------------------------------------

	/** 设置 Actor 名称过滤（空字符串禁用） */
	UFUNCTION(BlueprintCallable, Category="Destruction|Debug|Filter")
	void SetActorFilter(const FString& ActorNameFilter) { FilterActorName = ActorNameFilter; }

	/** 设置最小半径过滤（0 禁用） */
	UFUNCTION(BlueprintCallable, Category="Destruction|Debug|Filter")
	void SetMinRadiusFilter(float MinRadius) { FilterMinRadius = MinRadius; }

	/** 清除所有过滤条件 */
	UFUNCTION(BlueprintCallable, Category="Destruction|Debug|Filter")
	void ClearFilters() { FilterActorName.Empty(); FilterMinRadius = 0.0f; }

	//-------------------------------------------------------------------
	// 可视化
	//-------------------------------------------------------------------

	/** 在指定位置绘制调试可视化 */
	UFUNCTION(BlueprintCallable, Category="Destruction|Debug")
	void DrawDestructionDebug(const FVector& Location, const FVector& Normal, float Radius, float Duration = 2.0f);

	/** 按网络模式使用不同颜色绘制可视化 */
	void DrawDestructionDebugWithNetMode(const FVector& Location, const FVector& Normal, float Radius, bool bFromServer, float Duration = 2.0f);

	//-------------------------------------------------------------------
	// CSV 导出
	//-------------------------------------------------------------------

	/** 将历史记录导出为 CSV 文件 */
	UFUNCTION(BlueprintCallable, Category="Destruction|Debug|Export")
	bool ExportHistoryToCSV(const FString& FilePath);

	/** 将统计数据导出为 CSV 文件 */
	UFUNCTION(BlueprintCallable, Category="Destruction|Debug|Export")
	bool ExportStatsToCSV(const FString& FilePath);

	//-------------------------------------------------------------------
	// Console 命令函数
	//-------------------------------------------------------------------

	void PrintStats() const;
	void PrintNetworkStats() const;
	void PrintClientStats() const;
	void PrintPerformanceStats() const;
	void PrintHistory(int32 Count = 10) const;
	void PrintSessionSummary() const;

	//-------------------------------------------------------------------
	// 批处理/序列状态（用于 HUD 显示）
	//-------------------------------------------------------------------

	/** 获取待处理的 Server Batch Op 数量 */
	UFUNCTION(BlueprintPure, Category = "Destruction|Debug")
	int32 GetPendingBatchOpCount() const { return PendingBatchOpCount; }

	/** 获取 Server 序列号 */
	UFUNCTION(BlueprintPure, Category = "Destruction|Debug")
	int32 GetServerSequence() const { return ServerSequence; }

	/** 获取本地序列号 */
	UFUNCTION(BlueprintPure, Category = "Destruction|Debug")
	int32 GetLocalSequence() const { return LocalSequence; }

	/** 设置待处理 Batch Op 数量（由 RealtimeDestructibleMeshComponent 调用） */
	void SetPendingBatchOpCount(int32 Count) { PendingBatchOpCount = Count; }

	/** 设置 Server 序列号 */
	void SetServerSequence(int32 Seq) { ServerSequence = Seq; }

	/** 设置本地序列号 */
	void SetLocalSequence(int32 Seq) { LocalSequence = Seq; }

protected:
	void UpdateHUD();
	void UpdateDestructionsPerSecond(float DeltaTime);
	void UpdatePerformanceStats(float DeltaTime);
	FString GetNetModeString() const;
	bool PassesFilter(const FString& ActorName, float Radius) const;
	FColor GetColorForNetMode(bool bFromServer) const;

protected:
	//-------------------------------------------------------------------
	// 设置
	//-------------------------------------------------------------------

	UPROPERTY()
	bool bIsEnabled = true;

	UPROPERTY()
	bool bShowVisualization = true;

	UPROPERTY()
	bool bShowHUD = false;

	UPROPERTY()
	int32 MaxHistorySize = 100;

	UPROPERTY()
	float VisualizationDuration = 3.0f;

	/** Server 处理的破坏颜色（绿色） */
	UPROPERTY()
	FColor ServerColor = FColor::Green;

	/** Client 请求的破坏颜色（橙色） */
	UPROPERTY()
	FColor ClientColor = FColor::Orange;

	/** Standalone 破坏颜色（黄色） */
	UPROPERTY()
	FColor StandaloneColor = FColor::Yellow;

	/** 法线方向颜色 */
	UPROPERTY()
	FColor NormalColor = FColor::Blue;

	/** 过滤：Actor 名称（包含匹配） */
	FString FilterActorName;

	/** 过滤：最小半径 */
	float FilterMinRadius = 0.0f;

	/** 掉帧阈值（毫秒），低于 30 FPS 时触发 */
	float FrameDropThresholdMs = 33.33f;

	//-------------------------------------------------------------------
	// 数据
	//-------------------------------------------------------------------

	UPROPERTY()
	FDestructionStats Stats;

	UPROPERTY()
	FDestructionNetworkStats NetworkStats;

	UPROPERTY()
	FDestructionPerformanceStats PerformanceStats;

	UPROPERTY()
	TArray<FDestructionHistoryEntry> History;

	/** 逐 Client 统计（仅 Server），Key 为 ClientId */
	TMap<int32, FClientDestructionStats> ClientStatsMap;

	/** 近期破坏时间戳（最近 1 秒） */
	TArray<float> RecentDestructionTimestamps;

	/** 逐 Client 近期请求时间戳 */
	TMap<int32, TArray<float>> ClientRecentRequests;

	/** 总处理耗时（用于计算平均值） */
	double TotalProcessingTime = 0.0;

	/** 总破坏半径（用于计算平均值） */
	double TotalRadius = 0.0;

	/** 总 RTT（用于计算平均值） */
	double TotalRTT = 0.0;

	/** 总 FPS 下降量（用于计算平均值） */
	double TotalFPSDrop = 0.0;

	/** 破坏前的总 FPS（用于计算平均值） */
	double TotalFPSBefore = 0.0;

	/** 总 Boolean 操作耗时（用于计算平均值） */
	double TotalBooleanTime = 0.0;

	/** RTT 测量用请求时间戳映射（RequestId -> Timestamp） */
	TMap<uint32, double> PendingRTTRequests;

	/** 近期 FPS 采样（用于计算平均值） */
	TArray<float> RecentFPSSamples;

	/** 会话开始时间 */
	float SessionStartTime = 0.0f;

	/** 上一帧时间 */
	float LastFrameTime = 0.0f;

	/** 当前帧破坏次数 */
	int32 CurrentFrameDestructionCount = 0;

	/** FTSTicker 句柄 */
	FTSTicker::FDelegateHandle TickHandle;

	//-------------------------------------------------------------------
	// 批处理/序列状态（用于 HUD 显示）
	//-------------------------------------------------------------------

	/** 待处理 Server Batch Op 数量 */
	int32 PendingBatchOpCount = 0;

	/** Server 序列号 */
	int32 ServerSequence = 0;

	/** 本地序列号 */
	int32 LocalSequence = 0;
};
