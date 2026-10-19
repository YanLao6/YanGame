#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;

namespace ColorTransfer
{
	/**
	 * 保留大厅选队名单，全员无缝切往 NextLevel，并使用目标关卡配置的 Experience；仅在服务器上调用。
	 * SteamSockets 下非无缝切关会监听失败，因此一律走 SeamlessTravel。
	 * @return 是否已发起切关；NextLevel 未配置时返回 false
	 */
	bool ServerTravelToLevel(const AActor* Instigator, const TSoftObjectPtr<UWorld>& NextLevel);
}
