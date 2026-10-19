#pragma once

#include "AttributeSets/YanAttributeSet.h"
#include "AbilitySystemComponent.h"

#include "YanImpulseSet.generated.h"

#define UE_API YANGAMEABILITY_API

struct FGameplayEffectModCallbackData;

/**
 * 冲量属性集：承载"被推得多远"与"还能自己动几次"这两项对抗资源。
 *
 * Load 只放大他人施加的击飞，自身不造成任何伤害；Charge 是空中位移的唯一预算。
 * 二者共处一个属性集，因为它们描述的是同一件事的两面：被动位移的代价与主动位移的余额。
 */
UCLASS(MinimalAPI, BlueprintType)
class UYanImpulseSet : public UYanAttributeSet
{
	GENERATED_BODY()

public:
	UE_API UYanImpulseSet();

	ATTRIBUTE_ACCESSORS(UYanImpulseSet, Load);
	ATTRIBUTE_ACCESSORS(UYanImpulseSet, MaxLoad);
	ATTRIBUTE_ACCESSORS(UYanImpulseSet, Charge);
	ATTRIBUTE_ACCESSORS(UYanImpulseSet, MaxCharge);

	/** Load 变化时广播，供表现层切换材质与 HUD。 */
	mutable FYanAttributeEvent OnLoadChanged;

	/** Charge 变化时广播。 */
	mutable FYanAttributeEvent OnChargeChanged;

	//~Begin UObject Interface
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End UObject Interface

protected:
	UFUNCTION()
	UE_API void OnRep_Load(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_MaxLoad(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_Charge(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_MaxCharge(const FGameplayAttributeData& OldValue);

	//~Begin UAttributeSet Interface
	UE_API virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	UE_API virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	UE_API virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	UE_API virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	UE_API virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	//~End UAttributeSet Interface

	// 将属性值 clamp 到合法区间
	UE_API void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

private:
	// 当前负荷，上限由 MaxLoad 约束；出界重生时由死亡流程清零
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Load, Category = "Yan|Impulse", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Load;

	// 负荷上限，达到后不再累积但击飞仍按上限结算
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxLoad, Category = "Yan|Impulse", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxLoad;

	// 当前机动能量，以整数格为单位存储于浮点属性中
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Charge, Category = "Yan|Impulse", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Charge;

	// 机动能量上限
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxCharge, Category = "Yan|Impulse", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxCharge;

	// 变化前的备份，供结算后比较
	float LoadBeforeAttributeChange   = 0.0f;
	float ChargeBeforeAttributeChange = 0.0f;
};

#undef UE_API
