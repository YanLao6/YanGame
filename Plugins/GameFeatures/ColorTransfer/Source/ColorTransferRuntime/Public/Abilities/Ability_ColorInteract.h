#pragma once

#include "Engine/CollisionProfile.h"
#include "Interaction/Abilities/GameplayAbility_Interact.h"
#include "UIExtensionSystem.h"

#include "Ability_ColorInteract.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class UUserWidget;

/**
 * ColorTransfer 的交互技能：常驻激活，本地控制端沿准星检测可交互物并刷新选项，按下交互输入时触发首选项。
 *
 * 有可交互选项时把 WidgetClass 挂到 HUD 的 WidgetExtensionPointTag 扩展点，无选项或技能结束时撤下；
 * 不使用基类的世界空间指示器。服务器端"靠近即授予交互技能"沿用基类；输入 Tag 在技能集中配置。
 */
UCLASS(MinimalAPI)
class UAbility_ColorInteract : public UGameplayAbility_Interact
{
	GENERATED_BODY()

public:
	UE_API explicit UAbility_ColorInteract(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UGameplayAbility Interface
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	UE_API virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	UE_API virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	//~End UGameplayAbility Interface

protected:
	/** 本地检测所用的碰撞 Profile，其对象类型即检测通道 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Interaction")
	FCollisionProfileName TraceProfile;

	/** 有可交互选项时显示的提示控件 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Ability HUD Widget")
	TSubclassOf<UUserWidget> WidgetClass;

	/** 提示控件在 HUD 布局中的落点标签 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Ability HUD Widget", Meta = (Categories = "HUD.Slot"))
	FGameplayTag WidgetExtensionPointTag;

private:
	UFUNCTION()
	void OnInteractableOptionsChanged(const TArray<FInteractionOption>& InteractableOptions);

	void RemovePromptWidget();

	FUIExtensionHandle PromptExtensionHandle;
};

#undef UE_API
