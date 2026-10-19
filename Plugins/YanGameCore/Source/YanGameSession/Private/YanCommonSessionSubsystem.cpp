// Copyright YanLao.

#include "YanCommonSessionSubsystem.h"

#include "YanSessionKeys.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanCommonSessionSubsystem)

UCommonSession_HostSessionRequest* UYanCommonSessionSubsystem::CreateOnlineHostSessionRequest()
{
	UCommonSession_HostSessionRequest* Request = Super::CreateOnlineHostSessionRequest();

	// 房间码随 Lobby 数据公开，客机凭码即可定位到本房间
	Request->CustomSettings.Add(YanSessionKeys::Project, YanSessionKeys::ProjectValue);
	Request->CustomSettings.Add(YanSessionKeys::RoomCode, YanSessionKeys::GenerateRoomCode());
	Request->CustomSettings.Add(YanSessionKeys::Private, YanSessionKeys::PrivateFalse);

	return Request;
}

UCommonSession_SearchSessionRequest* UYanCommonSessionSubsystem::CreateOnlineSearchSessionRequest()
{
	UCommonSession_SearchSessionRequest* Request = Super::CreateOnlineSearchSessionRequest();

	// 默认只搜公开房间；房间码搜索由 UYanSessionManager 自行移除私有过滤
	Request->CustomFilters.Add(YanSessionKeys::Project, YanSessionKeys::ProjectValue);
	Request->CustomFilters.Add(YanSessionKeys::Private, YanSessionKeys::PrivateFalse);

	return Request;
}
