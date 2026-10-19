// 英雄专属开局配装 Fragment 声明。

#pragma once

#include "DataAsset/PawnDataFragment.h"
#include "UObject/SoftObjectPtr.h"

#include "PawnDataFragment_Loadout.generated.h"

#define UE_API EQUIPMANAGER_API

class UInGameLoadoutDefinition;

/**
 * 仅作为已应用的标记，撤销所需的配装定义取自 Fragment 自身。
 */
UCLASS(MinimalAPI)
class UPawnDataFragmentState_Loadout : public UPawnDataFragmentState
{
	GENERATED_BODY()
};

/**
 * 英雄专属开局配装 Fragment。
 *
 * 把一份配装交给控制器上的发放组件，与模式给全员的那份叠加：同一槽位由后加入者接管，
 * 不同槽位并存。本 Fragment 只交配置，物品何时发放由组件把控。
 *
 * 取 Pawn 作用域：配装随角色更替，而发放组件挂在跨角色存续的控制器上，
 * 经上下文中的控制器取用即可。
 */
UCLASS(MinimalAPI, Const, DisplayName = "Loadout")
class UPawnDataFragment_Loadout : public UPawnDataFragment
{
	GENERATED_BODY()

public:
	//~Begin UPawnDataFragment Interface
	virtual EPawnDataFragmentScope         GetScope() const override { return EPawnDataFragmentScope::Pawn; }
	virtual bool                           RequiresAuthority() const override { return true; }
	UE_API virtual UPawnDataFragmentState* Apply(const FPawnDataFragmentContext& Context) const override;
	UE_API virtual void                    Revoke(const FPawnDataFragmentContext& Context, UPawnDataFragmentState* State) const override;
#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsFragmentValid(class FDataValidationContext& Context) const override;
#endif
	//~End UPawnDataFragment Interface

	/** 该角色专属的开局配装。 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout", Meta = (AssetBundles = "Server"))
	TSoftObjectPtr<UInGameLoadoutDefinition> LoadoutDefinition;
};

#undef UE_API
