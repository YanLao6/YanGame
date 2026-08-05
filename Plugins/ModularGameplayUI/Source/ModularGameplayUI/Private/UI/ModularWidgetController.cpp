// Copyright Chronicler.


#include "UI/ModularWidgetController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularWidgetController)

void UModularWidgetController::SetWidgetControllerParams(const FWidgetControllerParams& WidgetControllerParams)
{
		PlayerController = WidgetControllerParams.PlayerController;
		PlayerState = WidgetControllerParams.PlayerState;
		AbilitySystemComponent = WidgetControllerParams.AbilitySystemComponent;
		AttributeSet = WidgetControllerParams.AttributeSet;
}

void UModularWidgetController::BroadcastInitialValues()
{
	// 子类填充：向 UI 广播初始 Attribute/状态等。
}

void UModularWidgetController::BindCallbacksToDependancies()
{
	// 子类填充：绑定 ASC 等依赖对象的 Delegate/回调。
}
