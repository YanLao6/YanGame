// Copyright Chronicler.

#pragma once

#include "CoreMinimal.h"
#include "ModularInputActivatableWidget.h"
#include "ModularUserWidget.generated.h"

#define UE_API MODULARGAMEPLAYUI_API

/**
 * 带 WidgetController 的通用 UserWidget 基类。
 * @see UModularWidgetController
 */
UCLASS(MinimalAPI)
class UModularUserWidget : public UModularInputActivatableWidget
{
	GENERATED_BODY()

public:
	/** 设置 WidgetController 并触发 `WidgetControllerSet`。 */
	UFUNCTION(BlueprintCallable)
	UE_API void SetWidgetController(UObject* NewWidgetController);
	
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UObject> WidgetController;

protected:

	/** Blueprint 回调：当 WidgetController 设置完成后触发。 */
	UFUNCTION(BlueprintImplementableEvent)
	UE_API void WidgetControllerSet();
};

#undef UE_API
