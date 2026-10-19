// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "Components/SlateWrapperTypes.h"
#include "GameSettingFilterState.h"
#include "GameplayTagContainer.h"

#include "GameSetting.generated.h"

#define UE_API GAMESETTINGS_API

class ULocalPlayer;
class UGameSettingRegistry;

//--------------------------------------
// UGameSetting
//--------------------------------------

DECLARE_DELEGATE_RetVal_OneParam(FText, FGetGameSettingsDetails, ULocalPlayer& /*InLocalPlayer*/);

/**
 * 
 */
UCLASS(MinimalAPI, Abstract, BlueprintType)
class UGameSetting : public UObject
{
	GENERATED_BODY()

public:
	UGameSetting() { }

public:
	DECLARE_EVENT_TwoParams(UGameSetting, FOnSettingChanged, UGameSetting* /*InSetting*/, EGameSettingChangeReason /*InChangeReason*/);
	DECLARE_EVENT_OneParam(UGameSetting, FOnSettingApplied, UGameSetting* /*InSetting*/);
	DECLARE_EVENT_OneParam(UGameSetting, FOnSettingEditConditionChanged, UGameSetting* /*InSetting*/);

	FOnSettingChanged OnSettingChangedEvent;
	FOnSettingApplied OnSettingAppliedEvent;
	FOnSettingEditConditionChanged OnSettingEditConditionChangedEvent;

public:

	/**
	 * 获取该设置的非本地化开发者名称。该名称应保持恒定，
	 * 并作为该设置在本 settings registry 内的唯一标识。
	 */
	UFUNCTION(BlueprintCallable)
	FName GetDevName() const { return DevName; }
	void SetDevName(const FName& Value) { DevName = Value; }

	bool GetAdjustListViewPostRefresh() const { return bAdjustListViewPostRefresh; }
	void SetAdjustListViewPostRefresh(const bool Value) { bAdjustListViewPostRefresh = Value; }

	UFUNCTION(BlueprintCallable)
	FText GetDisplayName() const { return DisplayName; }
	void SetDisplayName(const FText& Value) { DisplayName = Value; }
#if !UE_BUILD_SHIPPING
	void SetDisplayName(const FString& Value) { SetDisplayName(FText::FromString(Value)); }
#endif
	UFUNCTION(BlueprintCallable)
	ESlateVisibility GetDisplayNameVisibility() { return DisplayNameVisibility; }
	void SetNameDisplayVisibility(ESlateVisibility InVisibility) { DisplayNameVisibility = InVisibility; }

	UFUNCTION(BlueprintCallable)
	FText GetDescriptionRichText() const { return DescriptionRichText; }
	void SetDescriptionRichText(const FText& Value) { DescriptionRichText = Value; InvalidateSearchableText(); }
#if !UE_BUILD_SHIPPING
	/** 该重载供 cheat 等无需本地化的非 shipping 功能使用；shipping 下禁用，避免引入未本地化文本。 */
	void SetDescriptionRichText(const FString& Value) { SetDescriptionRichText(FText::FromString(Value)); }
#endif

	UFUNCTION(BlueprintCallable)
	const FGameplayTagContainer& GetTags() const { return Tags; }
	void AddTag(const FGameplayTag& TagToAdd) { Tags.AddTag(TagToAdd); }

	void SetRegistry(UGameSettingRegistry* InOwningRegistry) { OwningRegistry = InOwningRegistry; }

	/** 获取描述对应的可搜索纯文本。 */
	UE_API const FString& GetDescriptionPlainText() const;

	/** 初始化设置并绑定所属 local player。容器会自动初始化添加进来的设置。 */
	UE_API void Initialize(ULocalPlayer* InLocalPlayer);

	/** 获取该设置所属的 local player；所有已初始化的设置都持有它。 */
	ULocalPlayer* GetOwningLocalPlayer() const { return LocalPlayer; }
	
	/** 设置动态详情回调，构建描述面板时查询。该文本不参与搜索。 */
	void SetDynamicDetails(const FGetGameSettingsDetails& InDynamicDetails) { DynamicDetails = InDynamicDetails; }

	/**
	 * 获取该设置的动态详情，例如账号剩余退款次数、账号编号一类的信息。
	 */
	UFUNCTION(BlueprintCallable)
	UE_API FText GetDynamicDetails() const;

	UFUNCTION(BlueprintCallable)
	FText GetWarningRichText() const { return WarningRichText; }
	void SetWarningRichText(const FText& Value) { WarningRichText = Value; InvalidateSearchableText(); }
#if !UE_BUILD_SHIPPING
	/** 该重载供 cheat 等无需本地化的非 shipping 功能使用；shipping 下禁用，避免引入未本地化文本。 */
	void SetWarningRichText(const FString& Value) { SetWarningRichText(FText::FromString(Value)); }
#endif

	/**
	 * 综合当前 edit condition 与额外的 filter state，得出该属性的编辑状态。
	 */
	const FGameSettingEditableState& GetEditState() const { return EditableStateCache; }

	/** 为该设置添加 edit condition，用于控制其可见性与可编辑性。 */
	UE_API void AddEditCondition(const TSharedRef<FGameSettingEditCondition>& InEditCondition);

	/** 添加设置依赖：被依赖的设置发生变化时，重新求值该设置的 edit condition。 */
	UE_API void AddEditDependency(UGameSetting* DependencySetting);

	/** 持有该设置的父对象，多数情况下是 collection；顶层设置则为 registry。 */
	UE_API void SetSettingParent(UGameSetting* InSettingParent);
	UGameSetting* GetSettingParent() const { return SettingParent; }

	/** 该设置是否上报 analytics。 */
	bool GetIsReportedToAnalytics() const { return bReportAnalytics; }
	void SetIsReportedToAnalytics(bool bReport) { bReportAnalytics = bReport; }

	/** 获取该设置用于 analytics 上报的值。 */
	virtual FString GetAnalyticsValue() const { return TEXT(""); }

	/**
	 * 部分设置的初始化是异步的；设置系统会等待全部设置就绪后再显示。
	 */
	bool IsReady() const { return bReady; }

	/**
	 * 任何设置都可以拥有子设置，用于支持那些对用户不直接可见、却仍需要初始值与还原值的
	 * collection 或 action。典型场景是 action 子类内部持有一批设置，
	 * 它们由其他界面修改，从不直接列在设置面板上。
	 */
	virtual TArray<UGameSetting*> GetChildSettings() { return TArray<UGameSetting*>(); }

	/**
	 * 刷新该设置的可编辑状态并广播变更，使正在展示该设置的 UI 同步到新的选项。
	 */
	UE_API void RefreshEditableState(bool bNotifyEditConditionsChanged = true);

	/**
	 * 设置通常会立即改写生效值；但少数设置（如切换分辨率）先写入临时位置，
	 * 延后才真正应用。
	 */
	UE_API void Apply();

	/** 获取持有这些设置的 local player 当前所在的 world。 */
	UE_API virtual UWorld* GetWorld() const override;

protected:
	/**  */
	UE_API virtual void Startup();
	UE_API void StartupComplete();

	UE_API virtual void OnInitialized();
	UE_API virtual void OnApply();
	UE_API virtual void OnGatherEditState(FGameSettingEditableState& InOutEditState) const;
	UE_API virtual void OnDependencyChanged();

	/**  */
	UE_API virtual FText GetDynamicDetailsInternal() const;

	/** */
	UE_API void HandleEditDependencyChanged(UGameSetting* DependencySetting, EGameSettingChangeReason Reason);
	UE_API void HandleEditDependencyChanged(UGameSetting* DependencySetting);

	/** 若可搜索纯文本已被标脏则重新生成。 */
	UE_API void RefreshPlainText() const;
	void InvalidateSearchableText() { bRefreshPlainSearchableText = true; }

	/** 广播设置已变更 */
	UE_API void NotifySettingChanged(EGameSettingChangeReason Reason);
	UE_API virtual void OnSettingChanged(EGameSettingChangeReason Reason);

	/** 广播设置的 edit condition 已变更：可能变为不可见、被禁用，或选项发生了实质变化。 */
	UE_API void NotifyEditConditionsChanged();
	UE_API virtual void OnEditConditionsChanged();

	/**  */
	UE_API FGameSettingEditableState ComputeEditableState() const;

protected:

	UPROPERTY(Transient)
	TObjectPtr<ULocalPlayer> LocalPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UGameSetting> SettingParent;

	UPROPERTY(Transient)
	TObjectPtr<UGameSettingRegistry> OwningRegistry;

	FName DevName;
	FText DisplayName;
	ESlateVisibility DisplayNameVisibility = ESlateVisibility::SelfHitTestInvisible;
	FText DescriptionRichText;
	FText WarningRichText;

	/** 设置的 tag 集合，可作为任意标记供 UI 区分行为。 */
	FGameplayTagContainer Tags;

	FGetGameSettingsDetails DynamicDetails;

	/** 该设置的全部 edit condition。 */
	TArray<TSharedRef<FGameSettingEditCondition>> EditConditions;

	class FStringCultureCache
	{
		FStringCultureCache(TFunction<FString()> InStringGetter);

		void Invalidate();

		FString Get() const;

	private:
		mutable FString StringCache;
		mutable FCultureRef Culture;
		TFunction<FString()> StringGetter;
	};

	/** 文本变化时使可搜索文本失效。 */
	mutable bool bRefreshPlainSearchableText = true;
	/** 设置富文本时自动生成对应的纯文本。 */
	mutable FString AutoGenerated_DescriptionPlainText;

	/** 是否计入 analytics 上报；默认只有 GameSettingValue 会上报。 */
	bool bReportAnalytics = false;

private:

	/** 多数设置立即就绪；少数需要等待启动完成后才能安全调用其函数。 */
	bool bReady = false;

	/** 防止广播设置变更时发生重入。 */
	bool bOnSettingChangedEventGuard = false;

	/** 防止广播 edit condition 变更时发生重入。 */
	bool bOnEditConditionsChangedEventGuard = false;

	/**  */
	bool bAdjustListViewPostRefresh = true;

	/** 缓存设置的可编辑状态，仅在其变化时更新，避免每次取用都重新计算。 */
	FGameSettingEditableState EditableStateCache;
};

#undef UE_API
