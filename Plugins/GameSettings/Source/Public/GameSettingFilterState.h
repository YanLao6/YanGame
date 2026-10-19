// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/TextFilterExpressionEvaluator.h"

#include "UObject/ObjectPtr.h"
#include "GameSettingFilterState.generated.h"

#define UE_API GAMESETTINGS_API

class ULocalPlayer;
class UGameSetting;
class UGameSettingCollection;

/** 设置因何变更？ */
enum class EGameSettingChangeReason : uint8
{
	Change,
	DependencyChanged,
	ResetToDefault,
	RestoreToInitial,
};

/**
 * filter state 承载所支持的全部过滤条件。
 */
USTRUCT()
struct FGameSettingFilterState
{
	GENERATED_BODY()

public:

	UE_API FGameSettingFilterState();

	UPROPERTY()
	bool bIncludeDisabled = true;

	UPROPERTY()
	bool bIncludeHidden = false;

	UPROPERTY()
	bool bIncludeResetable = true;

	UPROPERTY()
	bool bIncludeNestedPages = false;

public:
	UE_API void SetSearchText(const FString& InSearchText);

	UE_API bool DoesSettingPassFilter(const UGameSetting& InSetting) const;

	UE_API void AddSettingToRootList(UGameSetting* InSetting);
	UE_API void AddSettingToAllowList(UGameSetting* InSetting);

	bool IsSettingInAllowList(const UGameSetting* InSetting) const
	{
		return SettingAllowList.Contains(InSetting);
	}
	
	const TArray<UGameSetting*>& GetSettingRootList() const { return SettingRootList; }
	bool IsSettingInRootList(const UGameSetting* InSetting) const
	{
		return SettingRootList.Contains(InSetting);
	}

private:
	FTextFilterExpressionEvaluator SearchTextEvaluator;

	UPROPERTY()
	TArray<TObjectPtr<UGameSetting>> SettingRootList;

	// 非空时，仅允许其中列出的设置
	UPROPERTY()
	TArray<TObjectPtr<UGameSetting>> SettingAllowList;
};

/**
 * 记录设置当前的可见性与启用状态，以及形成该状态的原因。
 */
class FGameSettingEditableState
{
public:
	FGameSettingEditableState()
		: bVisible(true)
		, bEnabled(true)
		, bResetable(true)
		, bHideFromAnalytics(false)
	{
	}

	bool IsVisible() const { return bVisible; }
	bool IsEnabled() const { return bEnabled; }
	bool IsResetable() const { return bResetable; }
	bool IsHiddenFromAnalytics() const { return bHideFromAnalytics; }
	const TArray<FText>& GetDisabledReasons() const { return DisabledReasons; }

#if !UE_BUILD_SHIPPING
	const TArray<FString>& GetHiddenReasons() const { return HiddenReasons; }
#endif

	const TArray<FString>& GetDisabledOptions() const { return DisabledOptions; }

	/** 隐藏该设置。面向用户的原因可以不填，但必须给出面向开发者的原因。 */
	UE_API void Hide(const FString& DevReason);

	/** 禁用该设置，必须给出禁用原因。 */
	UE_API void Disable(const FText& Reason);

	/** 需要对用户隐藏的离散选项。目前仅家长控制（Parental Controls）使用。 */
	UE_API void DisableOption(const FString& Option);

	template<typename EnumType>
	void DisableEnumOption(EnumType InEnumValue)
	{
		DisableOption(StaticEnum<EnumType>()->GetNameStringByValue((int64)InEnumValue));
	}

	/**
	 * 当用户把界面上的设置重置为默认值时，阻止该设置被重置。
	 */
	UE_API void UnableToReset();

	/**
	 * 对 analytics 隐藏。用于降低噪声，例如平台相关的 edit condition：
	 * 为并不存在该设置的平台上报数据没有意义。
	 */
	void HideFromAnalytics() { bHideFromAnalytics = true; }

	/** 以一切方式隐藏：视觉上隐藏、标记为不可重置、并对 analytics 隐藏。 */
	void Kill(const FString& DevReason)
	{
		Hide(DevReason);
		HideFromAnalytics();
		UnableToReset();
	}

private:
	uint8 bVisible : 1;
	uint8 bEnabled : 1;
	uint8 bResetable : 1;
	uint8 bHideFromAnalytics : 1;

	TArray<FString> DisabledOptions;

	TArray<FText> DisabledReasons;

#if !UE_BUILD_SHIPPING
	TArray<FString> HiddenReasons;
#endif
};

/**
 * edit condition 可监视游戏状态或其他设置的状态，并据此调整可见性。
 */
class FGameSettingEditCondition : public TSharedFromThis<FGameSettingEditCondition>
{
public:
	FGameSettingEditCondition() { }
	virtual ~FGameSettingEditCondition() { }

	DECLARE_EVENT_OneParam(FGameSettingEditCondition, FOnEditConditionChanged, bool);
	FOnEditConditionChanged OnEditConditionChangedEvent;

	/** 广播事件 */
	void BroadcastEditConditionChanged()
	{
		OnEditConditionChangedEvent.Broadcast(true);
	}

	/** 在设置初始化期间调用 */
	virtual void Initialize(const ULocalPlayer* InLocalPlayer)
	{
	}

	/** 设置被 apply 时调用。 */
	virtual void SettingApplied(const ULocalPlayer* InLocalPlayer, UGameSetting* Setting) const
	{
	}

	/** 设置发生变更时调用。 */
	virtual void SettingChanged(const ULocalPlayer* InLocalPlayer, UGameSetting* Setting, EGameSettingChangeReason Reason) const
	{
	}

	/**
	 * 设置需要重新求值编辑状态时调用。通常源于依赖项变化，
	 * 或该 edit condition 自身触发了 OnEditConditionChangedEvent。
	 */
	virtual void GatherEditState(const ULocalPlayer* InLocalPlayer, FGameSettingEditableState& InOutEditState) const
	{
	}

	/** 生成该 edit condition 的调试文本，便于排查非预期行为。 */
	virtual FString ToString() const { return TEXT(""); }
};

#undef UE_API
