// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ModularUIMessaging.h"

#include "CommonLocalPlayer.h"
#include "Messaging/CommonGameDialog.h"
#include "NativeGameplayTags.h"
#include "PrimaryGameLayout.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularUIMessaging)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_LAYER_MODAL, "UI.Layer.Modal");

void UModularUIMessaging::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 对话框在弹出时不能等待异步加载，故在此同步取到控件类
	ConfirmationDialogClassPtr = ConfirmationDialogClass.LoadSynchronous();
	ErrorDialogClassPtr = ErrorDialogClass.LoadSynchronous();
}

void UModularUIMessaging::ShowConfirmation(UCommonGameDialogDescriptor* DialogDescriptor, FCommonMessagingResultDelegate ResultCallback)
{
	if (UCommonLocalPlayer* LocalPlayer = GetLocalPlayer<UCommonLocalPlayer>())
	{
		if (UPrimaryGameLayout* RootLayout = LocalPlayer->GetRootUILayout())
		{
			RootLayout->PushWidgetToLayerStack<UCommonGameDialog>(TAG_UI_LAYER_MODAL, ConfirmationDialogClassPtr, [DialogDescriptor, ResultCallback](UCommonGameDialog& Dialog)
			{
				Dialog.SetupDialog(DialogDescriptor, ResultCallback);
			});
		}
	}
}

void UModularUIMessaging::ShowError(UCommonGameDialogDescriptor* DialogDescriptor, FCommonMessagingResultDelegate ResultCallback)
{
	if (UCommonLocalPlayer* LocalPlayer = GetLocalPlayer<UCommonLocalPlayer>())
	{
		if (UPrimaryGameLayout* RootLayout = LocalPlayer->GetRootUILayout())
		{
			RootLayout->PushWidgetToLayerStack<UCommonGameDialog>(TAG_UI_LAYER_MODAL, ErrorDialogClassPtr, [DialogDescriptor, ResultCallback](UCommonGameDialog& Dialog)
			{
				Dialog.SetupDialog(DialogDescriptor, ResultCallback);
			});
		}
	}
}
