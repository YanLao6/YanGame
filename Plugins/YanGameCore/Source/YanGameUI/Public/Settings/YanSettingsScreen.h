// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataTable.h"
#include "Widgets/GameSettingScreen.h"

#include "YanSettingsScreen.generated.h"

#define UE_API YANGAMEUI_API

class UGameSettingRegistry;
class UModularTabListWidgetBase;

/**
 * 设置界面。上方页签切换设置分类，下方 Settings_Panel 列出当前分类的设置项。
 *
 * 页签与各分类内容的对应关系配置在页签列表控件上；返回、应用、放弃三个操作
 * 经 CommonUI 的输入动作绑定，因此手柄与键鼠共用同一套交互。
 */
UCLASS(MinimalAPI, Abstract, meta = (Category = "Settings", DisableNativeTick))
class UYanSettingsScreen : public UGameSettingScreen
{
	GENERATED_BODY()

protected:
	//~Begin UUserWidget Interface
	UE_API virtual void NativeOnInitialized() override;
	//~End UUserWidget Interface

	//~Begin UGameSettingScreen Interface
	UE_API virtual UGameSettingRegistry* CreateRegistry() override;
	UE_API virtual void OnSettingsDirtyStateChanged_Implementation(bool bSettingsDirty) override;
	//~End UGameSettingScreen Interface

	void HandleBackAction();
	void HandleApplyAction();
	void HandleCancelChangesAction();

protected:
	UPROPERTY(BlueprintReadOnly, Category = Input, meta = (BindWidget, OptionalWidget = true, AllowPrivateAccess = true))
	TObjectPtr<UModularTabListWidgetBase> TopSettingsTabs;

	UPROPERTY(EditDefaultsOnly)
	FDataTableRowHandle BackInputActionData;

	UPROPERTY(EditDefaultsOnly)
	FDataTableRowHandle ApplyInputActionData;

	UPROPERTY(EditDefaultsOnly)
	FDataTableRowHandle CancelChangesInputActionData;

	FUIActionBindingHandle BackHandle;
	FUIActionBindingHandle ApplyHandle;
	FUIActionBindingHandle CancelChangesHandle;
};

#undef UE_API
