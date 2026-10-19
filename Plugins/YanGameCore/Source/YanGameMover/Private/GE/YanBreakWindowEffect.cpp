#include "GE/YanBreakWindowEffect.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "MoverAbility/YanBreakAbility.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanBreakWindowEffect)

UYanBreakWindowEffect::UYanBreakWindowEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy    = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.25f));

	// 同一玩家在窗口内被再次击中时刷新而非叠加：两次受击共享一次卸力机会，
	// 否则连击会攒出一串窗口，最后一次击退过去很久仍能卸力
	// 引擎 setter 未导出且仅供编辑器使用，跨模块 CDO 需直接初始化该字段。
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateBySource;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount            = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;

	// 组件必须走 CreateDefaultSubobject：CDO 构造期内禁止匿名 NewObject，
	// 且固定名字是子类沿 archetype 继承本组件标签配置的前提
	UTargetTagsGameplayEffectComponent* TargetTags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(this, TEXT("TargetTagsComponent"));
	GEComponents.Add(TargetTags);

	// CDO 不走 PostLoad，此处是标签写入 CachedGrantedTags 的唯一入口
	FInheritedTagContainer GrantedTags;
	GrantedTags.Added.AddTag(Status_Break_Window);
	TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
}
