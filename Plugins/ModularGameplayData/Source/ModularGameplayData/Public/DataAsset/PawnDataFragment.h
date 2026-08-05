// Copyright Chronicler.

#pragma once

#include "UObject/Object.h"

#include "PawnDataFragment.generated.h"

#define UE_API MODULARGAMEPLAYDATA_API

class APawn;
class AController;
enum class EDataValidationResult : uint8;
class FDataValidationContext;

/**
 * Fragment 的应用目标，决定其生命周期边界。
 */
UENUM(BlueprintType)
enum class EPawnDataFragmentScope : uint8
{
	/** 应用到 PlayerState。跨 Pawn 重生存活，因此撤销必须显式执行。 */
	PlayerState,

	/**
	 * 应用到 Controller。同样跨 Pawn 重生存活，撤销必须显式执行。
	 *
	 * 背包、快捷栏等玩家级持久数据的宿主组件位于 Controller 上，
	 * 用 Pawn 作用域挂载会使其随每次重生销毁重建，连同已有内容一起丢失。
	 */
	Controller,

	/** 应用到 Pawn。随 Pawn 销毁自动回收。 */
	Pawn,
};

/**
 * Fragment 应用时的目标上下文。
 */
USTRUCT(BlueprintType)
struct FPawnDataFragmentContext
{
	GENERATED_BODY()

	/** 应用目标，与 Fragment 的 Scope 对应（PlayerState 或 Pawn）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Pawn Data")
	TObjectPtr<AActor> TargetActor = nullptr;

	/** PlayerState 作用域下应用时 Pawn 可能尚未生成，此时为空。 */
	UPROPERTY(BlueprintReadOnly, Category = "Pawn Data")
	TObjectPtr<APawn> Pawn = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Pawn Data")
	TObjectPtr<AController> Controller = nullptr;

	bool IsValid() const
	{
		return TargetActor != nullptr;
	}
};

/**
 * Fragment 应用后产生的可撤销状态。
 *
 * 由应用方（PlayerState / PawnComponent）持有。Fragment 资产自身保持无状态，
 * 使同一份 PawnData 可被多个玩家共享而不互相干扰。
 */
UCLASS(MinimalAPI, Abstract)
class UPawnDataFragmentState : public UObject
{
	GENERATED_BODY()
};

/**
 * PawnData 的可组合数据单元。
 *
 * 每个 Fragment 描述一项可独立应用与撤销的注入（技能、输入、组件、界面等）。
 * Fragment 为资产内联的不可变对象：Apply 不得写入自身成员，运行时状态一律通过
 * 返回的 UPawnDataFragmentState 交由应用方持有。
 */
UCLASS(MinimalAPI, Abstract, Const, DefaultToInstanced, EditInlineNew, CollapseCategories)
class UPawnDataFragment : public UObject
{
	GENERATED_BODY()

public:
	/** 本 Fragment 的应用目标。 */
	virtual EPawnDataFragmentScope GetScope() const
	{
		return EPawnDataFragmentScope::Pawn;
	}

	/**
	 * 是否仅在 Authority 上应用。
	 *
	 * 授予技能、挂载复制组件等必须为 true；输入绑定与界面需要在拥有者客户端执行，应返回 false。
	 */
	virtual bool RequiresAuthority() const
	{
		return true;
	}

	/**
	 * 是否允许同一份 PawnData（含其引用的 FragmentSet）中出现多个本类型实例。
	 *
	 * 以 FindFragment 单例语义被查询的 Fragment 必须返回 false：展平后 FragmentSet 先于
	 * 内联 Fragment，重复配置会使查询静默命中 Set 中那份，导致内联的特化配置不生效。
	 */
	virtual bool AllowsMultipleInstances() const
	{
		return true;
	}

	/**
	 * 应用本 Fragment。
	 *
	 * @return 供 Revoke 使用的状态对象；无需撤销时返回 nullptr。
	 */
	virtual UPawnDataFragmentState* Apply(const FPawnDataFragmentContext& Context) const
	{
		return nullptr;
	}

	/**
	 * 撤销先前 Apply 的内容。
	 *
	 * @param State 对应 Apply 的返回值，可能为 nullptr。
	 */
	virtual void Revoke(const FPawnDataFragmentContext& Context, UPawnDataFragmentState* State) const
	{
	}

#if WITH_EDITOR
	/** 供 PawnData 汇总校验，返回本 Fragment 的配置有效性。 */
	UE_API virtual EDataValidationResult IsFragmentValid(FDataValidationContext& Context) const;
#endif
};

#undef UE_API
