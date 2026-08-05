// Copyright Chronicler.

#include "GameplayAbilities/ModularAbilityUtils.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularAbilityUtils)

FGameplayTagContainer UModularAbilityUtils::GetAbilityAssetTags(const UGameplayAbility* Ability)
{
	return Ability != nullptr ? Ability->GetAssetTags() : FGameplayTagContainer();
}

FGameplayTagContainer UModularAbilityUtils::GetAbilityClassAssetTags(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (AbilityClass == nullptr)
	{
		return FGameplayTagContainer();
	}

	return GetAbilityAssetTags(AbilityClass->GetDefaultObject<UGameplayAbility>());
}

bool UModularAbilityUtils::AbilityHasAssetTag(const UGameplayAbility* Ability, FGameplayTag AssetTag)
{
	return Ability != nullptr && Ability->GetAssetTags().HasTag(AssetTag);
}

bool UModularAbilityUtils::FindAbilityHandleByAssetTag(
	UAbilitySystemComponent*    AbilitySystemComponent,
	FGameplayTag                AssetTag,
	FGameplayAbilitySpecHandle& OutHandle)
{
	if (AbilitySystemComponent == nullptr || !AssetTag.IsValid())
	{
		return false;
	}

	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (Spec.Ability != nullptr && Spec.Ability->GetAssetTags().HasTag(AssetTag))
		{
			OutHandle = Spec.Handle;
			return true;
		}
	}

	return false;
}

void UModularAbilityUtils::FindAbilityHandlesByAssetTags(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayTagContainer& AssetTags, TArray<FGameplayAbilitySpecHandle>& OutHandles)
{
	if (AbilitySystemComponent == nullptr || AssetTags.IsEmpty())
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (Spec.Ability != nullptr && Spec.Ability->GetAssetTags().HasAny(AssetTags))
		{
			OutHandles.Add(Spec.Handle);
		}
	}
}

void UModularAbilityUtils::GetAllAbilityHandles(UAbilitySystemComponent* AbilitySystemComponent, TArray<FGameplayAbilitySpecHandle>& OutHandles)
{
	if (AbilitySystemComponent == nullptr)
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		OutHandles.Add(Spec.Handle);
	}
}
