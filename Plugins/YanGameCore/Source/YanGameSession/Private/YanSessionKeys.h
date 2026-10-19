// 会话广播键与房间码规则，模块内部共用。

#pragma once

#include "CoreMinimal.h"

namespace YanSessionKeys
{
	// 公开到平台 Lobby 数据的键。开发期共用 Steam 测试 AppID(480) 时，
	// 项目标识用于把本项目的房间与其他项目的隔离开，Lyra 自带的键做不到这点。
	inline const FName Project(TEXT("YANPROJECT"));
	inline const FName RoomCode(TEXT("ROOMCODE"));

	// 私有房间在平台侧仍是公开 Lobby（否则房间码也搜不到），靠该键把它排除出浏览与快速匹配
	inline const FName Private(TEXT("YANPRIVATE"));

	inline const FString ProjectValue(TEXT("YanGame"));
	inline const FString PrivateTrue(TEXT("1"));
	inline const FString PrivateFalse(TEXT("0"));

	// 剔除 0/O、1/I 等易混字符，玩家口头或截图传码时不易出错
	inline const FString RoomCodeAlphabet(TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ23456789"));
	inline constexpr int32 RoomCodeLength = 6;

	inline FString GenerateRoomCode()
	{
		FString Code;
		Code.Reserve(RoomCodeLength);

		for (int32 Index = 0; Index < RoomCodeLength; ++Index)
		{
			Code.AppendChar(RoomCodeAlphabet[FMath::RandRange(0, RoomCodeAlphabet.Len() - 1)]);
		}

		return Code;
	}
}
