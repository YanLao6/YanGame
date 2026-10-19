// Copyright Chronicler.

#include "GameplayAbilities/ModularAbilityGrant.h"

#include "AttributeSet.h"
#include "Engine/DataTable.h"
#include "ActorComponent/ModularAbilitySystemComponent.h"
#include "GameplayAbilities/ModularAbilitySet.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularAbilityGrant)

void ModularAbilityGrant::GiveToAbilitySystem(
	UModularAbilitySystemComponent* ModularASC,
	TConstArrayView<FModularAbilityGrant> Abilities,
	TConstArrayView<FModularEffectGrant> Effects,
	TConstArrayView<FModularAttributeSetGrant> Attributes,
	TConstArrayView<TSoftObjectPtr<const UModularAbilitySet>> AbilitySets,
	FModularAbilitySet_GrantedHandles& OutGrantedHandles)
{
	check(ModularASC);

	if (!ModularASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	// 调用方的 AssetBundle 不保证生效（PawnData 即如此），软引用一律同步加载。
	for (const FModularAttributeSetGrant& Grant : Attributes)
	{
		const TSubclassOf<UAttributeSet> SetType = Grant.AttributeSetType.LoadSynchronous();
		if (!SetType)
		{
			continue;
		}

		UAttributeSet* NewSet = NewObject<UAttributeSet>(ModularASC->GetOwner(), SetType);
		if (UDataTable* InitData = Grant.InitializationData.LoadSynchronous())
		{
			NewSet->InitFromMetaDataTable(InitData);
		}

		ModularASC->AddAttributeSetSubobject(NewSet);
		OutGrantedHandles.AddAttributeSet(NewSet);
	}

	for (const FModularAbilityGrant& Grant : Abilities)
	{
		const TSubclassOf<UGameplayAbility> AbilityClass = Grant.AbilityType.LoadSynchronous();
		if (!AbilityClass)
		{
			continue;
		}

		FGameplayAbilitySpec AbilitySpec(AbilityClass, Grant.AbilityLevel);
		if (Grant.InputTag.IsValid())
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(Grant.InputTag);
		}

		OutGrantedHandles.AddAbilitySpecHandle(ModularASC->GiveAbility(AbilitySpec));
	}

	for (const FModularEffectGrant& Grant : Effects)
	{
		const TSubclassOf<UGameplayEffect> EffectClass = Grant.EffectType.LoadSynchronous();
		if (!EffectClass)
		{
			continue;
		}

		const UGameplayEffect* GameplayEffect = EffectClass->GetDefaultObject<UGameplayEffect>();
		OutGrantedHandles.AddGameplayEffectHandle(ModularASC->ApplyGameplayEffectToSelf(GameplayEffect, Grant.EffectLevel, ModularASC->MakeEffectContext()));
	}

	for (const TSoftObjectPtr<const UModularAbilitySet>& SetPtr : AbilitySets)
	{
		if (const UModularAbilitySet* AbilitySet = SetPtr.LoadSynchronous())
		{
			AbilitySet->GiveToAbilitySystem(ModularASC, &OutGrantedHandles);
		}
	}
}
