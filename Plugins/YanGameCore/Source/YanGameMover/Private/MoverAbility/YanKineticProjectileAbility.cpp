#include "MoverAbility/YanKineticProjectileAbility.h"

#include "Actor/YanKineticSummon.h"
#include "YanMoverAngelscriptLibrary.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Curves/CurveFloat.h"
#include "MoverComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanKineticProjectileAbility)

UYanKineticProjectileAbility::UYanKineticProjectileAbility(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	// 生成与施力均为服务器权威；本地仅预测蓄力与施法表现
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 蓄力横跨多帧，须为每次激活保留独立的实例状态
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UYanKineticProjectileAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	if (ActorInfo && ActorInfo->IsLocallyControlled())
	{
		PlayChargeFeedback();
		OnChargeStarted();
	}

	// 两端并行等待：客户端把「松手」这一事实复制上来，时长各自计量，
	// 服务器因而不必采信客户端报上来的蓄力量
	UAbilityTask_WaitInputRelease* WaitRelease = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased=*/true);
	if (!WaitRelease)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	WaitRelease->OnRelease.AddDynamic(this, &UYanKineticProjectileAbility::HandleInputReleased);
	WaitRelease->ReadyForActivation();
}

void UYanKineticProjectileAbility::HandleInputReleased(float TimeHeld)
{
	const float ChargeRatio = ComputeChargeRatio(TimeHeld);

	// 点一下就松手多为误触，不该扔出一发威力最小的召唤物
	const bool bLaunched = ChargeRatio >= MinChargeToLaunch;

	if (IsLocallyControlled())
	{
		if (bLaunched)
		{
			OnProjectileLaunched(ChargeRatio);
		}
		else
		{
			OnChargeAborted();
		}
	}

	if (bLaunched && HasAuthority(&CurrentActivationInfo))
	{
		SpawnAndLaunch(ChargeRatio);
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

float UYanKineticProjectileAbility::ComputeChargeRatio(float TimeHeld) const
{
	const float RawRatio = FMath::Clamp(TimeHeld / FMath::Max(FullChargeSeconds, UE_KINDA_SMALL_NUMBER), 0.f, 1.f);

	if (!ChargeResponseCurve)
	{
		return RawRatio;
	}

	return FMath::Clamp(ChargeResponseCurve->GetFloatValue(RawRatio), 0.f, 1.f);
}

void UYanKineticProjectileAbility::SpawnAndLaunch(float ChargeRatio) const
{
	APawn*  OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	UWorld* World     = OwnerPawn ? OwnerPawn->GetWorld() : nullptr;

	if (!World || !SummonClass)
	{
		return;
	}

	// 视点取自上传服务器的输入包，客户端与服务器口径一致，不依赖纯客户端的摄像机状态
	UMoverComponent* MoverComp    = OwnerPawn->FindComponentByClass<UMoverComponent>();
	FVector          ViewLocation = FVector::ZeroVector;
	FVector          ViewForward  = FVector::ZeroVector;

	if (!UYanMoverAngelscriptLibrary::GetOwnerViewLocationAndForward(MoverComp, ViewLocation, ViewForward))
	{
		ViewLocation = OwnerPawn->GetPawnViewLocation();
		ViewForward  = OwnerPawn->GetActorRotation().Vector();
	}

	const float   LaunchSpeed     = FMath::Lerp(MinLaunchSpeed, MaxLaunchSpeed, ChargeRatio);
	const float   EffectMagnitude = FMath::Lerp(MinEffectMagnitude, MaxEffectMagnitude, ChargeRatio);
	const FVector SpawnLocation   = ViewLocation + ViewForward * SpawnForwardOffset;
	const FVector LaunchVelocity  = ViewForward * LaunchSpeed;

	if (LaunchVelocity.IsNearlyZero())
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner      = OwnerPawn;
	SpawnParams.Instigator = OwnerPawn;

	// 出生点贴着施法者，按默认策略会因重叠被顶开或直接取消生成
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (AYanKineticSummon* Summon = World->SpawnActor<AYanKineticSummon>(SummonClass, SpawnLocation, LaunchVelocity.Rotation(), SpawnParams))
	{
		Summon->Launch(LaunchVelocity, EffectMagnitude);
	}
}

void UYanKineticProjectileAbility::PlayChargeFeedback() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!OwnerPawn)
	{
		return;
	}

	if (ChargeSound)
	{
		UGameplayStatics::PlaySoundAtLocation(OwnerPawn, ChargeSound, OwnerPawn->GetActorLocation());
	}

	if (ChargeMontage)
	{
		if (const USkeletalMeshComponent* Mesh = OwnerPawn->FindComponentByClass<USkeletalMeshComponent>())
		{
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
			{
				AnimInstance->Montage_Play(ChargeMontage);
			}
		}
	}
}
