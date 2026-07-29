#pragma once

#include "AttributeSets/YanAttributeSet.h"
#include "AbilitySystemComponent.h"

#include "YanHealthSet.generated.h"

struct FGameplayEffectModCallbackData;

/**
 * 生命属性集。
 */
UCLASS(BlueprintType)
class YANGAMEABILITY_API UYanHealthSet : public UYanAttributeSet
{
	GENERATED_BODY()

public:
	UYanHealthSet();

	ATTRIBUTE_ACCESSORS(UYanHealthSet, Health);
	ATTRIBUTE_ACCESSORS(UYanHealthSet, MaxHealth);
	ATTRIBUTE_ACCESSORS(UYanHealthSet, Healing);
	ATTRIBUTE_ACCESSORS(UYanHealthSet, Damage);

	/** Health 因伤害/治疗变化时广播；客户端部分信息可能缺失。 */
	mutable FYanAttributeEvent OnHealthChanged;

	/** MaxHealth 变化时广播。 */
	mutable FYanAttributeEvent OnMaxHealthChanged;

	/** Health 归零时广播。 */
	mutable FYanAttributeEvent OnOutOfHealth;

	//~Begin UObject Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End UObject Interface

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	//~Begin UAttributeSet Interface
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	//~End UAttributeSet Interface

	// 将属性值 clamp 到合法区间
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

private:
	// 当前生命值，上限由 MaxHealth 约束；隐藏于 Modifier，仅 Execution 可改
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Yan|Health", Meta = (HideFromModifiers, AllowPrivateAccess = true))
	FGameplayAttributeData Health;

	// 最大生命值，可被 GameplayEffect 修改
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Yan|Health", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxHealth;

	// 是否已处于生命归零状态，避免重复广播
	bool bOutOfHealth = false;

	// 变化前的 Health / MaxHealth 备份
	float HealthBeforeAttributeChange    = 0.0f;
	float MaxHealthBeforeAttributeChange = 0.0f;

	// meta：传入治疗，直接映射为 +Health
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Health", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Healing;

	// meta：传入伤害，直接映射为 -Health
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Health", Meta = (HideFromModifiers, AllowPrivateAccess = true))
	FGameplayAttributeData Damage;
};
