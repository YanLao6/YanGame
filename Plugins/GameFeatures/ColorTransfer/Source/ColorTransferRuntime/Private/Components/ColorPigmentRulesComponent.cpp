#include "Components/ColorPigmentRulesComponent.h"

#include "AbilitySystemComponent.h"
#include "Attributes/ColorPigmentSet.h"
#include "Effects/ColorPigmentEffects.h"
#include "ColorTransferQueries.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Messages/VerbMessageHelpers.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Player/YanDeathAbility.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorPigmentRulesComponent)

// 同名标签登记在 DefaultGameplayTags.ini 中，GA_Sprint 的脚本在本模块加载前就要引用它
UE_DEFINE_GAMEPLAY_TAG_STATIC(Status_SprintLocked, "Status.SprintLocked");

UE_DEFINE_GAMEPLAY_TAG(Status_PurpleCharging, "Status.PurpleCharging");

UColorPigmentRulesComponent::UColorPigmentRulesComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UColorPigmentRulesComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() != NM_DedicatedServer)
	{
		// 提前加载并持有，避免在爆炸那一帧同步加载卡顿
		LoadedExplosionEffect = PurpleExplosionEffect.LoadSynchronous();
	}
}

void UColorPigmentRulesComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UColorPigmentSet* PigmentSet = BoundPigmentSet.Get())
	{
		PigmentSet->OnPigmentChanged.RemoveAll(this);
	}

	GetWorld()->GetTimerManager().ClearAllTimersForObject(this);

	Super::EndPlay(EndPlayReason);
}

void UColorPigmentRulesComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UColorPigmentRulesComponent, PurpleStartTime);
}

void UColorPigmentRulesComponent::AuthResetRules()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	bDeathRequested = false;
	SetPurpleCharging(false);

	if (!BoundPigmentSet.IsValid())
	{
		if (UColorPigmentSet* PigmentSet = UColorPigmentSet::FindForActor(GetOwner()))
		{
			PigmentSet->OnPigmentChanged.AddUObject(this, &ThisClass::EvaluateRules);
			BoundPigmentSet = PigmentSet;
		}
	}

	EvaluateRules();
}

void UColorPigmentRulesComponent::AuthRequestDeath()
{
	if (!GetOwner()->HasAuthority() || bDeathRequested)
	{
		return;
	}

	RequestDeath();
}

bool UColorPigmentRulesComponent::AuthCancelPurple()
{
	if (!GetOwner()->HasAuthority() || bDeathRequested || PurpleStartTime < 0.0)
	{
		return false;
	}

	SetPurpleCharging(false);

	// 先清状态再排空：排空会广播颜料变化并重新裁决，此时已不在紫爆中
	if (UColorPigmentSet* PigmentSet = UColorPigmentSet::FindForActor(GetOwner()))
	{
		ColorTransfer::AuthApplyReset(*PigmentSet, PigmentSet->GetOwnPigment());
	}

	return true;
}

float UColorPigmentRulesComponent::GetPurpleAlpha() const
{
	if (PurpleStartTime < 0.0)
	{
		return 0.0f;
	}

	const double Now = ColorTransfer::GetServerTime(GetWorld());
	return FMath::Clamp(static_cast<float>((Now - PurpleStartTime) / PurpleDuration), 0.0f, 1.0f);
}

void UColorPigmentRulesComponent::EvaluateRules()
{
	const UColorPigmentSet* PigmentSet = UColorPigmentSet::FindForActor(GetOwner());
	// 出生前 OwnPigment 为 None，不做裁决
	if (bDeathRequested || !PigmentSet || PigmentSet->GetOwnPigment() == EColorPigment::None)
	{
		return;
	}

	const float OwnAmount = PigmentSet->GetOwnPigmentAmount();

	UAbilitySystemComponent* ASC = PigmentSet->GetOwningAbilitySystemComponent();
	ASC->SetLooseGameplayTagCount(Status_SprintLocked, OwnAmount < SprintLockThreshold ? 1 : 0, EGameplayTagReplicationState::TagOnly);

	if (OwnAmount < CollapseThreshold)
	{
		RequestDeath();
		return;
	}

	// 一旦开始变紫就不再撤销，期间颜料再变化也照样自爆
	const float Red = PigmentSet->GetRed();
	if (PurpleStartTime < 0.0 && Red > PurpleTolerance && FMath::IsNearlyEqual(Red, PigmentSet->GetBlue(), PurpleTolerance))
	{
		SetPurpleCharging(true);
	}
}

void UColorPigmentRulesComponent::SetPurpleCharging(bool bCharging)
{
	UWorld* World = GetWorld();
	if (bCharging)
	{
		PurpleStartTime = ColorTransfer::GetServerTime(World);
		World->GetTimerManager().SetTimer(PurpleTimer, this, &ThisClass::ExplodePurple, PurpleDuration, false);
	}
	else
	{
		PurpleStartTime = -1.0;
		World->GetTimerManager().ClearTimer(PurpleTimer);
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->SetLooseGameplayTagCount(Status_PurpleCharging, bCharging ? 1 : 0, EGameplayTagReplicationState::TagOnly);
	}
}

void UColorPigmentRulesComponent::ExplodePurple()
{
	if (bDeathRequested)
	{
		return;
	}

	if (const APawn* Pawn = GetPawn<APawn>())
	{
		MulticastPurpleExplosion(Pawn->GetActorLocation());
	}

	RequestDeath();
}

void UColorPigmentRulesComponent::RequestDeath()
{
	bDeathRequested = true;
	SetPurpleCharging(false);

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = GameplayEvent_Death;
	Payload.Instigator = GetOwner();
	Payload.Target = GetPawn<APawn>();
	ASC->HandleGameplayEvent(GameplayEvent_Death, &Payload);
}

void UColorPigmentRulesComponent::MulticastPurpleExplosion_Implementation(FVector_NetQuantize Location)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (LoadedExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, LoadedExplosionEffect, Location);
	}
}

UAbilitySystemComponent* UColorPigmentRulesComponent::GetAbilitySystemComponent() const
{
	return UVerbMessageHelpers::GetAbilitySystemComponentFromObject(GetOwner());
}
