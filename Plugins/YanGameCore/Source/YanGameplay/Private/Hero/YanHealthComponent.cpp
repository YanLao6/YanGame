// 生命组件实现。

#include "Hero/YanHealthComponent.h"

#include "AttributeSets/YanHealthSet.h"
#include "ActorComponent/ModularAbilityExtensionComponent.h"
#include "ActorComponent/ModularAbilitySystemComponent.h"
#include "DataAsset/ModularAbilityData.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectExtension.h"
#include "Messages/ModularVerbMessage.h"
#include "Messages/VerbMessageHelpers.h"
#include "ModularAbilityTags.h"
#include "ModularGameplayAbilitiesLogChannels.h"
#include "Net/UnrealNetwork.h"
#include "Player/YanDeathAbility.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanHealthComponent)

namespace YanHealthTags
{
	// 击杀结算消息，与 YanShooter 侧监听的频道一致
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Message_Elimination, "Message.Elimination");

	// 自毁伤害的来源标记，供伤害计算与表现区分普通伤害
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Gameplay_DamageSelfDestruct, "Gameplay.DamageSelfDestruct");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Gameplay_FellOutOfWorld, "Gameplay.FellOutOfWorld");

	// 伤害 GE 读取伤害量所用的 SetByCaller 键
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SetByCaller_Damage, "SetByCaller.Damage");
}

UYanHealthComponent::UYanHealthComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);

	AbilitySystemComponent = nullptr;
	HealthSet = nullptr;
	DeathState = EYanDeathState::NotDead;
}

void UYanHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UYanHealthComponent, DeathState);
}

void UYanHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	// 本组件由蓝图挂载，构造期拿不到扩展组件，故在此按类型查找并订阅 Avatar 通知
	AActor* Owner = GetOwner();
	if (UModularAbilityExtensionComponent* AbilityExtension = Owner ? Owner->FindComponentByClass<UModularAbilityExtensionComponent>() : nullptr)
	{
		AbilityExtension->OnAbilitySystemInitialized_RegisterAndCall(
			FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemInitialized));
		AbilityExtension->OnAbilitySystemUninitialized_Register(
			FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemUninitialized));
	}
	else
	{
		UE_LOG(LogModularGameplayAbilities, Error, TEXT("生命组件：拥有者 [%s] 上没有 Ability 扩展组件，生命属性不会被监听。"), *GetNameSafe(Owner));
	}
}

void UYanHealthComponent::OnUnregister()
{
	UninitializeFromAbilitySystem();

	Super::OnUnregister();
}

void UYanHealthComponent::HandleAbilitySystemInitialized()
{
	const AActor* Owner = GetOwner();
	const UModularAbilityExtensionComponent* AbilityExtension = Owner ? Owner->FindComponentByClass<UModularAbilityExtensionComponent>() : nullptr;
	InitializeWithAbilitySystem(AbilityExtension ? AbilityExtension->GetModularAbilitySystemComponent() : nullptr);
}

void UYanHealthComponent::HandleAbilitySystemUninitialized()
{
	UninitializeFromAbilitySystem();
}

void UYanHealthComponent::InitializeWithAbilitySystem(UModularAbilitySystemComponent* InASC)
{
	const AActor* Owner = GetOwner();
	check(Owner);

	if (AbilitySystemComponent)
	{
		UE_LOG(LogModularGameplayAbilities, Error, TEXT("生命组件：拥有者 [%s] 的生命组件已完成能力系统绑定，不可重复绑定。"), *GetNameSafe(Owner));
		return;
	}

	AbilitySystemComponent = InASC;
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogModularGameplayAbilities, Error, TEXT("生命组件：拥有者 [%s] 传入的能力系统为空，无法完成绑定。"), *GetNameSafe(Owner));
		return;
	}

	HealthSet = AbilitySystemComponent->GetSet<UYanHealthSet>();
	if (!HealthSet)
	{
		UE_LOG(LogModularGameplayAbilities, Error, TEXT("生命组件：拥有者 [%s] 的能力系统上没有生命属性集，无法完成绑定。"), *GetNameSafe(Owner));
		return;
	}

	HealthSet->OnHealthChanged.AddUObject(this, &ThisClass::HandleHealthChanged);
	HealthSet->OnMaxHealthChanged.AddUObject(this, &ThisClass::HandleMaxHealthChanged);
	HealthSet->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);

	// 出生时把生命补满
	AbilitySystemComponent->SetNumericAttributeBase(UYanHealthSet::GetHealthAttribute(), HealthSet->GetMaxHealth());

	ClearGameplayTags();

	OnHealthChanged.Broadcast(this, HealthSet->GetHealth(), HealthSet->GetHealth(), nullptr);
	OnMaxHealthChanged.Broadcast(this, HealthSet->GetHealth(), HealthSet->GetHealth(), nullptr);
}

void UYanHealthComponent::UninitializeFromAbilitySystem()
{
	ClearGameplayTags();

	if (HealthSet)
	{
		HealthSet->OnHealthChanged.RemoveAll(this);
		HealthSet->OnMaxHealthChanged.RemoveAll(this);
		HealthSet->OnOutOfHealth.RemoveAll(this);
	}

	HealthSet = nullptr;
	AbilitySystemComponent = nullptr;
}

void UYanHealthComponent::ClearGameplayTags()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(ModularAbilityTags::Status_Death_Dying, 0);
		AbilitySystemComponent->SetLooseGameplayTagCount(ModularAbilityTags::Status_Death_Dead, 0);
	}
}

float UYanHealthComponent::GetHealth() const
{
	return (HealthSet ? HealthSet->GetHealth() : 0.0f);
}

float UYanHealthComponent::GetMaxHealth() const
{
	return (HealthSet ? HealthSet->GetMaxHealth() : 0.0f);
}

float UYanHealthComponent::GetHealthNormalized() const
{
	if (HealthSet)
	{
		const float Health = HealthSet->GetHealth();
		const float MaxHealth = HealthSet->GetMaxHealth();

		return ((MaxHealth > 0.0f) ? (Health / MaxHealth) : 0.0f);
	}

	return 0.0f;
}

void UYanHealthComponent::HandleHealthChanged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue)
{
	OnHealthChanged.Broadcast(this, OldValue, NewValue, DamageInstigator);
}

void UYanHealthComponent::HandleMaxHealthChanged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue)
{
	OnMaxHealthChanged.Broadcast(this, OldValue, NewValue, DamageInstigator);
}

void UYanHealthComponent::HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue)
{
#if WITH_SERVER_CODE
	if (AbilitySystemComponent && DamageEffectSpec)
	{
		// 抛出死亡事件，由死亡技能接手后续流程
		{
			FGameplayEventData Payload;
			Payload.EventTag = GameplayEvent_Death;
			Payload.Instigator = DamageInstigator;
			Payload.Target = AbilitySystemComponent->GetAvatarActor();
			Payload.OptionalObject = DamageEffectSpec->Def;
			Payload.ContextHandle = DamageEffectSpec->GetEffectContext();
			Payload.InstigatorTags = *DamageEffectSpec->CapturedSourceTags.GetAggregatedTags();
			Payload.TargetTags = *DamageEffectSpec->CapturedTargetTags.GetAggregatedTags();
			Payload.EventMagnitude = DamageMagnitude;

			FScopedPredictionWindow NewScopedWindow(AbilitySystemComponent, true);
			AbilitySystemComponent->HandleGameplayEvent(Payload.EventTag, &Payload);
		}

		// 广播击杀消息，供计分、播报与连杀统计等系统消费
		{
			FModularVerbMessage Message;
			Message.Verb = YanHealthTags::TAG_Message_Elimination;
			Message.Instigator = DamageInstigator;
			Message.InstigatorTags = *DamageEffectSpec->CapturedSourceTags.GetAggregatedTags();
			Message.Target = UVerbMessageHelpers::GetPlayerStateFromObject(AbilitySystemComponent->GetAvatarActor());
			Message.TargetTags = *DamageEffectSpec->CapturedTargetTags.GetAggregatedTags();

			UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(GetWorld());
			MessageSystem.BroadcastMessage(Message.Verb, Message);
		}
	}
#endif // WITH_SERVER_CODE
}

void UYanHealthComponent::OnRep_DeathState(EYanDeathState OldDeathState)
{
	const EYanDeathState NewDeathState = DeathState;

	// 先退回旧值，实际推进仍由 StartDeath / FinishDeath 完成，以复用其副作用
	DeathState = OldDeathState;

	if (OldDeathState > NewDeathState)
	{
		UE_LOG(LogModularGameplayAbilities, Warning, TEXT("生命组件：本地预测已越过服务端死亡阶段 [%d] -> [%d]，拥有者 [%s]。"), (uint8)OldDeathState, (uint8)NewDeathState, *GetNameSafe(GetOwner()));
		return;
	}

	if (OldDeathState == EYanDeathState::NotDead)
	{
		if (NewDeathState == EYanDeathState::DeathStarted)
		{
			StartDeath();
		}
		else if (NewDeathState == EYanDeathState::DeathFinished)
		{
			StartDeath();
			FinishDeath();
		}
		else
		{
			UE_LOG(LogModularGameplayAbilities, Error, TEXT("生命组件：非法的死亡阶段跃迁 [%d] -> [%d]，拥有者 [%s]。"), (uint8)OldDeathState, (uint8)NewDeathState, *GetNameSafe(GetOwner()));
		}
	}
	else if (OldDeathState == EYanDeathState::DeathStarted)
	{
		if (NewDeathState == EYanDeathState::DeathFinished)
		{
			FinishDeath();
		}
		else
		{
			UE_LOG(LogModularGameplayAbilities, Error, TEXT("生命组件：非法的死亡阶段跃迁 [%d] -> [%d]，拥有者 [%s]。"), (uint8)OldDeathState, (uint8)NewDeathState, *GetNameSafe(GetOwner()));
		}
	}

	ensureMsgf((DeathState == NewDeathState), TEXT("生命组件：死亡阶段推进失败 [%d] -> [%d]，拥有者 [%s]。"), (uint8)OldDeathState, (uint8)NewDeathState, *GetNameSafe(GetOwner()));
}

void UYanHealthComponent::StartDeath()
{
	if (DeathState != EYanDeathState::NotDead)
	{
		return;
	}

	DeathState = EYanDeathState::DeathStarted;

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(ModularAbilityTags::Status_Death_Dying, 1);
	}

	AActor* Owner = GetOwner();
	check(Owner);

	OnDeathStarted.Broadcast(Owner);

	Owner->ForceNetUpdate();
}

void UYanHealthComponent::FinishDeath()
{
	if (DeathState != EYanDeathState::DeathStarted)
	{
		return;
	}

	DeathState = EYanDeathState::DeathFinished;

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(ModularAbilityTags::Status_Death_Dead, 1);
	}

	AActor* Owner = GetOwner();
	check(Owner);

	OnDeathFinished.Broadcast(Owner);

	Owner->ForceNetUpdate();
}

void UYanHealthComponent::DamageSelfDestruct(bool bFellOutOfWorld)
{
	if ((DeathState != EYanDeathState::NotDead) || !AbilitySystemComponent)
	{
		return;
	}

	const TSubclassOf<UGameplayEffect> DamageGE = UModularAbilityData::Get().DamageGameplayEffect_SetByCaller.LoadSynchronous();
	if (!DamageGE)
	{
		UE_LOG(LogModularGameplayAbilities, Error, TEXT("生命组件：拥有者 [%s] 的自毁失败，未能加载伤害 GE。"), *GetNameSafe(GetOwner()));
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(DamageGE, 1.0f, AbilitySystemComponent->MakeEffectContext());
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();

	if (!Spec)
	{
		UE_LOG(LogModularGameplayAbilities, Error, TEXT("生命组件：拥有者 [%s] 的自毁失败，未能生成 [%s] 的 Spec。"), *GetNameSafe(GetOwner()), *GetNameSafe(DamageGE));
		return;
	}

	Spec->AddDynamicAssetTag(YanHealthTags::TAG_Gameplay_DamageSelfDestruct);

	if (bFellOutOfWorld)
	{
		Spec->AddDynamicAssetTag(YanHealthTags::TAG_Gameplay_FellOutOfWorld);
	}

	Spec->SetSetByCallerMagnitude(YanHealthTags::TAG_SetByCaller_Damage, GetMaxHealth());
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec);
}
