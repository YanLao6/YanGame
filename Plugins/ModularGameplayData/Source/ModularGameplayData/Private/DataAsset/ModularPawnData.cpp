// Copyright Chronicler.

#include "DataAsset/ModularPawnData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularPawnData)

#define LOCTEXT_NAMESPACE "ModularPawnData"

UModularPawnData::UModularPawnData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PawnClass = nullptr;
}

void UModularPawnData::BuildFragmentView(FFragmentView& OutView) const
{
	for (const TObjectPtr<UModularPawnDataFragmentSet>& FragmentSet : FragmentSets)
	{
		if (!FragmentSet)
		{
			continue;
		}

		for (const TObjectPtr<UPawnDataFragment>& Fragment : FragmentSet->Fragments)
		{
			OutView.Add(Fragment);
		}
	}

	for (const TObjectPtr<UPawnDataFragment>& Fragment : Fragments)
	{
		OutView.Add(Fragment);
	}
}

bool UModularPawnData::HasFragmentInScope(EPawnDataFragmentScope Scope) const
{
	FFragmentView View;
	BuildFragmentView(View);

	for (const UPawnDataFragment* Fragment : View)
	{
		if (Fragment && Fragment->GetScope() == Scope)
		{
			return true;
		}
	}

	return false;
}

void UModularPawnData::ApplyFragments(EPawnDataFragmentScope Scope,
                                      const FPawnDataFragmentContext& Context,
                                      bool bHasAuthority,
                                      TArray<TObjectPtr<UPawnDataFragmentState>>& OutStates) const
{
	FFragmentView View;
	BuildFragmentView(View);

	// 同一宿主可能分多次应用不同作用域，已有条目必须保留。
	if (OutStates.Num() != View.Num())
	{
		OutStates.SetNum(View.Num());
	}

	for (int32 Index = 0; Index < View.Num(); ++Index)
	{
		const UPawnDataFragment* Fragment = View[Index];
		if (!Fragment || Fragment->GetScope() != Scope)
		{
			continue;
		}

		if (Fragment->RequiresAuthority() && !bHasAuthority)
		{
			continue;
		}

		OutStates[Index] = Fragment->Apply(Context);
	}
}

void UModularPawnData::RevokeFragments(EPawnDataFragmentScope Scope,
                                       const FPawnDataFragmentContext& Context,
                                       bool bHasAuthority,
                                       TArray<TObjectPtr<UPawnDataFragmentState>>& States) const
{
	FFragmentView View;
	BuildFragmentView(View);

	for (int32 Index = View.Num() - 1; Index >= 0; --Index)
	{
		const UPawnDataFragment* Fragment = View[Index];
		if (!Fragment || Fragment->GetScope() != Scope)
		{
			continue;
		}

		if (Fragment->RequiresAuthority() && !bHasAuthority)
		{
			continue;
		}

		if (!States.IsValidIndex(Index))
		{
			Fragment->Revoke(Context, nullptr);
			continue;
		}

		Fragment->Revoke(Context, States[Index]);

		// 只清本作用域的条目，其余作用域的状态由各自的撤销调用负责。
		States[Index] = nullptr;
	}
}

#if WITH_EDITOR
EDataValidationResult UModularPawnData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	int32 SetIndex = 0;
	for (const TObjectPtr<UModularPawnDataFragmentSet>& FragmentSet : FragmentSets)
	{
		if (!FragmentSet)
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("NullFragmentSet", "FragmentSets[{0}] 为空。"), FText::AsNumber(SetIndex)));
		}

		++SetIndex;
	}

	// Set 内 Fragment 的自校验由 Set 资产负责，此处只校验本资产直接持有的条目。
	int32 FragmentIndex = 0;
	for (const TObjectPtr<UPawnDataFragment>& Fragment : Fragments)
	{
		if (!Fragment)
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("NullFragment", "Fragments[{0}] 为空。"), FText::AsNumber(FragmentIndex)));
		}
		else
		{
			Result = CombineDataValidationResults(Result, Fragment->IsFragmentValid(Context));
		}

		++FragmentIndex;
	}

	// 单例语义的 Fragment 跨层重复只有在展平后才可见，故在此汇总校验。
	FFragmentView View;
	BuildFragmentView(View);

	TSet<const UClass*> ReportedClasses;
	for (int32 OuterIndex = 0; OuterIndex < View.Num(); ++OuterIndex)
	{
		const UPawnDataFragment* Fragment = View[OuterIndex];
		if (!Fragment || Fragment->AllowsMultipleInstances())
		{
			continue;
		}

		const UClass* FragmentClass = Fragment->GetClass();
		if (ReportedClasses.Contains(FragmentClass))
		{
			continue;
		}

		int32 InstanceCount = 0;
		for (const UPawnDataFragment* Other : View)
		{
			if (Other && Other->GetClass() == FragmentClass)
			{
				++InstanceCount;
			}
		}

		if (InstanceCount > 1)
		{
			ReportedClasses.Add(FragmentClass);
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				LOCTEXT("DuplicateSingletonFragment",
				        "展平后存在 {0} 个 [{1}]，该类型只允许一个。需要覆盖基础配置时，请将其从 FragmentSet 中移出。"),
				FText::AsNumber(InstanceCount),
				FText::FromString(FragmentClass->GetName())));
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
