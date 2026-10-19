#include "Abilities/Ability_BlueAttractOrb.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Ability_ColorShot.h"
#include "Actors/ColorAttractOrb.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Ability_BlueAttractOrb)

UAbility_BlueAttractOrb::UAbility_BlueAttractOrb(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OrbSocket = TEXT("weapon_r");
	OrbClass = AColorAttractOrb::StaticClass();
}

void UAbility_BlueAttractOrb::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 蓄力阶段就订阅，球在手上被引爆时同样结束技能
	if (AColorAttractOrb* AttractOrb = Cast<AColorAttractOrb>(Orb))
	{
		AttractOrb->OnControlFinished.AddUObject(this, &ThisClass::HandleControlFinished);
	}
}

void UAbility_BlueAttractOrb::OnChargeReleased(float ChargeSeconds)
{
	// 操控阶段左键用于推球，不能同时发射颜色射线
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(Status_ColorShotBlocked);
		bBlockingColorShot = true;
	}

	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	AColorAttractOrb* AttractOrb = Cast<AColorAttractOrb>(Orb);
	if (!AttractOrb)
	{
		Super::OnChargeReleased(ChargeSeconds);
		return;
	}

	Orb = nullptr;
	LaunchedOrb = AttractOrb;
	AttractOrb->AuthLaunch(FMath::Lerp(ControlDurationByCharge.X, ControlDurationByCharge.Y, GetChargeAlpha(ChargeSeconds)));
}

void UAbility_BlueAttractOrb::HandleControlFinished()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
	}
}

void UAbility_BlueAttractOrb::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		// 未脱手的球由基类销毁，同样要先解绑
		if (AColorAttractOrb* HeldOrb = Cast<AColorAttractOrb>(Orb))
		{
			HeldOrb->OnControlFinished.RemoveAll(this);
		}

		if (AColorAttractOrb* AttractOrb = LaunchedOrb.Get())
		{
			// 先解绑，销毁时的结束广播不能再回调到正在结束的技能
			AttractOrb->OnControlFinished.RemoveAll(this);
			AttractOrb->Destroy();
		}
		LaunchedOrb.Reset();

		if (bBlockingColorShot)
		{
			bBlockingColorShot = false;
			if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
			{
				ASC->RemoveLooseGameplayTag(Status_ColorShotBlocked);
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
