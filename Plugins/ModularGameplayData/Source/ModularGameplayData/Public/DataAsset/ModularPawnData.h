// Copyright Chronicler.

#pragma once

#include "Engine/DataAsset.h"
#include "ModularPawnDataFragmentSet.h"
#include "PawnDataFragment.h"

#include "ModularPawnData.generated.h"

#define UE_API MODULARGAMEPLAYDATA_API

/**
 * Pawn 数据定义资产。
 *
 * 不可变（Const）数据资产。除 PawnClass 外的一切能力注入均由 Fragments 描述：
 * PawnClass 决定生成哪个 Pawn，逻辑上先于所有 Fragment，故不作为 Fragment 承载。
 */
UCLASS(MinimalAPI, BlueprintType, Const, Meta = (DisplayName = "Pawn Data", ShortTooltip = "Data asset used to define a Pawn."))
class UModularPawnData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 构造 PawnData 并初始化默认配置。 */
	UE_API explicit UModularPawnData(const FObjectInitializer& ObjectInitializer);

	/** 生成该 Pawn 时实例化的 Class（通常派生自项目侧 Modular Pawn/Character）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pawn")
	TSubclassOf<APawn> PawnClass;

	/**
	 * 复用的 Fragment 集合，构成本 Pawn 的基础层。
	 *
	 * 展平后整体先于内联 Fragments 应用，故内联条目可依赖此处已建立的状态。
	 * IncludeAssetBundles 使 Set 内 Fragment 的软引用并入本资产的 Bundle 数据：
	 * AssetBundles 元数据只在所属资产内被递归收集，跨资产引用不会自动传递。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Pawn", Meta = (IncludeAssetBundles = true))
	TArray<TObjectPtr<UModularPawnDataFragmentSet>> FragmentSets;

	/**
	 * 本 Pawn 独有的数据单元，构成特化层。
	 *
	 * 顺序即应用顺序，撤销时按逆序执行，使存在依赖关系的 Fragment 能正确成对。
	 * 条目标题由各 Fragment 类的 DisplayName 提供，无须 TitleProperty。
	 */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Pawn")
	TArray<TObjectPtr<UPawnDataFragment>> Fragments;

	/**
	 * 按作用域应用 Fragment。
	 *
	 * 可对同一状态数组分多次调用以覆盖不同作用域：条目按展平序列的索引对齐，互不干扰。
	 *
	 * @param bHasAuthority 为 false 时跳过要求 Authority 的 Fragment。
	 * @param OutStates 与展平序列等长并按索引对齐；未应用的位置保持为空。
	 */
	UE_API void ApplyFragments(EPawnDataFragmentScope Scope,
	                    const FPawnDataFragmentContext& Context,
	                    bool bHasAuthority,
	                    TArray<TObjectPtr<UPawnDataFragmentState>>& OutStates) const;

	/**
	 * 按作用域逆序撤销 Fragment，使存在先后依赖的 Fragment 正确成对。
	 *
	 * @param States 由 ApplyFragments 产生的状态数组；仅本作用域的条目被清空，
	 *               其余作用域的状态保留给各自的撤销调用。
	 */
	UE_API void RevokeFragments(EPawnDataFragmentScope Scope,
	                     const FPawnDataFragmentContext& Context,
	                     bool bHasAuthority,
	                     TArray<TObjectPtr<UPawnDataFragmentState>>& States) const;

	/** 是否存在指定作用域的 Fragment，用于在宿主缺失时判断是否需要告警。 */
	UE_API bool HasFragmentInScope(EPawnDataFragmentScope Scope) const;

	/** 查找首个指定类型的 Fragment，不存在时返回 nullptr。 */
	template <typename FragmentType>
	const FragmentType* FindFragment() const
	{
		static_assert(TIsDerivedFrom<FragmentType, UPawnDataFragment>::IsDerived, "FragmentType must derive from UPawnDataFragment.");

		FFragmentView View;
		BuildFragmentView(View);

		for (const UPawnDataFragment* Fragment : View)
		{
			if (const FragmentType* Typed = Cast<FragmentType>(Fragment))
			{
				return Typed;
			}
		}

		return nullptr;
	}

	/** 收集全部指定类型的 Fragment。 */
	template <typename FragmentType>
	void FindFragments(TArray<const FragmentType*>& OutFragments) const
	{
		static_assert(TIsDerivedFrom<FragmentType, UPawnDataFragment>::IsDerived, "FragmentType must derive from UPawnDataFragment.");

		FFragmentView View;
		BuildFragmentView(View);

		for (const UPawnDataFragment* Fragment : View)
		{
			if (const FragmentType* Typed = Cast<FragmentType>(Fragment))
			{
				OutFragments.Add(Typed);
			}
		}
	}

	//~Begin UObject Interface
#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	//~End UObject Interface

private:
	/** 展平序列容器。内联容量覆盖常见配置规模，避免每次遍历产生堆分配。 */
	using FFragmentView = TArray<const UPawnDataFragment*, TInlineAllocator<16>>;

	/**
	 * 按应用顺序展平全部 Fragment：先 FragmentSets（按数组顺序及各自内部顺序），后内联 Fragments。
	 *
	 * 该序列定义了 FragmentStates 的索引空间。空 Set 不占位，空 Fragment 保留占位，
	 * 使同一份资产的多次展平结果恒等，Apply 与 Revoke 因而看到一致的索引。
	 */
	UE_API void BuildFragmentView(FFragmentView& OutView) const;
};

#undef UE_API
