#include "Performance/YanPerformanceStatSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameMode/ModularExperienceGameState.h"
#include "MoverComponent.h"
#include "Performance/LatencyMarkerModule.h"
#include "Performance/YanPerformanceStatTypes.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanPerformanceStatSubsystem)

CSV_DEFINE_CATEGORY(YanPerformance, /*bIsEnabledByDefault=*/ false);

void FYanPerformanceStatCache::StartCharting()
{}

void FYanPerformanceStatCache::ProcessFrame(const FFrameData& FrameData)
{
	RecordStat(EYanDisplayablePerformanceStat::ClientFPS, (FrameData.TrueDeltaSeconds != 0.0) ? 1.0 / FrameData.TrueDeltaSeconds : 0.0);
	RecordStat(EYanDisplayablePerformanceStat::IdleTime, FrameData.IdleSeconds);
	RecordStat(EYanDisplayablePerformanceStat::FrameTime, FrameData.TrueDeltaSeconds);
	RecordStat(EYanDisplayablePerformanceStat::FrameTime_GameThread, FrameData.GameThreadTimeSeconds);
	RecordStat(EYanDisplayablePerformanceStat::FrameTime_RenderThread, FrameData.RenderThreadTimeSeconds);
	RecordStat(EYanDisplayablePerformanceStat::FrameTime_RHIThread, FrameData.RHIThreadTimeSeconds);
	RecordStat(EYanDisplayablePerformanceStat::FrameTime_GPU, FrameData.GPUTimeSeconds);

	UWorld* World = MySubsystem->GetGameInstance()->GetWorld();
	if (!World)
	{
		return;
	}

	if (const AModularExperienceGameState* GameState = World->GetGameState<AModularExperienceGameState>())
	{
		RecordStat(EYanDisplayablePerformanceStat::ServerFPS, GameState->GetServerFPS());
	}

	APlayerController* LocalPC = GEngine->GetFirstLocalPlayerController(World);
	if (!LocalPC)
	{
		return;
	}

	if (const APlayerState* PS = LocalPC->GetPlayerState<APlayerState>())
	{
		RecordStat(EYanDisplayablePerformanceStat::Ping, PS->GetPingInMilliseconds());
	}

	// 速度取 Mover 同步状态而非 ComponentVelocity，与移动模式里的速度参数同源；无 Pawn 或无 Mover 时记 0 避免残留旧值
	{
		const APawn* Pawn = LocalPC->GetPawn();
		const UMoverComponent* MoverComponent = Pawn ? Pawn->FindComponentByClass<UMoverComponent>() : nullptr;
		RecordStat(EYanDisplayablePerformanceStat::MovementSpeed, MoverComponent ? MoverComponent->GetVelocity().Size() : 0.0);
	}

	if (const UNetConnection* NetConnection = LocalPC->GetNetConnection())
	{
		RecordStat(EYanDisplayablePerformanceStat::PacketLoss_Incoming, NetConnection->GetInLossPercentage().GetAvgLossPercentage());
		RecordStat(EYanDisplayablePerformanceStat::PacketLoss_Outgoing, NetConnection->GetOutLossPercentage().GetAvgLossPercentage());

		RecordStat(EYanDisplayablePerformanceStat::PacketRate_Incoming, NetConnection->InPacketsPerSecond);
		RecordStat(EYanDisplayablePerformanceStat::PacketRate_Outgoing, NetConnection->OutPacketsPerSecond);

		RecordStat(EYanDisplayablePerformanceStat::PacketSize_Incoming,
			(NetConnection->InPacketsPerSecond != 0) ? (NetConnection->InBytesPerSecond / static_cast<float>(NetConnection->InPacketsPerSecond)) : 0.0f);
		RecordStat(EYanDisplayablePerformanceStat::PacketSize_Outgoing,
			(NetConnection->OutPacketsPerSecond != 0) ? (NetConnection->OutBytesPerSecond / static_cast<float>(NetConnection->OutPacketsPerSecond)) : 0.0f);
	}

	// 延迟统计由可选的厂商模块（如 Reflex）提供，未启用时整段跳过
	const TArray<ILatencyMarkerModule*> LatencyMarkerModules =
		IModularFeatures::Get().GetModularFeatureImplementations<ILatencyMarkerModule>(ILatencyMarkerModule::GetModularFeatureName());

	for (ILatencyMarkerModule* LatencyMarkerModule : LatencyMarkerModules)
	{
		if (!LatencyMarkerModule->GetEnabled())
		{
			continue;
		}

		const float TotalLatencyMs = LatencyMarkerModule->GetTotalLatencyInMs();
		if (TotalLatencyMs <= 0.0f)
		{
			continue;
		}

		RecordStat(EYanDisplayablePerformanceStat::Latency_Total, TotalLatencyMs);
		RecordStat(EYanDisplayablePerformanceStat::Latency_Game, LatencyMarkerModule->GetGameLatencyInMs());
		RecordStat(EYanDisplayablePerformanceStat::Latency_Render, LatencyMarkerModule->GetRenderLatencyInMs());

		// 以 CsvProfile Start / Stop 或 CsvProfile Frames=N 采集后，输出至 Saved\Profiling\CSV
#if CSV_PROFILER
		if (FCsvProfiler* Profiler = FCsvProfiler::Get())
		{
			static const FName TotalLatencyStatName  = TEXT("Yan_Latency_Total");
			static const FName GameLatencyStatName   = TEXT("Yan_Latency_Game");
			static const FName RenderLatencyStatName = TEXT("Yan_Latency_Render");

			Profiler->RecordCustomStat(TotalLatencyStatName, CSV_CATEGORY_INDEX(YanPerformance), TotalLatencyMs, ECsvCustomStatOp::Set);
			Profiler->RecordCustomStat(GameLatencyStatName, CSV_CATEGORY_INDEX(YanPerformance), LatencyMarkerModule->GetGameLatencyInMs(), ECsvCustomStatOp::Set);
			Profiler->RecordCustomStat(RenderLatencyStatName, CSV_CATEGORY_INDEX(YanPerformance), LatencyMarkerModule->GetRenderLatencyInMs(), ECsvCustomStatOp::Set);
		}
#endif

		break;
	}
}

void FYanPerformanceStatCache::StopCharting()
{}

void FYanPerformanceStatCache::RecordStat(const EYanDisplayablePerformanceStat Stat, const double Value)
{
	PerfStatCache.FindOrAdd(Stat).RecordSample(Value);
}

double FYanPerformanceStatCache::GetCachedStat(EYanDisplayablePerformanceStat Stat) const
{
	static_assert(static_cast<int32>(EYanDisplayablePerformanceStat::Count) == 19, "新增统计项后需同步检查此处");

	if (const FSampledStatCache* Cache = GetCachedStatData(Stat))
	{
		return Cache->GetLastCachedStat();
	}

	return 0.0;
}

const FSampledStatCache* FYanPerformanceStatCache::GetCachedStatData(const EYanDisplayablePerformanceStat Stat) const
{
	return PerfStatCache.Find(Stat);
}

void UYanPerformanceStatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Tracker = MakeShared<FYanPerformanceStatCache>(this);
	GEngine->AddPerformanceDataConsumer(Tracker);
}

void UYanPerformanceStatSubsystem::Deinitialize()
{
	GEngine->RemovePerformanceDataConsumer(Tracker);
	Tracker.Reset();
}

double UYanPerformanceStatSubsystem::GetCachedStat(EYanDisplayablePerformanceStat Stat) const
{
	return Tracker->GetCachedStat(Stat);
}

const FSampledStatCache* UYanPerformanceStatSubsystem::GetCachedStatData(const EYanDisplayablePerformanceStat Stat) const
{
	return Tracker->GetCachedStatData(Stat);
}
