// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "Widgets/IGameSettingActionInterface.h"
#include "YanSafeZoneEditor.generated.h"

enum class ECommonInputType : uint8;
class UCommonButtonBase;
class UCommonRichTextBlock;
class UGameSettingValueScalar;
class UWidgetSwitcher;

/** 预览并确认本机界面安全区，取消时恢复进入界面前的比例。 */
UCLASS(Abstract)
class YANGAMESETTING_API UYanSafeZoneEditor : public UCommonActivatableWidget, public IGameSettingActionInterface
{
	GENERATED_BODY()

public:
	UYanSafeZoneEditor(const FObjectInitializer& Initializer);
	FSimpleMulticastDelegate OnSafeZoneSet;

	//~Begin IGameSettingActionInterface
	virtual bool ExecuteActionForSetting_Implementation(FGameplayTag ActionTag, UGameSetting* InSetting) override;
	//~End IGameSettingActionInterface

protected:
	UPROPERTY(EditAnywhere, Category = "Yan|Settings")
	bool bCanCancel = true;

	//~Begin UCommonActivatableWidget Interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	//~End UCommonActivatableWidget Interface

	//~Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	//~End UUserWidget Interface

private:
	void HandleInputModeChanged(ECommonInputType InInputType);
	void AdjustSafeZone(float Delta);
	void HandleBackClicked();
	void HandleDoneClicked();

	float StartingSafeZone = 0.0f;
	bool bConfirmed = false;
	TWeakObjectPtr<UGameSettingValueScalar> ValueSetting;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|Settings", meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UWidgetSwitcher> Switcher_SafeZoneMessage;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|Settings", meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UCommonRichTextBlock> RichText_Default;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|Settings", meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UCommonButtonBase> Button_Back;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|Settings", meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UCommonButtonBase> Button_Done;
};
