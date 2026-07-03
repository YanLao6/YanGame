// Copyright Chronicler.


#include "GameMode/ModularExperienceDefinition.h"
#include "GameFeatureAction.h"
#if WITH_EDITOR
	#include "Misc/DataValidation.h"
#endif
#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularExperienceDefinition)

#define LOCTEXT_NAMESPACE "ModularSystem"

UModularExperienceDefinition::UModularExperienceDefinition()
{
}

#if WITH_EDITOR
EDataValidationResult UModularExperienceDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	int32 EntryIndex = 0;
	for (const UGameFeatureAction* Action : Actions)
	{
		if (Action)
		{
			const EDataValidationResult ChildResult = Action->IsDataValid(Context);
			Result = CombineDataValidationResults(Result, ChildResult);
		}
		else
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("ActionEntryIsNull", "Null entry at index {0} in Actions"), FText::AsNumber(EntryIndex)));
		}

		++EntryIndex;
	}

	// 禁止「Blueprint 的 Blueprint 子类」链：允许 C++ -> BP 一次，不允许 BP 再派生 BP（请用 ActionSet 组合）。
	if (!GetClass()->IsNative())
	{
		const UClass* ParentClass = GetClass()->GetSuperClass();

		// 找到最近的 Native 父类
		const UClass* FirstNativeParent = ParentClass;
		while ((FirstNativeParent != nullptr) && !FirstNativeParent->IsNative())
		{
			FirstNativeParent = FirstNativeParent->GetSuperClass();
		}

		if (FirstNativeParent != ParentClass)
		{
			Context.AddError(FText::Format(LOCTEXT("ExperienceInheritenceIsUnsupported", "Blueprint subclasses of Blueprint experiences is not currently supported (use composition via ActionSets instead). Parent class was {0} but should be {1}."), 
				FText::AsCultureInvariant(GetPathNameSafe(ParentClass)),
				FText::AsCultureInvariant(GetPathNameSafe(FirstNativeParent))
			));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif

#if WITH_EDITORONLY_DATA
void UModularExperienceDefinition::UpdateAssetBundleData()
{
	Super::UpdateAssetBundleData();

	// 将 Experience 自身 Actions 中声明的软引用注册到 AssetBundleData。
	// DefaultPawnData 的 Bundle 注册由 UPROPERTY Meta=(AssetBundles="Equipped") 自动完成。
	// ActionSet 已在 StartExperienceLoad 中单独加入 BundleAssetList，无需在此递归处理。
	for (UGameFeatureAction* Action : Actions)
	{
		if (Action)
		{
			Action->AddAdditionalAssetBundleData(AssetBundleData);
		}
	}
}
#endif

#undef LOCTEXT_NAMESPACE
