#pragma once

#include "Algo/MaxElement.h"
#include "Algo/MinElement.h"
#include "ChartCreation.h"
#include "Performance/YanPerformanceStatTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "YanPerformanceStatSubsystem.generated.h"

#define UE_API YANGAMESETTING_API

class UYanPerformanceStatSubsystem;

/**
 * 固定长度的环形采样缓冲，用于取某项统计的最新值与最小 / 最大 / 平均值。
 */
class FSampledStatCache
{
public:
	explicit FSampledStatCache(const int32 InSampleSize = 125)
		: SampleSize(InSampleSize)
	{
		check(InSampleSize > 0);

		Samples.Empty();
		Samples.AddZeroed(SampleSize);
	}

	void RecordSample(const double Sample)
	{
		Samples[CurrentSampleIndex] = Sample;

		if (++CurrentSampleIndex >= Samples.Num())
		{
			CurrentSampleIndex = 0;
		}
	}

	double GetCurrentCachedStat() const { return Samples[CurrentSampleIndex]; }

	double GetLastCachedStat() const
	{
		const int32 LastIndex = (CurrentSampleIndex - 1 < 0) ? Samples.Num() - 1 : CurrentSampleIndex - 1;

		return Samples[LastIndex];
	}

	/** 从最近一次采样处开始遍历全部样本。 */
	void ForEachCurrentSample(const TFunctionRef<void(const double Stat)> Func) const
	{
		int32 Index = CurrentSampleIndex;

		for (int32 i = 0; i < SampleSize; i++)
		{
			Func(Samples[Index]);

			if (++Index == SampleSize)
			{
				Index = 0;
			}
		}
	}

	int32 GetSampleSize() const { return SampleSize; }

	double GetAverage() const
	{
		double Sum = 0.0;
		ForEachCurrentSample([&Sum](const double Stat) { Sum += Stat; });

		return Sum / static_cast<double>(SampleSize);
	}

	double GetMin() const { return *Algo::MinElement(Samples); }
	double GetMax() const { return *Algo::MaxElement(Samples); }

private:
	const int32 SampleSize = 125;

	int32 CurrentSampleIndex = 0;

	TArray<double> Samples;
};

/**
 * 帧数据消费者，逐帧把引擎与网络统计写入采样缓冲。
 */
struct FYanPerformanceStatCache : public IPerformanceDataConsumer
{
public:
	explicit FYanPerformanceStatCache(UYanPerformanceStatSubsystem* InSubsystem)
		: MySubsystem(InSubsystem)
	{}

	//~Begin IPerformanceDataConsumer Interface
	UE_API virtual void StartCharting() override;
	UE_API virtual void ProcessFrame(const FFrameData& FrameData) override;
	UE_API virtual void StopCharting() override;
	//~End IPerformanceDataConsumer Interface

	/** 取某项统计的最新缓存值。 */
	UE_API double GetCachedStat(EYanDisplayablePerformanceStat Stat) const;

	/** 取某项统计的采样缓冲，可据此绘制随时间变化的曲线。 */
	UE_API const FSampledStatCache* GetCachedStatData(const EYanDisplayablePerformanceStat Stat) const;

protected:
	UE_API void RecordStat(const EYanDisplayablePerformanceStat Stat, const double Value);

	UYanPerformanceStatSubsystem* MySubsystem;

	TMap<EYanDisplayablePerformanceStat, FSampledStatCache> PerfStatCache;
};

/**
 * 性能统计子系统，向 UI 提供可读取的帧率、延迟与网络统计。
 */
UCLASS(MinimalAPI, BlueprintType)
class UYanPerformanceStatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Yan|Performance")
	UE_API double GetCachedStat(EYanDisplayablePerformanceStat Stat) const;

	UE_API const FSampledStatCache* GetCachedStatData(const EYanDisplayablePerformanceStat Stat) const;

	//~Begin USubsystem Interface
	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UE_API virtual void Deinitialize() override;
	//~End USubsystem Interface

protected:
	TSharedPtr<FYanPerformanceStatCache> Tracker;
};

#undef UE_API
