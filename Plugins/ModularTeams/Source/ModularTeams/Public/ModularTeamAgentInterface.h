// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericTeamAgentInterface.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"

#include "ModularTeamAgentInterface.generated.h"

#define UE_API MODULARTEAMS_API

template <typename InterfaceType> class TScriptInterface;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnModularTeamIndexChangedDelegate, UObject*, ObjectChangingTeam, int32, OldTeamID, int32, NewTeamID);

/** FGenericTeamId 的 NoTeam 对应蓝图与子系统侧的 INDEX_NONE。 */
inline int32 GenericTeamIdToInteger(FGenericTeamId ID)
{
	return (ID == FGenericTeamId::NoTeam) ? INDEX_NONE : (int32)ID;
}

inline FGenericTeamId IntegerToGenericTeamId(int32 ID)
{
	return (ID == INDEX_NONE) ? FGenericTeamId::NoTeam : FGenericTeamId((uint8)ID);
}

/** 可归属于某个队伍的 Actor 所实现的接口。 */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UModularTeamAgentInterface : public UGenericTeamAgentInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IModularTeamAgentInterface : public IGenericTeamAgentInterface
{
	GENERATED_IINTERFACE_BODY()

	/** 返回队伍变更委托；未持有委托的实现返回 nullptr。 */
	virtual FOnModularTeamIndexChangedDelegate* GetOnTeamIndexChangedDelegate() { return nullptr; }

	/** 队伍确实发生变化时才广播，避免重复赋值触发监听者刷新。 */
	static UE_API void ConditionalBroadcastTeamChanged(TScriptInterface<IModularTeamAgentInterface> This, FGenericTeamId OldTeamID, FGenericTeamId NewTeamID);

	FOnModularTeamIndexChangedDelegate& GetTeamChangedDelegateChecked()
	{
		FOnModularTeamIndexChangedDelegate* Result = GetOnTeamIndexChangedDelegate();
		check(Result);
		return *Result;
	}
};

#undef UE_API
