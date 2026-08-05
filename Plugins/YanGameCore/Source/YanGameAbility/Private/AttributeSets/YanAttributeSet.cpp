#include "AttributeSets/YanAttributeSet.h"

#include "ActorComponent/ModularAbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanAttributeSet)

UYanAttributeSet::UYanAttributeSet()
{}

UWorld* UYanAttributeSet::GetWorld() const
{
	const UObject* Outer = GetOuter();
	check(Outer);

	return Outer->GetWorld();
}

UModularAbilitySystemComponent* UYanAttributeSet::GetModularAbilitySystemComponent() const
{
	return Cast<UModularAbilitySystemComponent>(GetOwningAbilitySystemComponent());
}
