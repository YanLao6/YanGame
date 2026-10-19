#pragma once

#include "GameplayCueNotify_Static.h"
#include "GameplayTagContainer.h"

#include "YanContextEffectCue.generated.h"

#define UE_API YANGAMEEFFECTS_API

/**
 * 把 GameplayCue 接入上下文特效体系的桥接类。
 *
 * 让技能与 GameplayEffect 复用与动画通知同一套「事件 + 上下文 → 资源」查表，
 * 且自带 GAS 的 Cue 复制语义，模拟端也能收到。
 *
 * 发现机制说明：GameplayCueManager 仅扫描内容路径中的蓝图资产建立 Tag 到类的映射，
 * 纯原生类不会被自动发现。需为本类创建 data-only 蓝图子类，设置其 GameplayCueTag 与下列属性
 * 并置于 GameplayCue 内容路径。
 */
UCLASS(MinimalAPI)
class UYanContextEffectCue : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	//~Begin UGameplayCueNotify_Static Interface
	UE_API virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	//~End UGameplayCueNotify_Static Interface

protected:
	/**
	 * 查表用的事件标签，与本类蓝图子类的 GameplayCueTag 相互独立。
	 * 二者解耦是为了让同一条特效库条目既能被动画通知触发，也能被 GameplayEffect 触发，
	 * 而不必在库里写带 GameplayCue 前缀的重复条目。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "ContextEffects")
	FGameplayTag EffectTag;

	/** 附着插槽；具体挂在哪个组件上由目标的响应方决定 */
	UPROPERTY(EditDefaultsOnly, Category = "ContextEffects")
	FName AttachPoint;

	/** 追加到本次播放的上下文 */
	UPROPERTY(EditDefaultsOnly, Category = "ContextEffects")
	FGameplayTagContainer AdditionalContexts;

	UPROPERTY(EditDefaultsOnly, Category = "ContextEffects")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "ContextEffects")
	FRotator RotationOffset = FRotator::ZeroRotator;
};

#undef UE_API
