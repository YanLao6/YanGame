// Copyright Chronicler.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "ModularWidgetController.generated.h"

class AModularPlayerController;
class AModularPlayerState;
class UModularAbilitySystemComponent;
class UAttributeSet;


USTRUCT(BlueprintType)
struct FWidgetControllerParams
{
	GENERATED_BODY()
	
	/** 默认构造。 */
	FWidgetControllerParams() {}
	/** 使用完整上下文构造参数。 */
	FWidgetControllerParams(AModularPlayerController* PC,
		AModularPlayerState* PS,
		UModularAbilitySystemComponent* ASC,
		const UAttributeSet* AS)
	: PlayerController(PC), PlayerState(PS), AbilitySystemComponent(ASC), AttributeSet(AS) {};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AModularPlayerController> PlayerController = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AModularPlayerState> PlayerState = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UModularAbilitySystemComponent> AbilitySystemComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<const UAttributeSet> AttributeSet = nullptr;
};
/**
 * WidgetController 基类。
 *
 * 通过 `FWidgetControllerParams` 连接 Player/ASC/AttributeSet，
 * 供具体 UI 控制器执行初值广播与回调绑定。
 */
UCLASS()
class MODULARGAMEPLAYUI_API UModularWidgetController : public UObject
{
	GENERATED_BODY()

public:
	/** 设置控制器上下文参数。 */
	UFUNCTION(BlueprintCallable)
	virtual void SetWidgetControllerParams(const FWidgetControllerParams& WidgetControllerParams);

	/** 广播 UI 初始值。 */
	virtual void BroadcastInitialValues();
	/** 绑定依赖系统回调。 */
	virtual void BindCallbacksToDependancies();

	protected:

	UPROPERTY(BlueprintReadOnly, Category="WidgetController")
	TObjectPtr<AModularPlayerController> PlayerController;

	UPROPERTY(BlueprintReadOnly, Category="WidgetController")
	TObjectPtr<AModularPlayerState> PlayerState;

	UPROPERTY(BlueprintReadOnly, Category="WidgetController")
	TObjectPtr<UModularAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(BlueprintReadOnly, Category="WidgetController")
	TObjectPtr<const UAttributeSet> AttributeSet;
};
