#include "AttributeSets/YanAttributeSet.h"

#include "ActorComponent/ModularAbilitySystemComponent.h"

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
