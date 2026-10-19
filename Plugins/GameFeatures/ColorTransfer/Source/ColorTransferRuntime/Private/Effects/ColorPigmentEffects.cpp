#include "Effects/ColorPigmentEffects.h"

#include "AbilitySystemComponent.h"
#include "Attributes/ColorPigmentSet.h"
#include "ColorTransferRuntimeModule.h"
#include "DataAsset/ModularAssetManager.h"
#include "Messages/VerbMessageHelpers.h"
#include "System/ColorTransferGameData.h"

UE_DEFINE_GAMEPLAY_TAG(SetByCaller_ColorPigment_Red, "SetByCaller.ColorPigment.Red");
UE_DEFINE_GAMEPLAY_TAG(SetByCaller_ColorPigment_Blue, "SetByCaller.ColorPigment.Blue");
UE_DEFINE_GAMEPLAY_TAG(SetByCaller_ColorPigment_Black, "SetByCaller.ColorPigment.Black");
UE_DEFINE_GAMEPLAY_TAG(SetByCaller_ColorPigment_White, "SetByCaller.ColorPigment.White");
UE_DEFINE_GAMEPLAY_TAG(SetByCaller_ColorPigment_Amount, "SetByCaller.ColorPigment.Amount");

namespace ColorPigmentEffects
{
	// 构造 Spec 并施加到目标颜料集所在的 ASC；非服务器、ASC 缺失或 GE 未配置时返回无效句柄
	FActiveGameplayEffectHandle Apply(UColorPigmentSet& Target, const TSoftClassPtr<UGameplayEffect>& EffectClass, AActor* EffectCauser, TFunctionRef<void(FGameplayEffectSpec&)> SetMagnitudes)
	{
		UAbilitySystemComponent* TargetASC = Target.GetOwningAbilitySystemComponent();
		if (!TargetASC || !TargetASC->IsOwnerActorAuthoritative())
		{
			return FActiveGameplayEffectHandle();
		}

		const TSubclassOf<UGameplayEffect> LoadedClass = UModularAssetManager::GetSubclass(EffectClass);
		if (!LoadedClass)
		{
			UE_LOG(LogColorTransfer, Error, TEXT("颜料 GE 未配置或加载失败：%s，检查 UColorTransferGameData"), *EffectClass.ToString());
			return FActiveGameplayEffectHandle();
		}

		AActor* Instigator = EffectCauser && EffectCauser->GetInstigator() ? EffectCauser->GetInstigator() : EffectCauser;
		UAbilitySystemComponent* SourceASC = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(Instigator);
		if (!SourceASC)
		{
			SourceASC = TargetASC;
		}

		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		if (EffectCauser)
		{
			Context.AddInstigator(Instigator, EffectCauser);
		}

		const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(LoadedClass, 1.0f, Context);
		if (!Spec.IsValid())
		{
			return FActiveGameplayEffectHandle();
		}

		SetMagnitudes(*Spec.Data);
		return SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
	}
}

FGameplayTag ColorTransfer::GetPigmentSetByCallerTag(EColorPigment Pigment)
{
	switch (Pigment)
	{
	case EColorPigment::Red:
		return SetByCaller_ColorPigment_Red;
	case EColorPigment::Blue:
		return SetByCaller_ColorPigment_Blue;
	case EColorPigment::Black:
		return SetByCaller_ColorPigment_Black;
	case EColorPigment::White:
		return SetByCaller_ColorPigment_White;
	default:
		return FGameplayTag();
	}
}

float ColorTransfer::AuthApplyInjection(UColorPigmentSet& Target, EColorPigment Pigment, float Amount, AActor* EffectCauser)
{
	const float Accepted = Target.CalcAcceptedInjection(Pigment, Amount);
	if (Accepted <= 0.0f)
	{
		return 0.0f;
	}

	const FActiveGameplayEffectHandle Handle = ColorPigmentEffects::Apply(Target, UColorTransferGameData::Get().InjectEffect, EffectCauser, [Pigment, Amount](FGameplayEffectSpec& Spec)
	{
		Spec.SetSetByCallerMagnitude(GetPigmentSetByCallerTag(Pigment), Amount);
	});

	return Handle.WasSuccessfullyApplied() ? Accepted : 0.0f;
}

float ColorTransfer::AuthApplyShotConsumption(UColorPigmentSet& Shooter, float Amount, EColorPigment& OutPigment, AActor* EffectCauser)
{
	const float Fired = Shooter.CalcShotOutput(Amount, OutPigment);
	if (Fired <= 0.0f || OutPigment == Shooter.GetOwnPigment())
	{
		const UAbilitySystemComponent* ASC = Shooter.GetOwningAbilitySystemComponent();
		return ASC && ASC->IsOwnerActorAuthoritative() ? Fired : 0.0f;
	}

	const FActiveGameplayEffectHandle Handle = ColorPigmentEffects::Apply(Shooter, UColorTransferGameData::Get().ShotConsumptionEffect, EffectCauser, [Fired](FGameplayEffectSpec& Spec)
	{
		Spec.SetSetByCallerMagnitude(SetByCaller_ColorPigment_Amount, Fired);
	});

	return Handle.WasSuccessfullyApplied() ? Fired : 0.0f;
}

void ColorTransfer::AuthApplyForeignDecay(UColorPigmentSet& Target, float Amount, AActor* EffectCauser)
{
	if (Amount <= 0.0f || Target.GetLayers().IsEmpty())
	{
		return;
	}

	ColorPigmentEffects::Apply(Target, UColorTransferGameData::Get().ForeignDecayEffect, EffectCauser, [Amount](FGameplayEffectSpec& Spec)
	{
		Spec.SetSetByCallerMagnitude(SetByCaller_ColorPigment_Amount, Amount);
	});
}

void ColorTransfer::AuthApplyReset(UColorPigmentSet& Target, EColorPigment OwnPigment)
{
	ColorPigmentEffects::Apply(Target, UColorTransferGameData::Get().ResetEffect, /*EffectCauser=*/nullptr, [OwnPigment](FGameplayEffectSpec& Spec)
	{
		if (const FGameplayTag Tag = GetPigmentSetByCallerTag(OwnPigment); Tag.IsValid())
		{
			Spec.SetSetByCallerMagnitude(Tag, 1.0f);
		}
	});
}

float ColorTransfer::AuthAbsorbFromTopLayer(UColorPigmentSet& Target, TArray<FColorPigmentLayer>& SourceLayers, float MaxAmount, AActor* EffectCauser)
{
	if (SourceLayers.IsEmpty() || MaxAmount <= 0.0f || Target.GetOwnPigment() == EColorPigment::None)
	{
		return 0.0f;
	}

	const FColorPigmentLayer& Top = SourceLayers.Last();
	const float Requested = FMath::Min(Top.Amount, MaxAmount);

	float Absorbed = 0.0f;
	if (Top.Pigment == Target.GetOwnPigment())
	{
		// 身上没有外来颜料可冲时注入量为 0，但容器里的自身颜色照样取走
		const UAbilitySystemComponent* ASC = Target.GetOwningAbilitySystemComponent();
		if (ASC && ASC->IsOwnerActorAuthoritative())
		{
			AuthApplyInjection(Target, Top.Pigment, Requested, EffectCauser);
			Absorbed = Requested;
		}
	}
	else
	{
		Absorbed = AuthApplyInjection(Target, Top.Pigment, Requested, EffectCauser);
	}

	if (Absorbed > 0.0f)
	{
		DrainTopLayer(SourceLayers, Absorbed);
	}

	return Absorbed;
}
