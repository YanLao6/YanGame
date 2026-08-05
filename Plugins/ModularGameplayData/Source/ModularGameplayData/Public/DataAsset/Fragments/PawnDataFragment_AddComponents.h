// Copyright Chronicler.

#pragma once

#include "DataAsset/PawnDataFragment.h"
#include "UObject/SoftObjectPtr.h"

#include "PawnDataFragment_AddComponents.generated.h"

#define UE_API MODULARGAMEPLAYDATA_API

class UActorComponent;

/**
 * 一条组件注入配置。
 *
 * 组件在 Server 与 Client 上各自创建，不依赖动态组件复制；两个开关决定其在哪一端存在。
 */
USTRUCT()
struct FPawnDataComponentEntry
{
	GENERATED_BODY()

	/** 要挂载的组件类。 */
	UPROPERTY(EditDefaultsOnly, Category = "Component")
	TSoftClassPtr<UActorComponent> ComponentClass;

	/** 在具备 Authority 的一端创建。 */
	UPROPERTY(EditDefaultsOnly, Category = "Component")
	bool bServerComponent = true;

	/** 在客户端创建。 */
	UPROPERTY(EditDefaultsOnly, Category = "Component")
	bool bClientComponent = true;
};

/**
 * 记录本次注入创建的组件，供撤销时销毁。
 */
UCLASS(MinimalAPI)
class UPawnDataFragmentState_AddComponents : public UPawnDataFragmentState
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<TWeakObjectPtr<UActorComponent>> AddedComponents;
};

/**
 * 组件注入 Fragment。
 *
 * 对应 GameFeatureAction 的 Add Components，但作用域收敛到单个玩家而非整类 Actor。
 */
UCLASS(MinimalAPI, Const, DisplayName = "Add Components")
class UPawnDataFragment_AddComponents : public UPawnDataFragment
{
	GENERATED_BODY()

public:
	//~Begin UPawnDataFragment Interface
	virtual EPawnDataFragmentScope GetScope() const override { return Scope; }
	/** 返回 false 以便客户端也能创建 bClientComponent 条目，端内过滤在 Apply 中完成。 */
	virtual bool                    RequiresAuthority() const override { return false; }
	UE_API virtual UPawnDataFragmentState* Apply(const FPawnDataFragmentContext& Context) const override;
	UE_API virtual void                    Revoke(const FPawnDataFragmentContext& Context, UPawnDataFragmentState* State) const override;
#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsFragmentValid(FDataValidationContext& Context) const override;
#endif
	//~End UPawnDataFragment Interface

	/**
	 * 挂载目标。
	 *
	 * 默认挂到 Pawn，随 Pawn 销毁回收，适合武器模型、特效等随身表现组件；
	 * 背包、快捷栏等需要跨重生保留内容的组件应选 Controller。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Component")
	EPawnDataFragmentScope Scope = EPawnDataFragmentScope::Pawn;

	/** 要挂载的组件列表。 */
	UPROPERTY(EditDefaultsOnly, Category = "Component", Meta = (TitleProperty = "{ComponentClass}"))
	TArray<FPawnDataComponentEntry> Components;
};

#undef UE_API
