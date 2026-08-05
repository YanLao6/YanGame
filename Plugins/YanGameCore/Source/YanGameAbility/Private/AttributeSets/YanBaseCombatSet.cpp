#include "AttributeSets/YanBaseCombatSet.h"

#include "AttributeSets/YanAttributeSet.h"
#include "ActorComponent/ModularAbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanBaseCombatSet)

UYanBaseCombatSet::UYanBaseCombatSet()
	: CursedEnergy(100.0f)
	  , MaxCursedEnergy(100.0f)
	  , Poise(3.0f)
	  , MaxPoise(3.0f)
{}

void UYanBaseCombatSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UYanBaseCombatSet, CursedEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UYanBaseCombatSet, MaxCursedEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UYanBaseCombatSet, Poise, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UYanBaseCombatSet, MaxPoise, COND_None, REPNOTIFY_Always);
}

void UYanBaseCombatSet::OnRep_CursedEnergy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UYanBaseCombatSet, CursedEnergy, OldValue);

	OnCursedEnergyChanged.Broadcast(nullptr, nullptr, nullptr, GetCursedEnergy() - OldValue.GetCurrentValue(), OldValue.GetCurrentValue(), GetCursedEnergy());
}

void UYanBaseCombatSet::OnRep_MaxCursedEnergy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UYanBaseCombatSet, MaxCursedEnergy, OldValue);

	OnMaxCursedEnergyChanged.Broadcast(nullptr, nullptr, nullptr, GetMaxCursedEnergy() - OldValue.GetCurrentValue(), OldValue.GetCurrentValue(), GetMaxCursedEnergy());
}

void UYanBaseCombatSet::OnRep_Poise(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UYanBaseCombatSet, Poise, OldValue);

	const float CurrentPoise       = GetPoise();
	const float EstimatedMagnitude = CurrentPoise - OldValue.GetCurrentValue();

	OnPoiseChanged.Broadcast(nullptr, nullptr, nullptr, EstimatedMagnitude, OldValue.GetCurrentValue(), CurrentPoise);

	if (!bOutOfPoise && CurrentPoise <= 0.0f)
	{
		OnOutOfPoise.Broadcast(nullptr, nullptr, nullptr, EstimatedMagnitude, OldValue.GetCurrentValue(), CurrentPoise);
	}

	bOutOfPoise = (CurrentPoise <= 0.0f);
}

void UYanBaseCombatSet::OnRep_MaxPoise(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UYanBaseCombatSet, MaxPoise, OldValue);

	OnMaxPoiseChanged.Broadcast(nullptr, nullptr, nullptr, GetMaxPoise() - OldValue.GetCurrentValue(), OldValue.GetCurrentValue(), GetMaxPoise());
}

bool UYanBaseCombatSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data))
	{
		return false;
	}

	// 记录变化前的资源值，用于结算后比较
	CursedEnergyBeforeAttributeChange = GetCursedEnergy();
	PoiseBeforeAttributeChange        = GetPoise();

	return true;
}

void UYanBaseCombatSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayEffectContextHandle& EffectContext = Data.EffectSpec.GetEffectContext();
	AActor*                             Instigator    = EffectContext.GetOriginalInstigator();
	AActor*                             Causer        = EffectContext.GetEffectCauser();

	if (Data.EvaluatedData.Attribute == GetCursedEnergyCostAttribute())
	{
		// 咒力消耗 meta 换算为 -CursedEnergy 并 clamp
		SetCursedEnergy(FMath::Clamp(GetCursedEnergy() - GetCursedEnergyCost(), 0.0f, GetMaxCursedEnergy()));
		SetCursedEnergyCost(0.0f);
	}
	else if (Data.EvaluatedData.Attribute == GetCursedEnergyRegenAttribute())
	{
		// 咒力恢复 meta 换算为 +CursedEnergy 并 clamp
		SetCursedEnergy(FMath::Clamp(GetCursedEnergy() + GetCursedEnergyRegen(), 0.0f, GetMaxCursedEnergy()));
		SetCursedEnergyRegen(0.0f);
	}
	else if (Data.EvaluatedData.Attribute == GetPoiseDamageAttribute())
	{
		// 韧性打断 meta 换算为 -Poise 并 clamp
		SetPoise(FMath::Clamp(GetPoise() - GetPoiseDamage(), 0.0f, GetMaxPoise()));
		SetPoiseDamage(0.0f);
	}
	else if (Data.EvaluatedData.Attribute == GetPoiseRegenAttribute())
	{
		// 韧性恢复 meta 换算为 +Poise 并 clamp
		SetPoise(FMath::Clamp(GetPoise() + GetPoiseRegen(), 0.0f, GetMaxPoise()));
		SetPoiseRegen(0.0f);
	}
	else if (Data.EvaluatedData.Attribute == GetCursedEnergyAttribute())
	{
		SetCursedEnergy(FMath::Clamp(GetCursedEnergy(), 0.0f, GetMaxCursedEnergy()));
	}
	else if (Data.EvaluatedData.Attribute == GetPoiseAttribute())
	{
		SetPoise(FMath::Clamp(GetPoise(), 0.0f, GetMaxPoise()));
	}

	if (GetCursedEnergy() != CursedEnergyBeforeAttributeChange)
	{
		OnCursedEnergyChanged.Broadcast(Instigator, Causer, &Data.EffectSpec, Data.EvaluatedData.Magnitude, CursedEnergyBeforeAttributeChange, GetCursedEnergy());
	}

	if (GetPoise() != PoiseBeforeAttributeChange)
	{
		OnPoiseChanged.Broadcast(Instigator, Causer, &Data.EffectSpec, Data.EvaluatedData.Magnitude, PoiseBeforeAttributeChange, GetPoise());
	}

	if ((GetPoise() <= 0.0f) && !bOutOfPoise)
	{
		OnOutOfPoise.Broadcast(Instigator, Causer, &Data.EffectSpec, Data.EvaluatedData.Magnitude, PoiseBeforeAttributeChange, GetPoise());
	}

	// 再次检查，防止上方广播的监听者又改动了 Poise
	bOutOfPoise = (GetPoise() <= 0.0f);
}

void UYanBaseCombatSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void UYanBaseCombatSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void UYanBaseCombatSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetMaxCursedEnergyAttribute())
	{
		// MaxCursedEnergy 下降时，保证当前咒力不超过新上限
		if (GetCursedEnergy() > NewValue)
		{
			UModularAbilitySystemComponent* YanASC = GetModularAbilitySystemComponent();
			check(YanASC);

			YanASC->ApplyModToAttribute(GetCursedEnergyAttribute(), EGameplayModOp::Override, NewValue);
		}
	}
	else if (Attribute == GetMaxPoiseAttribute())
	{
		// MaxPoise 下降时，保证当前韧性不超过新上限
		if (GetPoise() > NewValue)
		{
			UModularAbilitySystemComponent* YanASC = GetModularAbilitySystemComponent();
			check(YanASC);

			YanASC->ApplyModToAttribute(GetPoiseAttribute(), EGameplayModOp::Override, NewValue);
		}
	}

	if (bOutOfPoise && (GetPoise() > 0.0f))
	{
		bOutOfPoise = false;
	}
}

void UYanBaseCombatSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetCursedEnergyAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxCursedEnergy());
	}
	else if (Attribute == GetMaxCursedEnergyAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetPoiseAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxPoise());
	}
	else if (Attribute == GetMaxPoiseAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
}
