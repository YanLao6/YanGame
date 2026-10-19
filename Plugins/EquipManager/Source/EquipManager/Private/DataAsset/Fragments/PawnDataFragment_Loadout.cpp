// 英雄专属开局配装 Fragment 实现。

#include "DataAsset/Fragments/PawnDataFragment_Loadout.h"

#include "GameFeaturesSubsystem.h"
#include "GameFramework/Controller.h"
#include "Loadout/InGameLoadoutComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PawnDataFragment_Loadout)

#define LOCTEXT_NAMESPACE "PawnDataFragment_Loadout"

UPawnDataFragmentState* UPawnDataFragment_Loadout::Apply(const FPawnDataFragmentContext& Context) const
{
	AController* Controller = Context.Controller;
	if ((Controller == nullptr) || LoadoutDefinition.IsNull())
	{
		return nullptr;
	}

	UInGameLoadoutComponent* LoadoutComponent = Controller->FindComponentByClass<UInGameLoadoutComponent>();
	if (LoadoutComponent == nullptr)
	{
		// 发放组件由 Experience 侧注入，缺它说明本局没有配置开局配装这条链路。
		UE_LOG(LogGameFeatures, Warning, TEXT("Loadout Fragment：[%s] 上没有发放组件，英雄专属配装不会生效。"), *Controller->GetPathName());
		return nullptr;
	}

	LoadoutComponent->AddLoadoutDefinition(LoadoutDefinition);

	// 记账对象须比写入目标活得久：配装加在控制器的组件上，以 Pawn 为 Outer 会随其死亡消失。
	return NewObject<UPawnDataFragmentState_Loadout>(Controller);
}

void UPawnDataFragment_Loadout::Revoke(const FPawnDataFragmentContext& Context, UPawnDataFragmentState* State) const
{
	AController* Controller = Context.Controller;
	if (Controller == nullptr)
	{
		return;
	}

	if (UInGameLoadoutComponent* LoadoutComponent = Controller->FindComponentByClass<UInGameLoadoutComponent>())
	{
		LoadoutComponent->RemoveLoadoutDefinition(LoadoutDefinition);
	}
}

#if WITH_EDITOR
EDataValidationResult UPawnDataFragment_Loadout::IsFragmentValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsFragmentValid(Context);

	if (LoadoutDefinition.IsNull())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(LOCTEXT("NoLoadout", "Loadout Fragment 未指定配装资产，不会产生任何效果。"));
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
