// Fill out your copyright notice in the Description page of Project Settings.


#include "DataAsset/Fragments/PawnDataFragment_TeamAppearance.h"

#include "AsyncAction_ObserveTeamColors.h"
#include "GameFramework/Pawn.h"
#include "ModularTeamDisplayAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PawnDataFragment_TeamAppearance)


void UPawnDataFragmentState_TeamAppearance::HandleTeamColorsChanged(bool bTeamSet, int32 TeamId, const UModularTeamDisplayAsset* DisplayAsset)
{
	APawn* Pawn = TargetPawn.Get();
	if (Pawn == nullptr || DisplayAsset == nullptr)
	{
		return;
	}

	// ApplyToActor 只读取展示资产的参数表、不改写自身，委托签名带来的 const 在此解除
	const_cast<UModularTeamDisplayAsset*>(DisplayAsset)->ApplyToActor(Pawn, bIncludeChildActors);
}

UPawnDataFragmentState* UPawnDataFragment_TeamAppearance::Apply(const FPawnDataFragmentContext& Context) const
{
	APawn* Pawn = Context.Pawn;
	if (Pawn == nullptr)
	{
		return nullptr;
	}

	// 专用服务器不渲染，创建动态材质实例纯属浪费
	if (Pawn->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}

	UPawnDataFragmentState_TeamAppearance* State = NewObject<UPawnDataFragmentState_TeamAppearance>(Pawn);
	State->TargetPawn         = Pawn;
	State->bIncludeChildActors = bIncludeChildActors;

	// 复用既有观察器：它同时监听队伍变更与展示资产变更，换队时还会自动迁移资产监听
	State->ColorObserver = UAsyncAction_ObserveTeamColors::ObserveTeamColors(Pawn);
	if (State->ColorObserver != nullptr)
	{
		// Activate 会立即广播一次当前配色，故必须先绑定后激活，否则首次着色丢失
		State->ColorObserver->OnTeamChanged.AddDynamic(State, &UPawnDataFragmentState_TeamAppearance::HandleTeamColorsChanged);
		State->ColorObserver->Activate();
	}

	return State;
}

void UPawnDataFragment_TeamAppearance::Revoke(const FPawnDataFragmentContext& Context, UPawnDataFragmentState* State) const
{
	UPawnDataFragmentState_TeamAppearance* TeamState = Cast<UPawnDataFragmentState_TeamAppearance>(State);
	if (TeamState == nullptr)
	{
		return;
	}

	if (TeamState->ColorObserver != nullptr)
	{
		TeamState->ColorObserver->OnTeamChanged.RemoveAll(TeamState);
		TeamState->ColorObserver->Cancel();
		TeamState->ColorObserver = nullptr;
	}
}
