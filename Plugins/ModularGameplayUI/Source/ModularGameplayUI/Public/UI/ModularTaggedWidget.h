// Copyright Chronicler.

#pragma once

#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"

#include "ModularTaggedWidget.generated.h"

#define UE_API MODULARGAMEPLAYUI_API

/**
 * 受 GameplayTag 控制显隐的布局控件。
 *
 * 当拥有者玩家携带 `HiddenByTags` 中的任意标签时，该控件切换为 `HiddenVisibility`。
 * 注意：标签变化的监听尚未接入，当前 `HiddenByTags` 不会实际触发隐藏。
 */
UCLASS(MinimalAPI, Abstract, Blueprintable)
class UModularTaggedWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UE_API UModularTaggedWidget(const FObjectInitializer& ObjectInitializer);

	//~Begin UWidget Interface
	/** 记录调用方期望的显隐意图，再结合标签状态决定最终可见性。 */
	UE_API virtual void SetVisibility(ESlateVisibility InVisibility) override;
	//~End UWidget Interface

	//~Begin UUserWidget Interface
	UE_API virtual void NativeConstruct() override;
	UE_API virtual void NativeDestruct() override;
	//~End UUserWidget Interface

protected:
	/** 拥有者玩家携带其中任意标签时，控件以 HiddenVisibility 隐藏。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FGameplayTagContainer HiddenByTags;

	/** 未被标签隐藏时使用的可见性。 */
	UPROPERTY(EditAnywhere, Category = "HUD")
	ESlateVisibility ShownVisibility = ESlateVisibility::Visible;

	/** 被标签隐藏时使用的可见性。 */
	UPROPERTY(EditAnywhere, Category = "HUD")
	ESlateVisibility HiddenVisibility = ESlateVisibility::Collapsed;

	/** 调用方期望的显隐意图，不受标签影响。 */
	bool bWantsToBeVisible = true;

private:
	// 监听的标签发生变化时重新计算可见性。
	UE_API void OnWatchedTagsChanged();

	// 结合显隐意图与标签状态，应用最终可见性。
	UE_API void ApplyDesiredVisibility();
};

#undef UE_API
