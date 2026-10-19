#include "Player/YanDeathAbility.h"

#include "ActorComponent/ModularAbilitySystemComponent.h"
#include "AttributeSets/YanImpulseSet.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameMode/ModularExperienceGameMode.h"
#include "GameMode/YanDuelRoundComponent.h"
#include "Hero/YanHealthComponent.h"
#include "ModularAbilityTags.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanDeathAbility)

UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Death, "GameplayEvent.Death");

UYanDeathAbility::UYanDeathAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 出界由服务器判定，客户端无从预测死亡
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FAbilityTriggerData TriggerData;
		TriggerData.TriggerTag    = GameplayEvent_Death;
		TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(TriggerData);
	}
}

void UYanDeathAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	check(ActorInfo);

	UModularAbilitySystemComponent* ASC = CastChecked<UModularAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());

	// 死亡期间只放行显式声明可存活的技能，其余一律中止
	FGameplayTagContainer AbilityTypesToIgnore;
	AbilityTypesToIgnore.AddTag(ModularAbilityTags::Ability_Behavior_SurvivesDeath);
	ASC->CancelAbilities(nullptr, &AbilityTypesToIgnore, this);

	// 死亡不可被打断，否则重生定时器会随技能取消一并丢失
	SetCanBeCanceled(false);

	ASC->AddLooseGameplayTag(ModularAbilityTags::Status_Death_Dead);

	StartDeath();
	ResetLoad();
	NotifyRoundComponent();

	if (bAutoRequestRespawn)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(RespawnTimerHandle, this, &UYanDeathAbility::RequestRespawn, FMath::Max(RespawnDelay, KINDA_SMALL_NUMBER), /*bLoop=*/false);
		}
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UYanDeathAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 无论技能因何结束都收束死亡阶段，避免 Pawn 停在 DeathStarted 上永不解绑
	FinishDeath();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimerHandle);
	}

	if (ActorInfo != nullptr)
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->RemoveLooseGameplayTag(ModularAbilityTags::Status_Death_Dead);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UYanDeathAbility::StartDeath() const
{
	if (UYanHealthComponent* HealthComponent = UYanHealthComponent::FindHealthComponent(GetAvatarActorFromActorInfo());
	    HealthComponent != nullptr && HealthComponent->GetDeathState() == EYanDeathState::NotDead)
	{
		HealthComponent->StartDeath();
	}
}

void UYanDeathAbility::FinishDeath() const
{
	if (UYanHealthComponent* HealthComponent = UYanHealthComponent::FindHealthComponent(GetAvatarActorFromActorInfo());
	    HealthComponent != nullptr && HealthComponent->GetDeathState() == EYanDeathState::DeathStarted)
	{
		HealthComponent->FinishDeath();
	}
}

void UYanDeathAbility::ResetLoad() const
{
	// 冲量属性集只在对抗 Experience 里授予，别处死亡时无负荷可清
	if (UModularAbilitySystemComponent* ASC = Cast<UModularAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	    ASC != nullptr && ASC->HasAttributeSetForAttribute(UYanImpulseSet::GetLoadAttribute()))
	{
		ASC->ApplyModToAttribute(UYanImpulseSet::GetLoadAttribute(), EGameplayModOp::Override, 0.0f);
	}
}

void UYanDeathAbility::NotifyRoundComponent() const
{
	// 赛制由 GameState 上的组件承载，且只在对抗 Experience 里存在；
	// 缺席时死亡仍照常清算与重生，只是不计分
	const UWorld* World = GetWorld();
	if (World == nullptr || World->GetGameState() == nullptr)
	{
		return;
	}

	UYanDuelRoundComponent* RoundComp = World->GetGameState()->FindComponentByClass<UYanDuelRoundComponent>();
	if (RoundComp == nullptr)
	{
		return;
	}

	if (const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo())
	{
		RoundComp->NotifyPlayerOut(Cast<APlayerState>(Info->OwnerActor.Get()));
	}
}

void UYanDeathAbility::RequestRespawn()
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (Info == nullptr)
	{
		return;
	}

	AController* OwningController = nullptr;
	if (const APlayerState* OwningPlayerState = Cast<APlayerState>(Info->OwnerActor.Get()))
	{
		OwningController = OwningPlayerState->GetOwningController();
	}

	if (OwningController == nullptr)
	{
		return;
	}

	// 先结束技能收束死亡阶段，再请求重生：两者都排在下一帧，旧 Pawn 必须先让出 Avatar
	constexpr bool bReplicateEndAbility = true;
	constexpr bool bWasCancelled        = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);

	if (UWorld* World = GetWorld())
	{
		if (AModularExperienceGameMode* GameMode = World->GetAuthGameMode<AModularExperienceGameMode>())
		{
			GameMode->RequestPlayerRestartNextFrame(OwningController, /*bForceReset=*/true);
		}
	}
}
