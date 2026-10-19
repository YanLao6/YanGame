// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapon/Abilities/GameplayAbility_RangedWeapon.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "DrawDebugHelpers.h"
#include "ModularGameplayAbilitiesLogChannels.h"
#include "NativeGameplayTags.h"
#include "Weapon/RangedWeaponInstance.h"
#include "Weapon/WeaponAbilityTargetData.h"
#include "Weapon/WeaponChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayAbility_RangedWeapon)

namespace RangedWeaponConsoleVariables
{
	static float DrawBulletTracesDuration = 0.0f;
	static FAutoConsoleVariableRef CVarDrawBulletTraceDuration(
		TEXT("RangedWeapon.DrawBulletTraceDuration"),
		DrawBulletTracesDuration,
		TEXT("绘制子弹弹道调试线的持续时间，单位为秒；大于零时启用。"),
		ECVF_Default);

	static float DrawBulletHitDuration = 0.0f;
	static FAutoConsoleVariableRef CVarDrawBulletHits(
		TEXT("RangedWeapon.DrawBulletHitDuration"),
		DrawBulletHitDuration,
		TEXT("绘制子弹命中点调试图元的持续时间，单位为秒；大于零时启用。"),
		ECVF_Default);

	static float DrawBulletHitRadius = 3.0f;
	static FAutoConsoleVariableRef CVarDrawBulletHitRadius(
		TEXT("RangedWeapon.DrawBulletHitRadius"),
		DrawBulletHitRadius,
		TEXT("命中点调试图元的半径，单位为厘米。"),
		ECVF_Default);
}

// 拥有该标签时武器开火会被阻止或打断
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WeaponFireBlocked, "Ability.Weapon.NoFiring");

//////////////////////////////////////////////////////////////////////

// 在锥形范围内按指数分布采样一个方向；Exponent 越大弹着越向中心聚拢
static FVector VRandConeNormalDistribution(const FVector& Dir, const float ConeHalfAngleRad, const float Exponent)
{
	if (ConeHalfAngleRad > 0.f)
	{
		const float ConeHalfAngleDegrees = FMath::RadiansToDegrees(ConeHalfAngleRad);

		// 把锥形采样拆成两次旋转：一次偏离中心轴，一次绕中心轴旋转。
		// 指数只作用于偏离量，因此可以在不改变锥角的前提下控制弹着聚拢程度。
		const float FromCenter = FMath::Pow(FMath::FRand(), Exponent);
		const float AngleFromCenter = FromCenter * ConeHalfAngleDegrees;
		const float AngleAround = FMath::FRand() * 360.0f;

		FRotator Rot = Dir.Rotation();
		FQuat DirQuat(Rot);
		FQuat FromCenterQuat(FRotator(0.0f, AngleFromCenter, 0.0f));
		FQuat AroundQuat(FRotator(0.0f, 0.0, AngleAround));
		FQuat FinalDirectionQuat = DirQuat * AroundQuat * FromCenterQuat;
		FinalDirectionQuat.Normalize();

		return FinalDirectionQuat.RotateVector(FVector::ForwardVector);
	}
	else
	{
		return Dir.GetSafeNormal();
	}
}

UGameplayAbility_RangedWeapon::UGameplayAbility_RangedWeapon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SourceBlockedTags.AddTag(TAG_WeaponFireBlocked);
}

URangedWeaponInstance* UGameplayAbility_RangedWeapon::GetWeaponInstance() const
{
	return Cast<URangedWeaponInstance>(GetAssociatedEquipment());
}

bool UGameplayAbility_RangedWeapon::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	bool bResult = Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);

	if (bResult)
	{
		if (GetWeaponInstance() == nullptr)
		{
			UE_LOG(LogModularGameplayAbilities, Error, TEXT("武器能力 %s 无法激活：没有关联的远程武器实例（当前装备实例为 %s，需要派生自 %s）"),
				*GetPathName(),
				*GetPathNameSafe(GetAssociatedEquipment()),
				*URangedWeaponInstance::StaticClass()->GetName());
			bResult = false;
		}
	}

	return bResult;
}

int32 UGameplayAbility_RangedWeapon::FindFirstPawnHitResult(const TArray<FHitResult>& HitResults)
{
	for (int32 Idx = 0; Idx < HitResults.Num(); ++Idx)
	{
		const FHitResult& CurHitResult = HitResults[Idx];
		if (CurHitResult.HitObjectHandle.DoesRepresentClass(APawn::StaticClass()))
		{
			return Idx;
		}
		else
		{
			// 命中挂在 Pawn 身上的组件或 Actor 同样视为命中该 Pawn
			AActor* HitActor = CurHitResult.HitObjectHandle.FetchActor();
			if ((HitActor != nullptr) && (HitActor->GetAttachParentActor() != nullptr) && (Cast<APawn>(HitActor->GetAttachParentActor()) != nullptr))
			{
				return Idx;
			}
		}
	}

	return INDEX_NONE;
}

void UGameplayAbility_RangedWeapon::AddAdditionalTraceIgnoreActors(FCollisionQueryParams& TraceParams) const
{
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		// 忽略挂在射击者身上的所有 Actor，避免打到自己的武器或配件
		TArray<AActor*> AttachedActors;
		Avatar->GetAttachedActors(/*输出*/ AttachedActors);
		TraceParams.AddIgnoredActors(AttachedActors);
	}
}

ECollisionChannel UGameplayAbility_RangedWeapon::DetermineTraceChannel(FCollisionQueryParams& TraceParams, bool bIsSimulated) const
{
	return EquipManager_TraceChannel_Weapon;
}

FHitResult UGameplayAbility_RangedWeapon::WeaponTrace(const FVector& StartTrace, const FVector& EndTrace, float SweepRadius, bool bIsSimulated, OUT TArray<FHitResult>& OutHitResults) const
{
	TArray<FHitResult> HitResults;

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(WeaponTrace), /*bTraceComplex=*/ true, /*IgnoreActor=*/ GetAvatarActorFromActorInfo());
	TraceParams.bReturnPhysicalMaterial = true;
	AddAdditionalTraceIgnoreActors(TraceParams);

	const ECollisionChannel TraceChannel = DetermineTraceChannel(TraceParams, bIsSimulated);

	if (SweepRadius > 0.0f)
	{
		GetWorld()->SweepMultiByChannel(HitResults, StartTrace, EndTrace, FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(SweepRadius), TraceParams);
	}
	else
	{
		GetWorld()->LineTraceMultiByChannel(HitResults, StartTrace, EndTrace, TraceChannel, TraceParams);
	}

	FHitResult Hit(ForceInit);
	if (HitResults.Num() > 0)
	{
		// 按 Actor 去重，避免一颗子弹对同一目标重复结算伤害
		for (FHitResult& CurHitResult : HitResults)
		{
			auto Pred = [&CurHitResult](const FHitResult& Other)
			{
				return Other.HitObjectHandle == CurHitResult.HitObjectHandle;
			};

			if (!OutHitResults.ContainsByPredicate(Pred))
			{
				OutHitResults.Add(CurHitResult);
			}
		}

		Hit = OutHitResults.Last();
	}
	else
	{
		Hit.TraceStart = StartTrace;
		Hit.TraceEnd = EndTrace;
	}

	return Hit;
}

FVector UGameplayAbility_RangedWeapon::GetWeaponTargetingSourceLocation() const
{
	// 以 Pawn 位置作为基准
	APawn* const AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	check(AvatarPawn);

	const FVector TargetingSourceLocation = AvatarPawn->GetActorLocation();

	return TargetingSourceLocation;
}

FTransform UGameplayAbility_RangedWeapon::GetTargetingTransform(APawn* SourcePawn, EWeaponTargetingSource Source) const
{
	check(SourcePawn);

	// Custom 模式的位置完全由调用方决定，不应进入此函数
	check(Source != EWeaponTargetingSource::Custom);

	const FVector ActorLoc = SourcePawn->GetActorLocation();
	FQuat AimQuat = SourcePawn->GetActorQuat();
	AController* Controller = SourcePawn->GetController();
	FVector SourceLoc;

	double FocalDistance = 1024.0f;
	FVector FocalLoc;

	FVector CamLoc;
	FRotator CamRot;
	bool bFoundFocus = false;

	if ((Controller != nullptr) && ((Source == EWeaponTargetingSource::CameraTowardsFocus) || (Source == EWeaponTargetingSource::PawnTowardsFocus) || (Source == EWeaponTargetingSource::WeaponTowardsFocus)))
	{
		bFoundFocus = true;

		APlayerController* PC = Cast<APlayerController>(Controller);
		if (PC != nullptr)
		{
			PC->GetPlayerViewPoint(/*输出*/ CamLoc, /*输出*/ CamRot);
		}
		else
		{
			SourceLoc = GetWeaponTargetingSourceLocation();
			CamLoc = SourceLoc;
			CamRot = Controller->GetControlRotation();
		}

		FVector AimDir = CamRot.Vector().GetSafeNormal();
		FocalLoc = CamLoc + (AimDir * FocalDistance);

		// 把起点和焦点沿视线推到 Pawn 前方，避免射线从角色背后穿过身体
		if (PC)
		{
			const FVector WeaponLoc = GetWeaponTargetingSourceLocation();
			CamLoc = FocalLoc + (((WeaponLoc - FocalLoc) | AimDir) * AimDir);
			FocalLoc = CamLoc + (AimDir * FocalDistance);
		}
		// AI 没有相机，改用头部高度作为起点
		else if (AAIController* AIController = Cast<AAIController>(Controller))
		{
			CamLoc = SourcePawn->GetActorLocation() + FVector(0, 0, SourcePawn->BaseEyeHeight);
		}

		if (Source == EWeaponTargetingSource::CameraTowardsFocus)
		{
			return FTransform(CamRot, CamLoc);
		}
	}

	if ((Source == EWeaponTargetingSource::WeaponForward) || (Source == EWeaponTargetingSource::WeaponTowardsFocus))
	{
		SourceLoc = GetWeaponTargetingSourceLocation();
	}
	else
	{
		// 要么使用 Pawn 位置，要么是没能取到相机时的兜底
		SourceLoc = ActorLoc;
	}

	if (bFoundFocus && ((Source == EWeaponTargetingSource::PawnTowardsFocus) || (Source == EWeaponTargetingSource::WeaponTowardsFocus)))
	{
		return FTransform((FocalLoc - SourceLoc).Rotation(), SourceLoc);
	}

	// 没有相机或不使用相机时，沿 Pawn 朝向射击
	return FTransform(AimQuat, SourceLoc);
}

FHitResult UGameplayAbility_RangedWeapon::DoSingleBulletTrace(const FVector& StartTrace, const FVector& EndTrace, float SweepRadius, bool bIsSimulated, OUT TArray<FHitResult>& OutHits) const
{
#if ENABLE_DRAW_DEBUG
	if (RangedWeaponConsoleVariables::DrawBulletTracesDuration > 0.0f)
	{
		static float DebugThickness = 1.0f;
		DrawDebugLine(GetWorld(), StartTrace, EndTrace, FColor::Red, false, RangedWeaponConsoleVariables::DrawBulletTracesDuration, 0, DebugThickness);
	}
#endif // ENABLE_DRAW_DEBUG

	FHitResult Impact;

	// 先用零半径射线做精确检测
	if (FindFirstPawnHitResult(OutHits) == INDEX_NONE)
	{
		Impact = WeaponTrace(StartTrace, EndTrace, /*SweepRadius=*/ 0.0f, bIsSimulated, /*输出*/ OutHits);
	}

	if (FindFirstPawnHitResult(OutHits) == INDEX_NONE)
	{
		// 精确射线没打中 Pawn，且武器允许扫掠宽容度时再试一次
		if (SweepRadius > 0.0f)
		{
			TArray<FHitResult> SweepHits;
			Impact = WeaponTrace(StartTrace, EndTrace, SweepRadius, bIsSimulated, /*输出*/ SweepHits);

			const int32 FirstPawnIdx = FindFirstPawnHitResult(SweepHits);
			if (SweepHits.IsValidIndex(FirstPawnIdx))
			{
				// 若命中 Pawn 之前已经存在阻挡物，说明该 Pawn 在掩体之后，
				// 此时必须沿用精确射线的结果，避免扫掠半径让子弹绕过掩体
				bool bUseSweepHits = true;
				for (int32 Idx = 0; Idx < FirstPawnIdx; ++Idx)
				{
					const FHitResult& CurHitResult = SweepHits[Idx];

					auto Pred = [&CurHitResult](const FHitResult& Other)
					{
						return Other.HitObjectHandle == CurHitResult.HitObjectHandle;
					};
					if (CurHitResult.bBlockingHit && OutHits.ContainsByPredicate(Pred))
					{
						bUseSweepHits = false;
						break;
					}
				}

				if (bUseSweepHits)
				{
					OutHits = SweepHits;
				}
			}
		}
	}

	return Impact;
}

void UGameplayAbility_RangedWeapon::PerformLocalTargeting(OUT TArray<FHitResult>& OutHits)
{
	APawn* const AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());

	URangedWeaponInstance* WeaponData = GetWeaponInstance();
	if (AvatarPawn && AvatarPawn->IsLocallyControlled() && WeaponData)
	{
		FRangedWeaponFiringInput InputData;
		InputData.WeaponData = WeaponData;
		InputData.bCanPlayBulletFX = (AvatarPawn->GetNetMode() != NM_DedicatedServer);

		const FTransform TargetTransform = GetTargetingTransform(AvatarPawn, EWeaponTargetingSource::CameraTowardsFocus);
		InputData.AimDir = TargetTransform.GetUnitAxis(EAxis::X);
		InputData.StartTrace = TargetTransform.GetTranslation();

		InputData.EndAim = InputData.StartTrace + InputData.AimDir * WeaponData->GetMaxDamageRange();

#if ENABLE_DRAW_DEBUG
		if (RangedWeaponConsoleVariables::DrawBulletTracesDuration > 0.0f)
		{
			static float DebugThickness = 2.0f;
			DrawDebugLine(GetWorld(), InputData.StartTrace, InputData.StartTrace + (InputData.AimDir * 100.0f), FColor::Yellow, false, RangedWeaponConsoleVariables::DrawBulletTracesDuration, 0, DebugThickness);
		}
#endif

		TraceBulletsInCartridge(InputData, /*输出*/ OutHits);
	}
}

void UGameplayAbility_RangedWeapon::TraceBulletsInCartridge(const FRangedWeaponFiringInput& InputData, OUT TArray<FHitResult>& OutHits)
{
	URangedWeaponInstance* WeaponData = InputData.WeaponData;
	check(WeaponData);

	const int32 BulletsPerCartridge = WeaponData->GetBulletsPerCartridge();

	for (int32 BulletIndex = 0; BulletIndex < BulletsPerCartridge; ++BulletIndex)
	{
		const float BaseSpreadAngle = WeaponData->GetCalculatedSpreadAngle();
		const float SpreadAngleMultiplier = WeaponData->GetCalculatedSpreadAngleMultiplier();
		const float ActualSpreadAngle = BaseSpreadAngle * SpreadAngleMultiplier;

		const float HalfSpreadAngleInRadians = FMath::DegreesToRadians(ActualSpreadAngle * 0.5f);

		const FVector BulletDir = VRandConeNormalDistribution(InputData.AimDir, HalfSpreadAngleInRadians, WeaponData->GetSpreadExponent());

		const FVector EndTrace = InputData.StartTrace + (BulletDir * WeaponData->GetMaxDamageRange());
		FVector HitLocation = EndTrace;

		TArray<FHitResult> AllImpacts;

		FHitResult Impact = DoSingleBulletTrace(InputData.StartTrace, EndTrace, WeaponData->GetBulletTraceSweepRadius(), /*bIsSimulated=*/ false, /*输出*/ AllImpacts);

		const AActor* HitActor = Impact.GetActor();

		if (HitActor)
		{
#if ENABLE_DRAW_DEBUG
			if (RangedWeaponConsoleVariables::DrawBulletHitDuration > 0.0f)
			{
				DrawDebugPoint(GetWorld(), Impact.ImpactPoint, RangedWeaponConsoleVariables::DrawBulletHitRadius, FColor::Red, false, RangedWeaponConsoleVariables::DrawBulletHitRadius);
			}
#endif

			if (AllImpacts.Num() > 0)
			{
				OutHits.Append(AllImpacts);
			}

			HitLocation = Impact.ImpactPoint;
		}

		// 即使没有命中也要留一条记录，弹道拖尾等表现需要它提供方向
		if (OutHits.Num() == 0)
		{
			if (!Impact.bBlockingHit)
			{
				Impact.Location = EndTrace;
				Impact.ImpactPoint = EndTrace;
			}

			OutHits.Add(Impact);
		}
	}
}

void UGameplayAbility_RangedWeapon::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 绑定命中数据就绪回调
	UAbilitySystemComponent* MyAbilityComponent = CurrentActorInfo->AbilitySystemComponent.Get();
	check(MyAbilityComponent);

	OnTargetDataReadyCallbackDelegateHandle = MyAbilityComponent->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey()).AddUObject(this, &ThisClass::OnTargetDataReadyCallback);

	// 刷新开火时间，供散布恢复逻辑判断冷却延迟
	URangedWeaponInstance* WeaponData = GetWeaponInstance();
	check(WeaponData);
	WeaponData->UpdateFiringTime();

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UGameplayAbility_RangedWeapon::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		if (ScopeLockCount > 0)
		{
			WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility, Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
			return;
		}

		UAbilitySystemComponent* MyAbilityComponent = CurrentActorInfo->AbilitySystemComponent.Get();
		check(MyAbilityComponent);

		// 能力结束时消费掉残留的命中数据并解绑回调
		MyAbilityComponent->AbilityTargetDataSetDelegate(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey()).Remove(OnTargetDataReadyCallbackDelegateHandle);
		MyAbilityComponent->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());

		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
}

void UGameplayAbility_RangedWeapon::OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag)
{
	UAbilitySystemComponent* MyAbilityComponent = CurrentActorInfo->AbilitySystemComponent.Get();
	check(MyAbilityComponent);

	if (const FGameplayAbilitySpec* AbilitySpec = MyAbilityComponent->FindAbilitySpecFromHandle(CurrentSpecHandle))
	{
		FScopedPredictionWindow ScopedPrediction(MyAbilityComponent);

		// 取得数据所有权，防止回调进入游戏逻辑后原数据被销毁
		FGameplayAbilityTargetDataHandle LocalTargetDataHandle(MoveTemp(const_cast<FGameplayAbilityTargetDataHandle&>(InData)));

		const bool bShouldNotifyServer = CurrentActorInfo->IsLocallyControlled() && !CurrentActorInfo->IsNetAuthority();
		if (bShouldNotifyServer)
		{
			MyAbilityComponent->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(), LocalTargetDataHandle, ApplicationTag, MyAbilityComponent->ScopedPredictionKey);
		}

		const bool bIsTargetDataValid = true;

		// 确认弹药充足后才真正提交消耗
		if (bIsTargetDataValid && CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
		{
			// 本次射击已生效，累加散布热量
			URangedWeaponInstance* WeaponData = GetWeaponInstance();
			check(WeaponData);
			WeaponData->AddSpread();

			// 交由蓝图对命中目标施加效果
			OnRangedWeaponTargetDataReady(LocalTargetDataHandle);
		}
		else
		{
			UE_LOG(LogModularGameplayAbilities, Warning, TEXT("武器能力 %s 提交失败（命中数据有效性=%d）"), *GetPathName(), bIsTargetDataValid ? 1 : 0);
			K2_EndAbility();
		}
	}

	// 数据已处理完毕
	MyAbilityComponent->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
}

void UGameplayAbility_RangedWeapon::StartRangedWeaponTargeting()
{
	check(CurrentActorInfo);

	AActor* AvatarActor = CurrentActorInfo->AvatarActor.Get();
	check(AvatarActor);

	UAbilitySystemComponent* MyAbilityComponent = CurrentActorInfo->AbilitySystemComponent.Get();
	check(MyAbilityComponent);

	TArray<FHitResult> FoundHits;
	PerformLocalTargeting(/*输出*/ FoundHits);

	// 用命中结果填充目标数据
	FGameplayAbilityTargetDataHandle TargetData;

	if (FoundHits.Num() > 0)
	{
		const int32 CartridgeID = FMath::Rand();

		for (const FHitResult& FoundHit : FoundHits)
		{
			FWeaponAbilityTargetData_SingleTargetHit* NewTargetData = new FWeaponAbilityTargetData_SingleTargetHit();
			NewTargetData->HitResult = FoundHit;
			NewTargetData->CartridgeID = CartridgeID;

			TargetData.Add(NewTargetData);
		}
	}

	// 立即处理目标数据
	OnTargetDataReadyCallback(TargetData, FGameplayTag());
}
