#include "ColorTransferTravel.h"

#include "ColorTransferRuntimeModule.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "YanSessionManager.h"

bool ColorTransfer::ServerTravelToLevel(const AActor* Instigator, const TSoftObjectPtr<UWorld>& NextLevel)
{
	if (!IsValid(Instigator) || !Instigator->HasAuthority())
	{
		return false;
	}

	const FString NextMap = NextLevel.GetLongPackageName();
	if (NextMap.IsEmpty())
	{
		UE_LOG(LogColorTransfer, Error, TEXT("%s 没有配置下一关"), *GetNameSafe(Instigator));
		return false;
	}

	UWorld* World = Instigator->GetWorld();
	if (World == nullptr || !World->NextURL.IsEmpty() || World->IsInSeamlessTravel())
	{
		return false;
	}

	// 大厅选队名单只对单张关卡生效，不转交的话下一关会重新分队
	UYanSessionManager::CarryMatchRosterTo(Instigator, NextMap);

	FWorldContext& WorldContext = GEngine->GetWorldContextFromWorldChecked(World);
	const FURL PreviousURL = WorldContext.LastURL;
	// 相对切关会多次合并 LastURL，清除继承源以使用目标地图的 Experience。
	WorldContext.LastURL.RemoveOption(TEXT("Experience"));
	FURL URL = WorldContext.LastURL;
	URL.Map = NextMap;
#if WITH_EDITOR
	URL.Map = UWorld::StripPIEPrefixFromPackageName(URL.Map, World->StreamingLevelsPrefix);
#endif
	URL.AddOption(TEXT("SeamlessTravel"));

	FString URLString = URL.ToString();
	// 相对切关保留监听端口，传入的 URL 不包含主机地址与端口。
	URLString.RemoveFromStart(URL.GetHostPortString());
	UE_LOG(LogColorTransfer, Log, TEXT("%s 切关：%s"), *GetNameSafe(Instigator), *URLString);
	const bool bTravelAccepted = World->ServerTravel(URLString, /*bAbsolute=*/false);
	if (!bTravelAccepted)
	{
		WorldContext.LastURL = PreviousURL;
		UE_LOG(LogColorTransfer, Error, TEXT("%s 切关请求被拒绝：%s"), *GetNameSafe(Instigator), *URLString);
	}
	return bTravelAccepted;
}
