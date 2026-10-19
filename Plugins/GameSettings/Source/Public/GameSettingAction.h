// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameSetting.h"

#include "GameSettingAction.generated.h"

#define UE_API GAMESETTINGS_API

//--------------------------------------
// UGameSettingAction
//--------------------------------------

class ULocalPlayer;

DECLARE_DELEGATE_TwoParams(UGameSettingCustomAction, UGameSetting* /*Setting*/, ULocalPlayer* /*LocalPlayer*/)

/**
 * 
 */
UCLASS(MinimalAPI)
class UGameSettingAction : public UGameSetting
{
	GENERATED_BODY()

public:
	UE_API UGameSettingAction();

public:

	DECLARE_EVENT_TwoParams(UGameSettingAction, FOnExecuteNamedAction, UGameSetting* /*Setting*/, FGameplayTag /*GameSettings_Action_Tag*/);
	FOnExecuteNamedAction OnExecuteNamedActionEvent;

public:

	FText GetActionText() const { return ActionText; }
	void SetActionText(FText Value) { ActionText = Value; }
#if !UE_BUILD_SHIPPING
	void SetActionText(const FString& Value) { SetActionText(FText::FromString(Value)); }
#endif

	FGameplayTag GetNamedAction() const { return NamedAction; }
	void SetNamedAction(FGameplayTag Value) { NamedAction = Value; }

	bool HasCustomAction() const { return CustomAction.IsBound(); }
	void SetCustomAction(UGameSettingCustomAction InAction) { CustomAction = InAction; }
	UE_API void SetCustomAction(TFunction<void(ULocalPlayer*)> InAction);

	/**
	 * action 默认不会把设置标脏，因为多数 action 要么执行不可撤销的操作，
	 * 要么只是显示制作人员名单或 Eula 之类的内容。若并非如此，
	 * 将该标志置为 true，action 触发时便会一并触发变更事件。
	 */
	void SetDoesActionDirtySettings(bool Value) { bDirtyAction = Value; }

	UE_API virtual void ExecuteAction();

protected:
	/** UGameSettingValue */
	UE_API virtual void OnInitialized() override;

protected:
	FText ActionText;
	FGameplayTag NamedAction;
	UGameSettingCustomAction CustomAction;
	bool bDirtyAction = false;
};

#undef UE_API
