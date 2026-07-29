#pragma once

#include "AttributeSet.h"
#include "AbilitySystemComponent.h"

#include "YanAttributeSet.generated.h"

class UModularAbilitySystemComponent;
struct FGameplayEffectSpec;

/**
 * 属性访问器宏：为指定属性一次性生成 Getter / Setter / Initter 与静态 AttributeGetter。
 *
 * 例：ATTRIBUTE_ACCESSORS(UYanHealthSet, Health) 会生成
 *   static FGameplayAttribute GetHealthAttribute();
 *   float GetHealth() const;
 *   void  SetHealth(float NewVal);
 *   void  InitHealth(float NewVal);
 */
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

DECLARE_MULTICAST_DELEGATE_SixParams(FYanAttributeEvent, AActor* /*EffectInstigator*/, AActor* /*EffectCauser*/, const FGameplayEffectSpec* /*EffectSpec*/, float /*EffectMagnitude*/, float /*OldValue*/, float /*NewValue*/);

/**
 * 项目 AttributeSet 基类。
 *
 * 提供 GetWorld 与项目 ASC 访问入口，供各具体 AttributeSet 复用。
 */
UCLASS()
class YANGAMEABILITY_API UYanAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UYanAttributeSet();

	//~Begin UObject Interface
	virtual UWorld* GetWorld() const override;
	//~End UObject Interface

	/** 获取拥有本 AttributeSet 的项目 ASC。 */
	UModularAbilitySystemComponent* GetModularAbilitySystemComponent() const;
};
