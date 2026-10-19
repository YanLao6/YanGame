// Copyright Chronicler.

#pragma once

#include "DataAsset/PawnDataFragment.h"
#include "GameplayAbilities/ModularAbilityGrant.h"
#include "GameplayAbilities/ModularAbilitySet.h"

#include "PawnDataFragment_AddAbilities.generated.h"

#define UE_API MODULARGAMEPLAYABILITIES_API

/**
 * 记录本次授予产生的 Handle 与实例，供撤销时精确回收。
 */
UCLASS(MinimalAPI)
class UPawnDataFragmentState_AddAbilities : public UPawnDataFragmentState
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FModularAbilitySet_GrantedHandles GrantedHandles;
};

/**
 * 技能授予 Fragment。
 *
 * 字段与 GameFeatureAction 的 Add Abilities 条目一致，但作用域为单个玩家。
 * 授予落在 PlayerState 的 ASC 上，因此跨 Pawn 重生存活，撤销必须显式执行。
 *
 * 回收依赖逐条 Handle，不得改用整体清空：ASC 上同时存在 Experience 级授予，
 * 整体清空会使其永久丢失且不会被重新授予。
 */
UCLASS(MinimalAPI, Const, DisplayName = "Add Abilities")
class UPawnDataFragment_AddAbilities : public UPawnDataFragment
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

	/** 要授予的单个 Ability。 */
	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TArray<FModularAbilityGrant> GrantedAbilities;

	/** 要授予的 AttributeSet，可附带初始化数据表。 */
	UPROPERTY(EditDefaultsOnly, Category = "Attributes")
	TArray<FModularAttributeSetGrant> GrantedAttributes;

	/** 要应用到自身的 GameplayEffect。 */
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TArray<FModularEffectGrant> GrantedEffects;

	/** 要授予的 AbilitySet。 */
	UPROPERTY(EditDefaultsOnly, Category = "Attributes", meta = (AssetBundles = "Client,Server"))
	TArray<TSoftObjectPtr<const UModularAbilitySet>> GrantedAbilitySets;
};

#undef UE_API
