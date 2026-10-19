// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/GameSettingListEntry.h"

#include "YanSettingsListEntrySetting_KeyboardInput.generated.h"

#define UE_API YANGAMEUI_API

class UGameSetting;
class UGameSettingPressAnyKey;
class UKeyAlreadyBoundWarning;
class UModularButtonBase;
class UYanSettingKeyboardInput;

/**
 * 按键绑定条目。主副两个槽位各自可改键，改键流程经模态层的"按下任意键"面板完成；
 * 目标按键已被占用时先弹出确认，玩家再次按下同一按键才真正改绑。
 */
UCLASS(MinimalAPI, Abstract, Blueprintable, meta = (Category = "Settings", DisableNativeTick))
class UYanSettingsListEntrySetting_KeyboardInput : public UGameSettingListEntry_Setting
{
	GENERATED_BODY()

public:
	//~Begin UGameSettingListEntryBase Interface
	UE_API virtual void SetSetting(UGameSetting* InSetting) override;
	//~End UGameSettingListEntryBase Interface

protected:
	//~Begin UUserWidget Interface
	UE_API virtual void NativeOnInitialized() override;
	//~End UUserWidget Interface

	//~Begin UGameSettingListEntryBase Interface
	UE_API virtual void NativeOnEntryReleased() override;
	UE_API virtual void OnSettingChanged() override;
	//~End UGameSettingListEntryBase Interface

	void HandlePrimaryKeyClicked();
	void HandleSecondaryKeyClicked();
	void HandleClearClicked();
	void HandleResetToDefaultClicked();

	void HandlePrimaryKeySelected(FKey InKey, UGameSettingPressAnyKey* PressAnyKeyPanel);
	void HandleSecondaryKeySelected(FKey InKey, UGameSettingPressAnyKey* PressAnyKeyPanel);
	void HandlePrimaryDuplicateKeySelected(FKey InKey, UKeyAlreadyBoundWarning* DuplicateKeyPressAnyKeyPanel) const;
	void HandleSecondaryDuplicateKeySelected(FKey InKey, UKeyAlreadyBoundWarning* DuplicateKeyPressAnyKeyPanel) const;
	void ChangeBinding(int32 InKeyBindSlot, FKey InKey);
	void HandleKeySelectionCanceled(UGameSettingPressAnyKey* PressAnyKeyPanel);
	void HandleKeySelectionCanceled(UKeyAlreadyBoundWarning* PressAnyKeyPanel);

	void Refresh();

private:
	// 冲突确认期间暂存待绑定的按键，玩家确认后才写入
	UPROPERTY(Transient)
	FKey OriginalKeyToBind = EKeys::Invalid;

protected:
	UPROPERTY()
	TObjectPtr<UYanSettingKeyboardInput> KeyboardInputSetting;

	/** "按下任意键"面板，需在蓝图中指定 */
	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	TSubclassOf<UGameSettingPressAnyKey> PressAnyKeyPanelClass;

	/** 按键冲突提示面板，需在蓝图中指定 */
	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	TSubclassOf<UKeyAlreadyBoundWarning> KeyAlreadyBoundWarningPanelClass;

private:	// Bound Widgets
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, BlueprintProtected = true, AllowPrivateAccess = true))
	TObjectPtr<UModularButtonBase> Button_PrimaryKey;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, BlueprintProtected = true, AllowPrivateAccess = true))
	TObjectPtr<UModularButtonBase> Button_SecondaryKey;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, BlueprintProtected = true, AllowPrivateAccess = true))
	TObjectPtr<UModularButtonBase> Button_Clear;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, BlueprintProtected = true, AllowPrivateAccess = true))
	TObjectPtr<UModularButtonBase> Button_ResetToDefault;
};

#undef UE_API
