// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataTable.h"
#include "Messaging/CommonGameDialog.h"
#include "Messaging/CommonMessagingSubsystem.h"

#include "ModularConfirmationScreen.generated.h"

#define UE_API MODULARGAMEPLAYUI_API

class IWidgetCompilerLog;
class UCommonBorder;
class UCommonRichTextBlock;
class UCommonTextBlock;
class UDynamicEntryBox;

/**
 * 确认对话框基类：按 Descriptor 给出的按钮列表动态生成按钮，用户的选择经回调交回发起方。
 * 外观由 WidgetBlueprint 子类提供，其控件树须包含下列同名控件，否则编译期即报缺绑定。
 */
UCLASS(MinimalAPI, Abstract, BlueprintType, Blueprintable)
class UModularConfirmationScreen : public UCommonGameDialog
{
	GENERATED_BODY()

public:
	//~Begin UCommonGameDialog Interface
	UE_API virtual void SetupDialog(UCommonGameDialogDescriptor* Descriptor, FCommonMessagingResultDelegate ResultCallback) override;
	UE_API virtual void KillDialog() override;
	//~End UCommonGameDialog Interface

protected:
	//~Begin UUserWidget Interface
	UE_API virtual void NativeOnInitialized() override;
#if WITH_EDITOR
	UE_API virtual void ValidateCompiledDefaults(IWidgetCompilerLog& CompileLog) const override;
#endif
	//~End UUserWidget Interface

	// 关闭对话框并把结果交回发起方
	UE_API virtual void CloseConfirmationWindow(ECommonMessagingResult Result);

private:
	// 点击对话框之外的遮罩区域等同于否定
	UFUNCTION()
	FEventReply HandleTapToCloseZoneMouseButtonDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	FCommonMessagingResultDelegate OnResultCallback;

	UPROPERTY(Meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Title;

	UPROPERTY(Meta = (BindWidget))
	TObjectPtr<UCommonRichTextBlock> RichText_Description;

	UPROPERTY(Meta = (BindWidget))
	TObjectPtr<UDynamicEntryBox> EntryBox_Buttons;

	UPROPERTY(Meta = (BindWidget))
	TObjectPtr<UCommonBorder> Border_TapToCloseZone;

	/** 取消动作的输入绑定：确认与否定取自 CommonInput 的默认动作，取消没有默认值，须在子类逐个指定。 */
	UPROPERTY(EditDefaultsOnly, meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle CancelAction;
};

#undef UE_API
