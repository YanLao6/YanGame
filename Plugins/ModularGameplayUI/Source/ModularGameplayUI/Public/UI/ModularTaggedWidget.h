// Copyright Chronicler.

#pragma once

#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"

#include "ModularTaggedWidget.generated.h"

#define UE_API MODULARGAMEPLAYUI_API

class APawn;
class APlayerController;
class UAbilitySystemComponent;

/**
 * 受 GameplayTag 控制显隐的布局控件。
 *
 * 当拥有者玩家所控制的 Pawn 经 IAbilitySystemInterface 暴露的 ASC 携带 `HiddenByTags` 中的任意标签时，
 * 该控件切换为 `HiddenVisibility`。Pawn 更换或其 ASC 延迟初始化时会自动重新绑定；没有 Pawn 或 ASC 时视为不携带标签。
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
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	// 切换监听的 Pawn；其 ASC 尚未初始化时等待扩展组件的初始化通知再绑定。
	void BindToPawn(APawn* Pawn);

	// 当前 Pawn 的 ASC 初始化完成后重新取 ASC 绑定。
	void HandleAbilitySystemInitialized();

	void BindToAbilitySystem(UAbilitySystemComponent* AbilitySystem);
	void UnbindFromAbilitySystem();

	// 监听的标签发生变化时重新计算可见性。
	void OnWatchedTagsChanged(const FGameplayTag Tag, int32 NewCount);

	// 结合显隐意图与标签状态，应用最终可见性。
	void ApplyDesiredVisibility();

	TWeakObjectPtr<APlayerController> WatchedController;
	TWeakObjectPtr<APawn> WatchedPawn;
	TWeakObjectPtr<UAbilitySystemComponent> WatchedAbilitySystem;
	TArray<TPair<FGameplayTag, FDelegateHandle>> WatchedTagHandles;
};

#undef UE_API
