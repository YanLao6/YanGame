#pragma once

#include "GameFramework/OnlineReplStructs.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "HeroSelectionSubsystem.generated.h"

#define UE_API HEROASSIGNMENTSYSTEM_API

/**
 * 角色选择结果的服务器权威存储。
 *
 * 挂在 GameInstance 上以跨越关卡切换存活。大厅完成选择后 ServerTravel 到对战关卡时，
 * PlayerState 会被销毁重建，其 CopyProperties 发生在新 PlayerState 的 PostInitializeComponents
 * 之后，晚于 PawnData 的首次决议，因此选择结果不能依赖 PlayerState 迁移。
 */
UCLASS(MinimalAPI)
class UHeroSelectionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 记录玩家的角色选择，PlayerId 无效时忽略。 */
	UE_API void SetSelectedHero(const FUniqueNetIdRepl& PlayerId, const FPrimaryAssetId& HeroId);

	/** 取回玩家的角色选择，未登记时返回无效 Id。 */
	UE_API FPrimaryAssetId GetSelectedHero(const FUniqueNetIdRepl& PlayerId) const;

	/** 清除单个玩家的选择，用于玩家离开或重新选人。 */
	UE_API void ClearSelection(const FUniqueNetIdRepl& PlayerId);

	/** 清除全部选择，用于对局结束返回大厅。 */
	UE_API void ClearAllSelections();

private:
	TMap<FUniqueNetIdRepl, FPrimaryAssetId> SelectionsByPlayer;
};

#undef UE_API
