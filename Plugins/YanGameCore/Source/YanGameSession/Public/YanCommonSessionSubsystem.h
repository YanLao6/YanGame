// Copyright YanLao.

#pragma once

#include "CommonSessionSubsystem.h"

#include "YanCommonSessionSubsystem.generated.h"

#define UE_API YANGAMESESSION_API

/**
 * 项目级会话子系统。
 *
 * 基类在存在子类时自动让位，因此所有经 CommonSession 发起的开房与搜索都会经过这里：
 * 开房请求统一带上项目标识、房间码与私有标记，搜索请求统一按项目标识过滤并排除私有房间。
 * Lyra 前端蓝图、QuickPlay 与 UYanSessionManager 无需各自处理。
 */
UCLASS(MinimalAPI)
class UYanCommonSessionSubsystem : public UCommonSessionSubsystem
{
	GENERATED_BODY()

public:
	//~Begin UCommonSessionSubsystem Interface
	UE_API virtual UCommonSession_HostSessionRequest* CreateOnlineHostSessionRequest() override;
	UE_API virtual UCommonSession_SearchSessionRequest* CreateOnlineSearchSessionRequest() override;
	//~End UCommonSessionSubsystem Interface
};

#undef UE_API
