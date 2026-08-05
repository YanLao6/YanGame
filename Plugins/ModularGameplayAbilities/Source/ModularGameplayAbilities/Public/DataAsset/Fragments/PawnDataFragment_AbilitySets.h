// Copyright Chronicler.

#pragma once

#include "DataAsset/PawnDataFragment.h"
#include "GameplayAbilities/ModularAbilitySet.h"

#include "PawnDataFragment_AbilitySets.generated.h"

#define UE_API MODULARGAMEPLAYABILITIES_API

/**
 * 记录本次授予产生的 Handle，供撤销时精确回收。
 */
UCLASS(MinimalAPI)
class UPawnDataFragmentState_AbilitySets : public UPawnDataFragmentState
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FModularAbilitySet_GrantedHandles> GrantedHandles;
};

/**
 * AbilitySet 授予 Fragment。
 *
 * 对应 GameFeatureAction 的 Add Abilities，但作用域为单个玩家。
 * 授予落在 PlayerState 的 ASC 上，因此跨 Pawn 重生存活，撤销必须显式执行。
 *
 * 回收依赖逐条 Handle，不得改用整体清空：ASC 上同时存在 Experience 级授予，
 * 整体清空会使其永久丢失且不会被重新授予。
 */
UCLASS(MinimalAPI, Const, DisplayName = "Ability Sets")
class UPawnDataFragment_AbilitySets : public UPawnDataFragment
{
	GENERATED_BODY()

public:
	//~Begin UPawnDataFragment Interface
	virtual EPawnDataFragmentScope  GetScope() const override { return EPawnDataFragmentScope::PlayerState; }
	virtual bool                    RequiresAuthority() const override { return true; }
	UE_API virtual UPawnDataFragmentState* Apply(const FPawnDataFragmentContext& Context) const override;
	UE_API virtual void                    Revoke(const FPawnDataFragmentContext& Context, UPawnDataFragmentState* State) const override;
#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsFragmentValid(FDataValidationContext& Context) const override;
#endif
	//~End UPawnDataFragment Interface

	/** 要授予的 AbilitySet 列表。 */
	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TArray<TObjectPtr<UModularAbilitySet>> AbilitySets;
};

#undef UE_API
