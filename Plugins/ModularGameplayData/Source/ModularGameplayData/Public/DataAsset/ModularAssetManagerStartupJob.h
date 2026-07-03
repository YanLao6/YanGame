// Copyright Chronicler Software Corporation.


#pragma once

#include "Engine/StreamableManager.h"

DECLARE_DELEGATE_OneParam(FModularAssetManagerStartupJobSubstepProgress, float /*NewProgress*/);

/** 启动任务：从 FStreamableHandle 汇总子进度并对外汇报。 */
struct FModularAssetManagerStartupJob
{
	FModularAssetManagerStartupJobSubstepProgress SubstepProgressDelegate;
	TFunction<void(const FModularAssetManagerStartupJob&, TSharedPtr<FStreamableHandle>&)> JobFunc;
	FString JobName;
	float JobWeight;
	mutable double LastUpdate = 0;

	/** 构造一项启动任务（JobFunc 内可同步或发起异步加载）。 */
	FModularAssetManagerStartupJob
		(const FString& InJobName,
			const TFunction<void(const FModularAssetManagerStartupJob&, TSharedPtr<FStreamableHandle>&)>& InJobFunc,
			const float InJobWeight): JobFunc(InJobFunc),
			                          JobName(InJobName),
			                          JobWeight(InJobWeight)
	{}

	/** 执行加载逻辑；若创建了异步句柄则返回 FStreamableHandle。 */
	TSharedPtr<FStreamableHandle> DoJob() const;

	/** 向 Substep 委托汇报进度（0~1）。 */
	void UpdateSubstepProgress(const float NewProgress) const
	{
		SubstepProgressDelegate.ExecuteIfBound(NewProgress);
	}

	/**
	 * 根据 Streamable 句柄刷新子进度（限频，减轻 GetProgress 开销）。
	 * @param StreamableHandle 异步加载句柄
	 * @todo 按值传入 TSharedRef 可能有额外开销，可改为 const 引用等形式再评估。
	 */
	void UpdateSubstepProgressFromStreamable(const TSharedRef<FStreamableHandle> StreamableHandle) const
	{
		if (SubstepProgressDelegate.IsBound())
		{
			// GetProgress 会遍历依赖图，较昂贵，故与上一帧间隔至少 ~1/60s 再更新
			if (const double Now = FPlatformTime::Seconds(); LastUpdate - Now > 1.0 / 60)
			{
				SubstepProgressDelegate.Execute(StreamableHandle->GetProgress());
				LastUpdate = Now;
			}
		}
	}
};
