// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ModularConfirmationScreen.h"

#include "CommonBorder.h"
#include "CommonInputSettings.h"
#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"
#include "Components/DynamicEntryBox.h"
#include "ICommonInputModule.h"
#include "UI/ModularButtonBase.h"

#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularConfirmationScreen)

void UModularConfirmationScreen::SetupDialog(UCommonGameDialogDescriptor* Descriptor, FCommonMessagingResultDelegate ResultCallback)
{
	Super::SetupDialog(Descriptor, ResultCallback);

	Text_Title->SetText(Descriptor->Header);
	RichText_Description->SetText(Descriptor->Body);

	EntryBox_Buttons->Reset<UModularButtonBase>([](UModularButtonBase& Button)
	{
		Button.OnClicked().Clear();
	});

	for (const FConfirmationDialogAction& Action : Descriptor->ButtonActions)
	{
		FDataTableRowHandle ActionRow;

		switch (Action.Result)
		{
			case ECommonMessagingResult::Confirmed:
				ActionRow = ICommonInputModule::GetSettings().GetDefaultClickAction();
				break;

			case ECommonMessagingResult::Declined:
				ActionRow = ICommonInputModule::GetSettings().GetDefaultBackAction();
				break;

			case ECommonMessagingResult::Cancelled:
				ActionRow = CancelAction;
				break;

			default:
				ensure(false);
				continue;
		}

		UModularButtonBase* Button = EntryBox_Buttons->CreateEntry<UModularButtonBase>();
		Button->SetTriggeringInputAction(ActionRow);
		Button->OnClicked().AddUObject(this, &ThisClass::CloseConfirmationWindow, Action.Result);
		Button->SetButtonText(Action.OptionalDisplayText);
	}

	OnResultCallback = ResultCallback;
}

void UModularConfirmationScreen::KillDialog()
{
	Super::KillDialog();
}

void UModularConfirmationScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Border_TapToCloseZone->OnMouseButtonDownEvent.BindDynamic(this, &UModularConfirmationScreen::HandleTapToCloseZoneMouseButtonDown);
}

void UModularConfirmationScreen::CloseConfirmationWindow(ECommonMessagingResult Result)
{
	DeactivateWidget();
	OnResultCallback.ExecuteIfBound(Result);
}

FEventReply UModularConfirmationScreen::HandleTapToCloseZoneMouseButtonDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	FEventReply Reply;
	Reply.NativeReply = FReply::Unhandled();

	if (MouseEvent.IsTouchEvent() || MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		CloseConfirmationWindow(ECommonMessagingResult::Declined);
		Reply.NativeReply = FReply::Handled();
	}

	return Reply;
}

#if WITH_EDITOR
void UModularConfirmationScreen::ValidateCompiledDefaults(IWidgetCompilerLog& CompileLog) const
{
	if (CancelAction.IsNull())
	{
		CompileLog.Error(FText::Format(FText::FromString(TEXT("{0} has unset property: CancelAction.")), FText::FromString(GetName())));
	}
}
#endif
