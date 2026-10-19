// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Messaging/CommonMessagingSubsystem.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"

#include "ModularUIMessaging.generated.h"

#define UE_API MODULARGAMEPLAYUI_API

class FSubsystemCollectionBase;
class UCommonGameDialog;
class UCommonGameDialogDescriptor;

/**
 * 对话框的呈现方：把确认框与错误框推入本地玩家根布局的 Modal 层。
 * 基类的同名接口是空实现，缺少本子系统时对话框请求会被静默丢弃。
 * 两个控件类经配置指定，见 DefaultGame.ini 中的同名段。
 */
UCLASS(MinimalAPI)
class UModularUIMessaging : public UCommonMessagingSubsystem
{
	GENERATED_BODY()

public:
	//~Begin USubsystem Interface
	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	//~End USubsystem Interface

	//~Begin UCommonMessagingSubsystem Interface
	UE_API virtual void ShowConfirmation(UCommonGameDialogDescriptor* DialogDescriptor, FCommonMessagingResultDelegate ResultCallback = FCommonMessagingResultDelegate()) override;
	UE_API virtual void ShowError(UCommonGameDialogDescriptor* DialogDescriptor, FCommonMessagingResultDelegate ResultCallback = FCommonMessagingResultDelegate()) override;
	//~End UCommonMessagingSubsystem Interface

private:
	UPROPERTY()
	TSubclassOf<UCommonGameDialog> ConfirmationDialogClassPtr;

	UPROPERTY()
	TSubclassOf<UCommonGameDialog> ErrorDialogClassPtr;

	UPROPERTY(config)
	TSoftClassPtr<UCommonGameDialog> ConfirmationDialogClass;

	UPROPERTY(config)
	TSoftClassPtr<UCommonGameDialog> ErrorDialogClass;
};

#undef UE_API
