#include "ModularAbilityPlayerState.h"

#include "ActorComponent/ModularAbilitySystemComponent.h"
#include "GameplayTagStack.h"
#include "Components/GameFrameworkComponentManager.h"

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

void AModularAbilityPlayerState::PreRevokePawnData()
{
	Super::PreRevokePawnData();

	// 正在激活的技能会让 ClearAbility 转为延迟移除，先行取消以确保句柄立即回收。
	// 取消不影响授予状态，Experience 级技能仍保留在 ASC 上。
	if (IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent->CancelAbilities();
	}
}

void AModularAbilityPlayerState::PostApplyPawnData()
{
	Super::PostApplyPawnData();

	// PawnData 与其 Fragment 注入均已就绪，通知等待中的 GameFeatureAction。
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
