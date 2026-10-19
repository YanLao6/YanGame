#include "AttributeSets/YanImpulseSet.h"

#include "AttributeSets/YanAttributeSet.h"
#include "ActorComponent/ModularAbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanImpulseSet)

UYanImpulseSet::UYanImpulseSet()
	: Load(0.0f)
	  , MaxLoad(100.0f)
	  , Charge(3.0f)
	  , MaxCharge(3.0f)
{}

void UYanImpulseSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UYanImpulseSet, Load, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UYanImpulseSet, MaxLoad, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UYanImpulseSet, Charge, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UYanImpulseSet, MaxCharge, COND_None, REPNOTIFY_Always);
}

void UYanImpulseSet::OnRep_Load(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UYanImpulseSet, Load, OldValue);

	const float CurrentLoad = GetLoad();
	OnLoadChanged.Broadcast(nullptr, nullptr, nullptr, CurrentLoad - OldValue.GetCurrentValue(), OldValue.GetCurrentValue(), CurrentLoad);
}

void UYanImpulseSet::OnRep_MaxLoad(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UYanImpulseSet, MaxLoad, OldValue);
}

void UYanImpulseSet::OnRep_Charge(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UYanImpulseSet, Charge, OldValue);

	const float CurrentCharge = GetCharge();
	OnChargeChanged.Broadcast(nullptr, nullptr, nullptr, CurrentCharge - OldValue.GetCurrentValue(), OldValue.GetCurrentValue(), CurrentCharge);
}

void UYanImpulseSet::OnRep_MaxCharge(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UYanImpulseSet, MaxCharge, OldValue);
}

bool UYanImpulseSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data))
	{
		return false;
	}

	LoadBeforeAttributeChange   = GetLoad();
	ChargeBeforeAttributeChange = GetCharge();

	return true;
}

void UYanImpulseSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayEffectContextHandle& EffectContext = Data.EffectSpec.GetEffectContext();
	AActor*                             Instigator    = EffectContext.GetOriginalInstigator();
	AActor*                             Causer        = EffectContext.GetEffectCauser();

	if (Data.EvaluatedData.Attribute == GetLoadAttribute())
	{
		SetLoad(FMath::Clamp(GetLoad(), 0.0f, GetMaxLoad()));
	}
	else if (Data.EvaluatedData.Attribute == GetChargeAttribute())
	{
		// 能量以整格结算，避免多次分数增减累积出 2.9999 这类无法通过整数消耗判定的值
		SetCharge(FMath::Clamp(FMath::RoundToFloat(GetCharge()), 0.0f, GetMaxCharge()));
	}

	if (GetLoad() != LoadBeforeAttributeChange)
	{
		OnLoadChanged.Broadcast(Instigator, Causer, &Data.EffectSpec, Data.EvaluatedData.Magnitude, LoadBeforeAttributeChange, GetLoad());
	}

	if (GetCharge() != ChargeBeforeAttributeChange)
	{
		OnChargeChanged.Broadcast(Instigator, Causer, &Data.EffectSpec, Data.EvaluatedData.Magnitude, ChargeBeforeAttributeChange, GetCharge());
	}
}

void UYanImpulseSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void UYanImpulseSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void UYanImpulseSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetMaxLoadAttribute())
	{
		// 上限下降时收敛当前值，否则 Load 会停在旧上限并让击飞计算超出设计区间
		if (GetLoad() > NewValue)
		{
			UModularAbilitySystemComponent* YanASC = GetModularAbilitySystemComponent();
			check(YanASC);

			YanASC->ApplyModToAttribute(GetLoadAttribute(), EGameplayModOp::Override, NewValue);
		}
	}
	else if (Attribute == GetMaxChargeAttribute())
	{
		if (GetCharge() > NewValue)
		{
			UModularAbilitySystemComponent* YanASC = GetModularAbilitySystemComponent();
			check(YanASC);

			YanASC->ApplyModToAttribute(GetChargeAttribute(), EGameplayModOp::Override, NewValue);
		}
	}
}

void UYanImpulseSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetLoadAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxLoad());
	}
	else if (Attribute == GetMaxLoadAttribute())
	{
		// 下限为 1，避免击飞公式出现除零与零上限下的整场满负荷
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetChargeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxCharge());
	}
	else if (Attribute == GetMaxChargeAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
}
