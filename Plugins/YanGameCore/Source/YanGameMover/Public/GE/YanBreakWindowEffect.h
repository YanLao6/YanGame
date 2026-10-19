#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"

#include "YanBreakWindowEffect.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 卸力窗口效果：受击后一段时间内授予 Status.Break.Window，卸力技能凭此标签方可激活。
 *
 * 时长即窗口长度，默认 0.25 秒。这个数是被联机架构定死的：预测回滚下客户端看到的受击
 * 时刻与服务器判定存在偏差，再短就变成玄学。要改窗口长度，以本类为父类新建 GE 蓝图并调
 * Duration Magnitude，不要去改击退侧的任何东西——窗口长度是防守方的参数。
 */
UCLASS(MinimalAPI)
class UYanBreakWindowEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UE_API UYanBreakWindowEffect(const FObjectInitializer& ObjectInitializer);
};

#undef UE_API
