// Copyright Chronicler.

#pragma once

#include "DataAsset/PawnDataFragment.h"

#include "PawnDataFragment_TagRelationship.generated.h"

#define UE_API MODULARGAMEPLAYABILITIES_API

class UModularAbilityTagRelationshipMapping;

/**
 * Ability Tag 关系映射 Fragment。
 *
 * 写入 PlayerState 上 ASC 的 TagRelationshipMapping。同一 ASC 只保留一份映射，
 * 故撤销时清空而非恢复前值；一份 PawnData 中配置多个本类型 Fragment 时后者生效。
 */
UCLASS(MinimalAPI, Const, DisplayName = "Ability Tag Relationship")
class UPawnDataFragment_TagRelationship : public UPawnDataFragment
{
	GENERATED_BODY()

public:
	//~Begin UPawnDataFragment Interface
	virtual EPawnDataFragmentScope  GetScope() const override { return EPawnDataFragmentScope::PlayerState; }
	virtual bool                    RequiresAuthority() const override { return false; }
	UE_API virtual UPawnDataFragmentState* Apply(const FPawnDataFragmentContext& Context) const override;
	UE_API virtual void                    Revoke(const FPawnDataFragmentContext& Context, UPawnDataFragmentState* State) const override;
#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsFragmentValid(FDataValidationContext& Context) const override;
#endif
	//~End UPawnDataFragment Interface

	/** 描述 Ability Tag 之间阻塞与取消关系的映射资产。 */
	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TObjectPtr<UModularAbilityTagRelationshipMapping> TagRelationshipMapping;
};

#undef UE_API
