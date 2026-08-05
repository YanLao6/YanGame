// Copyright Chronicler.


#include "UI/ModularUserWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularUserWidget)

void UModularUserWidget::SetWidgetController(UObject* NewWidgetController)
{
	WidgetController = NewWidgetController;
	WidgetControllerSet();
}
