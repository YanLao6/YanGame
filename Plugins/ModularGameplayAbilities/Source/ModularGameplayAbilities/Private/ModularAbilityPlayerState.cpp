#include "ModularAbilityPlayerState.h"

#include "ActorComponent/ModularAbilitySystemComponent.h"
#include "GameplayTagStack.h"
#include "Components/GameFrameworkComponentManager.h"
#include "DataAsset/IAbilityPawnDataInterface.h"
#include "GameplayAbilities/ModularAbilitySet.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularAbilityPlayerState)


const FName AModularAbilityPlayerState::NAME_ModularAbilityReady("ModularAbilitiesReady");

AModularAbilityPlayerState::AModularAbilityPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystemComponent = CreateDefaultSubobject<UModularAbilitySystemComponent>("AbilitySystemComponent");
}

UAbilitySystemComponent* AModularAbilityPlayerState::GetAbilitySystemComponent() const
{
	return GetModularAbilitySystemComponent();
}

void AModularAbilityPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	check(AbilitySystemComponent);
	// 把 Avatar 重置为 Pawn（此时必定为 nullptr），覆盖掉底层默认设置的 PlayerState 自己。
	AbilitySystemComponent->InitAbilityActorInfo(this, GetPawn());
}

void AModularAbilityPlayerState::SetPawnData(const UModularPawnData* InPawnData)
{
	check(InPawnData);

	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	if (PawnData)
	{
		UE_LOG(LogTemp,
		       Error,
		       TEXT("Trying to set PawnData [%s] on player state [%s] that already has valid PawnData [%s]."),
		       *GetNameSafe(InPawnData),
		       *GetNameSafe(this),
		       *GetNameSafe(PawnData));
		return;
	}

	// 先通过 Super 完成 PawnData 赋值与网络标脏（匹配 Lyra：PawnData 在事件发出前已就绪）
	Super::SetPawnData(InPawnData);

	// PawnData 已设置，授予 PawnData 中定义的 AbilitySet
	if (const IAbilityPawnDataInterface* AbilityPawnData = Cast<IAbilityPawnDataInterface>(PawnData))
	{
		for (const UModularAbilitySet* AbilitySet : AbilityPawnData->GetAbilitySet())
		{
			if (AbilitySet)
			{
				AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, nullptr);
			}
		}
	}

	// 最后发事件，此时 PawnData 与 Abilities 均已就绪
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, NAME_ModularAbilityReady);
}

void AModularAbilityPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void AModularAbilityPlayerState::AddStatTagStack(FGameplayTag Tag, int32 StackCount)
{
	StatTags.AddStack(Tag, StackCount);
}

void AModularAbilityPlayerState::RemoveStatTagStack(FGameplayTag Tag, int32 StackCount)
{
	StatTags.RemoveStack(Tag, StackCount);
}

int32 AModularAbilityPlayerState::GetStatTagStackCount(FGameplayTag Tag) const
{
	return StatTags.GetStackCount(Tag);
}

bool AModularAbilityPlayerState::HasStatTag(FGameplayTag Tag) const
{
	return StatTags.ContainsTag(Tag);
}
