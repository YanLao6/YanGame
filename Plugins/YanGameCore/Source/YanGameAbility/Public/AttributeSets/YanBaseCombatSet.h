#pragma once

#include "AttributeSets/YanAttributeSet.h"
#include "AbilitySystemComponent.h"

#include "YanBaseCombatSet.generated.h"

struct FGameplayEffectModCallbackData;

/**
 * 战斗基础属性集。
 */
UCLASS(BlueprintType)
class YANGAMEABILITY_API UYanBaseCombatSet : public UYanAttributeSet
{
	GENERATED_BODY()

public:
	UYanBaseCombatSet();

	ATTRIBUTE_ACCESSORS(UYanBaseCombatSet, CursedEnergy);
	ATTRIBUTE_ACCESSORS(UYanBaseCombatSet, MaxCursedEnergy);
	ATTRIBUTE_ACCESSORS(UYanBaseCombatSet, CursedEnergyCost);
	ATTRIBUTE_ACCESSORS(UYanBaseCombatSet, CursedEnergyRegen);

	ATTRIBUTE_ACCESSORS(UYanBaseCombatSet, Poise);
	ATTRIBUTE_ACCESSORS(UYanBaseCombatSet, MaxPoise);
	ATTRIBUTE_ACCESSORS(UYanBaseCombatSet, PoiseDamage);
	ATTRIBUTE_ACCESSORS(UYanBaseCombatSet, PoiseRegen);

	/** CursedEnergy 变化时广播。 */
	mutable FYanAttributeEvent OnCursedEnergyChanged;

	/** MaxCursedEnergy 变化时广播。 */
	mutable FYanAttributeEvent OnMaxCursedEnergyChanged;

	/** Poise 变化时广播。 */
	mutable FYanAttributeEvent OnPoiseChanged;

	/** MaxPoise 变化时广播。 */
	mutable FYanAttributeEvent OnMaxPoiseChanged;

	/** Poise 归零时广播（供晕眩逻辑使用）。 */
	mutable FYanAttributeEvent OnOutOfPoise;

	//~Begin UObject Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End UObject Interface

protected:
	UFUNCTION()
	void OnRep_CursedEnergy(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxCursedEnergy(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Poise(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxPoise(const FGameplayAttributeData& OldValue);

	//~Begin UAttributeSet Interface
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	//~End UAttributeSet Interface

	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

private:
	// 当前咒力，上限由 MaxCursedEnergy 约束；隐藏于 Modifier，仅 Execution 可改
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CursedEnergy, Category = "Yan|CursedEnergy", Meta = (HideFromModifiers, AllowPrivateAccess = true))
	FGameplayAttributeData CursedEnergy;

	// 最大咒力，可被 GameplayEffect 修改
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxCursedEnergy, Category = "Yan|CursedEnergy", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxCursedEnergy;

	// 当前韧性（0-3），归零触发晕眩；隐藏于 Modifier，仅 Execution 可改
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Poise, Category = "Yan|Poise", Meta = (HideFromModifiers, AllowPrivateAccess = true))
	FGameplayAttributeData Poise;

	// 最大韧性，可被 GameplayEffect 修改
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxPoise, Category = "Yan|Poise", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxPoise;

	// 是否已处于韧性归零状态，避免重复广播
	bool bOutOfPoise = false;

	// 变化前的 CursedEnergy / Poise 备份
	float CursedEnergyBeforeAttributeChange = 0.0f;
	float PoiseBeforeAttributeChange        = 0.0f;

	// meta：咒力消耗，映射为 -CursedEnergy
	UPROPERTY(BlueprintReadOnly, Category = "Yan|CursedEnergy", Meta = (HideFromModifiers, AllowPrivateAccess = true))
	FGameplayAttributeData CursedEnergyCost;

	// meta：咒力恢复，映射为 +CursedEnergy
	UPROPERTY(BlueprintReadOnly, Category = "Yan|CursedEnergy", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData CursedEnergyRegen;

	// meta：韧性打断，映射为 -Poise
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Poise", Meta = (HideFromModifiers, AllowPrivateAccess = true))
	FGameplayAttributeData PoiseDamage;

	// meta：韧性恢复，映射为 +Poise
	UPROPERTY(BlueprintReadOnly, Category = "Yan|Poise", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData PoiseRegen;
};
