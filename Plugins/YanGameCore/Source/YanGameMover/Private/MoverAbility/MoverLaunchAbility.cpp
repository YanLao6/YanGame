#include "MoverAbility/MoverLaunchAbility.h"

#include "MoverAbility/YanLaunchTargetData.h"
#include "Actor/YanHookProjectile.h"
#include "YanMoverAngelscriptLibrary.h"
#include "AbilitySystemComponent.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverLaunchAbility)

UMoverLaunchAbility::UMoverLaunchAbility(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	// 本地预测：本地受控端 spawn cosmetic 投射物探测命中，经 TargetData 回传服务器施加击飞。
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UMoverLaunchAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	PendingProjectile = nullptr;

	UAbilitySystemComponent* ASC       = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	APawn*                   OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());

	if (!ASC || !OwnerPawn || !ProjectileClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// 服务器：注册委托等待本地受控端回传命中数据；若数据已先到则立即触发
	if (ActorInfo->IsNetAuthority())
	{
		TargetDataDelegateHandle = ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
		                               .AddUObject(this, &UMoverLaunchAbility::OnLaunchTargetDataReceived);
		ASC->CallReplicatedTargetDataDelegatesIfSet(Handle, ActivationInfo.GetActivationPredictionKey());
	}

	// 本地受控端：从 Pawn 视点 spawn cosmetic 投射物做命中探测
	if (ActorInfo->IsLocallyControlled())
	{
		UMoverComponent* MoverComp = OwnerPawn->FindComponentByClass<UMoverComponent>();

		// 方向取上传服务器的输入 ControlRotation（含完整 Pitch），原点取 Pawn 视点，两端口径一致
		const FCharacterDefaultInputs* CharInputs  = MoverComp ? MoverComp->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>() : nullptr;
		const FVector                  AimLocation = OwnerPawn->GetPawnViewLocation();
		const FRotator                 AimRotation = CharInputs ? CharInputs->ControlRotation : OwnerPawn->GetActorRotation();

		AYanHookProjectile* Projectile = UYanMoverAngelscriptLibrary::SpawnHookProjectile(MoverComp, ProjectileClass, AimLocation, AimRotation);
		if (IsValid(Projectile))
		{
			PendingProjectile = Projectile;
			Projectile->OnHookHit.AddDynamic(this, &UMoverLaunchAbility::HandleProjectileHit);
			Projectile->OnHookMissed.AddDynamic(this, &UMoverLaunchAbility::HandleProjectileMissed);
			Projectile->Launch(AimRotation.Vector(), ProjectileSpeed);
		}
		else if (!ActorInfo->IsNetAuthority())
		{
			// 纯客户端 spawn 失败：无可等待，直接结束（服务器侧已挂委托者由其超时/取消处理）
			EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		}
	}
}

void UMoverLaunchAbility::HandleProjectileHit(AYanHookProjectile* Projectile, const FHitResult& Hit)
{
	UAbilitySystemComponent* ASC     = GetAbilitySystemComponentFromActorInfo();
	APawn*                   HitPawn = Cast<APawn>(Hit.GetActor());

	if (ASC && HitPawn && IsValid(Projectile))
	{
		// 击飞方向：投射物前向与 Up 按 UpwardRatio 插值后归一化
		const FVector LaunchDir = (Projectile->GetActorForwardVector() + FVector::UpVector * UpwardRatio).GetSafeNormal();

		// 命中数据交由 GAS 内建预测性 TargetData RPC 回传服务器（句柄持有堆分配，由其管理生命周期）
		FYanLaunchTargetData* TargetData = new FYanLaunchTargetData();
		TargetData->HitResult            = Hit;
		TargetData->LaunchVelocity       = LaunchDir * LaunchSpeed;
		TargetData->DurationMs           = LaunchDurationMs;

		FGameplayAbilityTargetDataHandle DataHandle;
		DataHandle.Add(TargetData);

		FScopedPredictionWindow ScopedPrediction(ASC, IsPredictingClient());
		ASC->ServerSetReplicatedTargetData(GetCurrentAbilitySpecHandle(),
		                                   GetCurrentActivationInfo().GetActivationPredictionKey(),
		                                   DataHandle,
		                                   FGameplayTag(),
		                                   ASC->ScopedPredictionKey);
	}

	// 命中非 Pawn 或命中 Pawn 回传完毕：本地受控端使命结束
	CleanupAndEnd();
}

void UMoverLaunchAbility::HandleProjectileMissed(AYanHookProjectile* Projectile)
{
	CleanupAndEnd();
}

void UMoverLaunchAbility::OnLaunchTargetDataReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ActivationTag)
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->ConsumeClientReplicatedTargetData(GetCurrentAbilitySpecHandle(), GetCurrentActivationInfo().GetActivationPredictionKey());
	}

	ApplyLaunchFromTargetData(Data);
	CleanupAndEnd();
}

void UMoverLaunchAbility::ApplyLaunchFromTargetData(const FGameplayAbilityTargetDataHandle& Data) const
{
	for (int32 Index = 0; Index < Data.Num(); ++Index)
	{
		const FGameplayAbilityTargetData* Base = Data.Get(Index);
		// 仅处理本技能约定的命中数据类型，规避异常或伪造的结构
		if (!Base || Base->GetScriptStruct() != FYanLaunchTargetData::StaticStruct())
		{
			continue;
		}

		const FYanLaunchTargetData* Launch    = static_cast<const FYanLaunchTargetData*>(Base);
		const FHitResult*           HitResult = Launch->GetHitResult();
		APawn*                      HitPawn   = HitResult ? Cast<APawn>(HitResult->GetActor()) : nullptr;
		if (!HitPawn)
		{
			continue;
		}

		// 服务器权威施加击飞；如需加固可在此校验射程/朝向/目标合法性
		if (UMoverComponent* TargetMover = HitPawn->FindComponentByClass<UMoverComponent>())
		{
			UYanMoverAngelscriptLibrary::QueueLaunchMove(TargetMover, Launch->LaunchVelocity, FName("Falling"), Launch->DurationMs);
		}
	}
}

void UMoverLaunchAbility::CleanupAndEnd()
{
	if (IsValid(PendingProjectile))
	{
		PendingProjectile->Destroy();
	}
	PendingProjectile = nullptr;

	K2_EndAbility();
}

void UMoverLaunchAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 反注册服务器侧 TargetData 委托，避免悬挂绑定
	if (ActorInfo && ActorInfo->IsNetAuthority() && TargetDataDelegateHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).Remove(TargetDataDelegateHandle);
		}
		TargetDataDelegateHandle.Reset();
	}

	if (IsValid(PendingProjectile))
	{
		PendingProjectile->Destroy();
		PendingProjectile = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
