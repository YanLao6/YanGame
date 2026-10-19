// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "Widgets/IGameSettingActionInterface.h"
#include "YanHDRCalibrationEditor.generated.h"

class UCommonButtonBase;
class UGameSettingValueScalar;
class UImage;

/** 以 PQ 编码调整 HDR 峰值亮度，退出时恢复临时 UI 亮度。 */
UCLASS(Abstract)
class YANGAMESETTING_API UYanHDRCalibrationEditor : public UCommonActivatableWidget, public IGameSettingActionInterface
{
	GENERATED_BODY()

public:
	UYanHDRCalibrationEditor(const FObjectInitializer& Initializer);

	//~Begin IGameSettingActionInterface
	virtual bool ExecuteActionForSetting_Implementation(FGameplayTag ActionTag, UGameSetting* InSetting) override;
	//~End IGameSettingActionInterface

	/** 通知蓝图更新测试图案，参数为相对于 10000 nits 的亮度。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Yan|Settings")
	void OnMaxLuminanceChange(float InMaxLuminance);

protected:
	UPROPERTY(EditAnywhere, Category = "Yan|Settings")
	bool bCanCancel = true;

	//~Begin UCommonActivatableWidget Interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	//~End UCommonActivatableWidget Interface

	//~Begin UUserWidget Interface
	virtual FReply NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	//~End UUserWidget Interface

private:
	void AdjustLuminance(float Delta);
	void HandleBackClicked();
	void HandleDoneClicked();

	float StartingUILevel = 1.0f;
	float StartingUILuminance = 0.0f;
	bool bStartingUILuminanceSeparate = false;
	float MaxLuminancePQ = 0.0f;
	TWeakObjectPtr<UGameSettingValueScalar> ValueSetting;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|Settings", meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UImage> Checkerboard;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|Settings", meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UCommonButtonBase> Button_Back;

	UPROPERTY(BlueprintReadOnly, Category = "Yan|Settings", meta = (BindWidget, AllowPrivateAccess = true))
	TObjectPtr<UCommonButtonBase> Button_Done;
};
