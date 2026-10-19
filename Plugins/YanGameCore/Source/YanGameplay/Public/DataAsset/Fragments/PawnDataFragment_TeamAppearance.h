// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "DataAsset/PawnDataFragment.h"

#include "PawnDataFragment_TeamAppearance.generated.h"

#define UE_API YANGAMEPLAY_API

class APawn;
class UAsyncAction_ObserveTeamColors;
class UModularTeamDisplayAsset;

/**
 * 持有配色观察器，供撤销时解绑。
 *
 * 观察器以 Pawn 为 Outer，随 Pawn 销毁自动回收；显式 Revoke 用于中途换装 PawnData 的情形。
 */
UCLASS(MinimalAPI)
class UPawnDataFragmentState_TeamAppearance : public UPawnDataFragmentState
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UAsyncAction_ObserveTeamColors> ColorObserver;

	UPROPERTY()
	TWeakObjectPtr<APawn> TargetPawn;

	/** 是否连同子 Actor 一起着色，由 Fragment 配置带入 */
	bool bIncludeChildActors = true;

	/** 队伍变更、或所属队伍的展示资产被改动时重新着色 */
	UFUNCTION()
	UE_API void HandleTeamColorsChanged(bool bTeamSet, int32 TeamId, const UModularTeamDisplayAsset* DisplayAsset);
};

/**
 * 队伍配色 Fragment。
 *
 * 把 Pawn 上的网格与 Niagara 组件绑定到其所属队伍的展示资产：应用时立即着色一次，
 * 此后队伍变更或展示资产被编辑都会自动重刷。
 *
 * 队伍号到展示资产的映射配置在 GameState 建队组件的 UModularTeamCreationComponent::TeamsToCreate 上，
 * 本 Fragment 不重复承载——同一支队伍的配色对全场 Pawn 一致，属对局级而非角色级数据。
 */
UCLASS(MinimalAPI, Const, DisplayName = "Team Appearance")
class UPawnDataFragment_TeamAppearance : public UPawnDataFragment
{
	GENERATED_BODY()

public:
	//~Begin UPawnDataFragment Interface
	virtual EPawnDataFragmentScope GetScope() const override
	{
		return EPawnDataFragmentScope::Pawn;
	}

	/** 配色是本地表现，须在每个观察端各自执行，不能只在 Authority 上应用。 */
	virtual bool RequiresAuthority() const override
	{
		return false;
	}

	/** 同一 Pawn 挂多份观察器只会互相覆盖着色，无意义。 */
	virtual bool AllowsMultipleInstances() const override
	{
		return false;
	}

	UE_API virtual UPawnDataFragmentState* Apply(const FPawnDataFragmentContext& Context) const override;
	UE_API virtual void                    Revoke(const FPawnDataFragmentContext& Context, UPawnDataFragmentState* State) const override;
	//~End UPawnDataFragment Interface

	/** 是否一并为挂载的子 Actor（武器、挂件等）着色。 */
	UPROPERTY(EditDefaultsOnly, Category = "Team Appearance")
	bool bIncludeChildActors = true;
};

#undef UE_API
