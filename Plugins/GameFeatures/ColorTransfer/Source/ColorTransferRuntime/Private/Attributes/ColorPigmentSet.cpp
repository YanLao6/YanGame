#include "Attributes/ColorPigmentSet.h"

#include "AbilitySystemComponent.h"
#include "ColorTransferRuntimeModule.h"
#include "Effects/ColorPigmentEffects.h"
#include "GameplayEffectExtension.h"
#include "Messages/VerbMessageHelpers.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorPigmentSet)

namespace ColorPigmentSet
{
	constexpr EColorPigment Pigments[] = {EColorPigment::Red, EColorPigment::Blue, EColorPigment::Black, EColorPigment::White};
}

UColorPigmentSet* UColorPigmentSet::FindForActor(const AActor* Actor)
{
	UAbilitySystemComponent* ASC = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(const_cast<AActor*>(Actor));
	if (!ASC)
	{
		return nullptr;
	}

	for (UAttributeSet* Set : ASC->GetSpawnedAttributes())
	{
		if (UColorPigmentSet* PigmentSet = Cast<UColorPigmentSet>(Set))
		{
			return PigmentSet;
		}
	}

	return nullptr;
}

FGameplayAttribute UColorPigmentSet::GetPigmentAttribute(EColorPigment Pigment)
{
	switch (Pigment)
	{
	case EColorPigment::Red:
		return GetRedAttribute();
	case EColorPigment::Blue:
		return GetBlueAttribute();
	case EColorPigment::Black:
		return GetBlackAttribute();
	case EColorPigment::White:
		return GetWhiteAttribute();
	default:
		return FGameplayAttribute();
	}
}

FGameplayAttribute UColorPigmentSet::GetInjectAttribute(EColorPigment Pigment)
{
	switch (Pigment)
	{
	case EColorPigment::Red:
		return GetInjectRedAttribute();
	case EColorPigment::Blue:
		return GetInjectBlueAttribute();
	case EColorPigment::Black:
		return GetInjectBlackAttribute();
	case EColorPigment::White:
		return GetInjectWhiteAttribute();
	default:
		return FGameplayAttribute();
	}
}

float UColorPigmentSet::CalcAcceptedInjection(EColorPigment Pigment, float Amount) const
{
	if (OwnPigment == EColorPigment::None || Pigment == EColorPigment::None || Amount <= 0.0f)
	{
		return 0.0f;
	}

	if (Pigment == OwnPigment)
	{
		return FMath::Min(Amount, ColorTransfer::GetLayersTotal(Layers));
	}

	return FMath::Min(Amount, GetOwnPigmentAmount());
}

float UColorPigmentSet::CalcShotOutput(float Amount, EColorPigment& OutPigment) const
{
	OutPigment = GetShotPigment();

	if (OutPigment == EColorPigment::None || Amount <= 0.0f)
	{
		return 0.0f;
	}

	return Layers.IsEmpty() ? Amount : FMath::Min(Amount, Layers.Last().Amount);
}

EColorPigment UColorPigmentSet::GetShotPigment() const
{
	return Layers.IsEmpty() ? OwnPigment : Layers.Last().Pigment;
}

float UColorPigmentSet::GetOwnPigmentAmount() const
{
	if (OwnPigment == EColorPigment::None)
	{
		return 0.0f;
	}

	return FMath::Max(TotalPigment - ColorTransfer::GetLayersTotal(Layers), 0.0f);
}

void UColorPigmentSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UColorPigmentSet, Red, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UColorPigmentSet, Blue, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UColorPigmentSet, Black, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UColorPigmentSet, White, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME(UColorPigmentSet, OwnPigment);
	DOREPLIFETIME(UColorPigmentSet, Layers);
}

bool UColorPigmentSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data))
	{
		return false;
	}

	if (IsPigmentAttribute(Data.EvaluatedData.Attribute))
	{
		UE_LOG(LogColorTransfer, Warning, TEXT("%s 直接修改了颜色属性 %s，已拒绝；颜料须经 UColorTransferGameData 中的颜料 GE 改写 meta 属性"),
			*GetNameSafe(Data.EffectSpec.Def), *Data.EvaluatedData.Attribute.GetName());
		return false;
	}

	return true;
}

void UColorPigmentSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute& Attribute = Data.EvaluatedData.Attribute;
	bool bChanged = false;

	if (const EColorPigment InjectedPigment = GetInjectedPigment(Attribute); InjectedPigment != EColorPigment::None)
	{
		bChanged = InjectPigment(InjectedPigment, ConsumeMetaAttribute(Attribute));
	}
	else if (Attribute == GetShotConsumptionAttribute())
	{
		bChanged = DrainForShot(ConsumeMetaAttribute(Attribute));
	}
	else if (Attribute == GetForeignDecayAttribute())
	{
		bChanged = DrainForeignPigment(ConsumeMetaAttribute(Attribute));
	}
	else if (Attribute == GetPigmentResetAttribute())
	{
		ConsumeMetaAttribute(Attribute);

		EColorPigment NewOwnPigment = EColorPigment::None;
		for (const EColorPigment Pigment : ColorPigmentSet::Pigments)
		{
			if (Data.EffectSpec.GetSetByCallerMagnitude(ColorTransfer::GetPigmentSetByCallerTag(Pigment), /*WarnIfNotFound=*/false) > 0.0f)
			{
				NewOwnPigment = Pigment;
				break;
			}
		}

		bChanged = ResetToOwnPigment(NewOwnPigment);
	}

	if (bChanged)
	{
		SyncAttributesFromLayers();
	}
}

void UColorPigmentSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (IsPigmentAttribute(Attribute))
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, TotalPigment);
	}
}

void UColorPigmentSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (IsPigmentAttribute(Attribute))
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, TotalPigment);
	}
}

void UColorPigmentSet::OnRep_Red(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UColorPigmentSet, Red, OldValue);
}

void UColorPigmentSet::OnRep_Blue(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UColorPigmentSet, Blue, OldValue);
}

void UColorPigmentSet::OnRep_Black(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UColorPigmentSet, Black, OldValue);
}

void UColorPigmentSet::OnRep_White(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UColorPigmentSet, White, OldValue);
}

EColorPigment UColorPigmentSet::GetInjectedPigment(const FGameplayAttribute& Attribute)
{
	for (const EColorPigment Pigment : ColorPigmentSet::Pigments)
	{
		if (Attribute == GetInjectAttribute(Pigment))
		{
			return Pigment;
		}
	}

	return EColorPigment::None;
}

bool UColorPigmentSet::IsPigmentAttribute(const FGameplayAttribute& Attribute)
{
	for (const EColorPigment Pigment : ColorPigmentSet::Pigments)
	{
		if (Attribute == GetPigmentAttribute(Pigment))
		{
			return true;
		}
	}

	return false;
}

float UColorPigmentSet::ConsumeMetaAttribute(const FGameplayAttribute& Attribute)
{
	const float Value = Attribute.GetNumericValue(this);

	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		ASC->SetNumericAttributeBase(Attribute, 0.0f);
	}

	return Value;
}

bool UColorPigmentSet::InjectPigment(EColorPigment Pigment, float Amount)
{
	const float Accepted = CalcAcceptedInjection(Pigment, Amount);
	if (Accepted <= 0.0f)
	{
		return false;
	}

	if (Pigment == OwnPigment)
	{
		ColorTransfer::DrainLayers(Layers, Accepted);
	}
	else
	{
		ColorTransfer::PushLayer(Layers, Pigment, Accepted);
	}

	return true;
}

bool UColorPigmentSet::DrainForShot(float Amount)
{
	return Amount > 0.0f && ColorTransfer::DrainTopLayer(Layers, Amount) > 0.0f;
}

bool UColorPigmentSet::DrainForeignPigment(float Amount)
{
	return Amount > 0.0f && ColorTransfer::DrainLayers(Layers, Amount) > 0.0f;
}

bool UColorPigmentSet::ResetToOwnPigment(EColorPigment InOwnPigment)
{
	OwnPigment = InOwnPigment;
	Layers.Reset();

	// 重置总要重算并广播：出生时即使颜色与栈都没变，规则组件也要据此重新裁决
	return true;
}

void UColorPigmentSet::SyncAttributesFromLayers()
{
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	// 按枚举值下标累加，None 占位不写回
	float Totals[static_cast<uint8>(EColorPigment::White) + 1] = {};
	for (const FColorPigmentLayer& Layer : Layers)
	{
		Totals[static_cast<uint8>(Layer.Pigment)] += Layer.Amount;
	}

	Totals[static_cast<uint8>(OwnPigment)] += GetOwnPigmentAmount();

	for (const EColorPigment Pigment : ColorPigmentSet::Pigments)
	{
		ASC->SetNumericAttributeBase(GetPigmentAttribute(Pigment), Totals[static_cast<uint8>(Pigment)]);
	}

	OnPigmentChanged.Broadcast();
}
