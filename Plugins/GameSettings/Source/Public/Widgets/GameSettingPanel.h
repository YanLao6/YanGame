// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserWidget.h"
#include "Containers/Ticker.h"
#include "GameSettingFilterState.h"
#include "GameplayTagContainer.h"
#include "Misc/ExpressionParserTypesFwd.h"

#include "GameSettingPanel.generated.h"

#define UE_API GAMESETTINGS_API

class UGameSetting;
class UGameSettingDetailView;
class UGameSettingListView;
class UGameSettingRegistry;
class UObject;
struct FFocusEvent;
struct FGeometry;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnFocusedSettingChanged, UGameSetting*)

UCLASS(MinimalAPI, Abstract)
class UGameSettingPanel : public UCommonUserWidget
{
	GENERATED_BODY()

public:

	UE_API UGameSettingPanel();
	UE_API virtual void NativeOnInitialized() override;
	UE_API virtual void NativeConstruct() override;
	UE_API virtual void NativeDestruct() override;

	// 手柄模式下把焦点转移到子控件
	UE_API virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;

	/**  */
	UE_API void SetRegistry(UGameSettingRegistry* InRegistry);

	/** 设置该面板的过滤条件，限定当前可用的设置项。 */
	UE_API void SetFilterState(const FGameSettingFilterState& InFilterState, bool bClearNavigationStack = true);

	/** 依据 filter state 获取当前可见且可用的设置项。 */
	TArray<UGameSetting*> GetVisibleSettings() const { return VisibleSettings; }

	/** 当前导航栈是否可以弹出 */
	UE_API bool CanPopNavigationStack() const;

	/** 弹出导航栈 */
	UE_API void PopNavigationStack();

	/**
	 * 获取该界面上潜在可用的设置集合。
	 * 可能包含不可见的设置。
	 * 不包含嵌套子页面。
	 */
	UE_API TArray<UGameSetting*> GetSettingsWeCanResetToDefault() const;

	UE_API void SelectSetting(const FName& SettingDevName);
	UE_API UGameSetting* GetSelectedSetting() const;

	UE_API void RefreshSettingsList();

	FOnFocusedSettingChanged OnFocusedSettingChanged;

protected:
	UE_API void RegisterRegistryEvents();
	UE_API void UnregisterRegistryEvents();
	
	UE_API void HandleSettingItemHoveredChanged(UObject* Item, bool bHovered);
	UE_API void HandleSettingItemSelectionChanged(UObject* Item);
	UE_API void FillSettingDetails(UGameSetting* InSetting);
	UE_API void HandleSettingNamedAction(UGameSetting* Setting, FGameplayTag GameSettings_Action_Tag);
	UE_API void HandleSettingNavigation(UGameSetting* Setting);
	UE_API void HandleSettingEditConditionsChanged(UGameSetting* Setting);

private:

	UPROPERTY(Transient)
	TObjectPtr<UGameSettingRegistry> Registry;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameSetting>> VisibleSettings;

	UPROPERTY(Transient)
	TObjectPtr<UGameSetting> LastHoveredOrSelectedSetting;

	UPROPERTY(Transient)
	FGameSettingFilterState FilterState;

	UPROPERTY(Transient)
	TArray<FGameSettingFilterState> FilterNavigationStack;

	FName DesiredSelectionPostRefresh;

	bool bAdjustListViewPostRefresh = true;

private:	// 绑定的控件
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, BlueprintProtected = true, AllowPrivateAccess = true))
	TObjectPtr<UGameSettingListView> ListView_Settings;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, BlueprintProtected = true, AllowPrivateAccess = true))
	TObjectPtr<UGameSettingDetailView> Details_Settings;

private:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnExecuteNamedActionBP, UGameSetting*, Setting, FGameplayTag, Action);
	UPROPERTY(BlueprintAssignable, Category = Events, meta = (DisplayName = "On Execute Named Action"))
	FOnExecuteNamedActionBP BP_OnExecuteNamedAction;

private:
	FTSTicker::FDelegateHandle RefreshHandle;
};

#undef UE_API
