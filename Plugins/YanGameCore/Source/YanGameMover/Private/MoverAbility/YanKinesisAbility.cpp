#include "MoverAbility/YanKinesisAbility.h"

#include "Kinesis/YanKinesisProjection.h"
#include "Kinesis/YanKinesisRegistrySubsystem.h"
#include "Kinesis/YanKinesisTargetComponent.h"
#include "YanMoverAngelscriptLibrary.h"
#include "AbilitySystemComponent.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Messages/VerbMessageHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanKinesisAbility)

namespace
{
	// 首次调用时 TagManager 已就绪，之后复用查表结果
	const FGameplayTag& GetPushModifierTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Event.Kinesis.Modifier.Push"));
		return Tag;
	}

	const FGameplayTag& GetPullModifierTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Event.Kinesis.Modifier.Pull"));
		return Tag;
	}
}

UYanKinesisAbility::UYanKinesisAbility(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	// 目标筛选与施力均为服务器权威；本地仅预测施法表现
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UYanKinesisAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (ActorInfo && ActorInfo->IsLocallyControlled())
	{
		PlayLocalFeedback();
	}

	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		ApplyKinesisToTargets(DecodeIntent(TriggerEventData));
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UYanKinesisAbility::ApplyKinesisToTargets(const FYanKinesisIntent& Intent)
{
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!OwnerPawn)
	{
		return;
	}

	UMoverComponent* MoverComp = OwnerPawn->FindComponentByClass<UMoverComponent>();

	// 瞄准取自上传服务器的输入包：客户端与服务器口径一致，不依赖纯客户端的摄像机状态
	const FCharacterDefaultInputs* CharInputs = MoverComp ? MoverComp->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>() : nullptr;

	// 视点既是目标筛选的球心，也是径向施力的原点：两者同源，避免筛选范围与施力方向脱节
	const FVector  ViewLocation    = OwnerPawn->GetPawnViewLocation();
	const FRotator AimRotation     = CharInputs ? CharInputs->ControlRotation : OwnerPawn->GetActorRotation();
	const FVector  ViewForward     = AimRotation.Vector();
	const FVector  AimIntentDirection = ResolveAimIntentDirection(AimRotation, Intent.AimScreenDirection);

	UAbilitySystemComponent* InstigatorASC = GetAbilitySystemComponentFromActorInfo();
	if (!InstigatorASC || !KinesisGameplayEffect)
	{
		return;
	}

	TArray<AActor*> Targets;
	if (!AcceptNominatedTarget(Intent.NominatedTarget, OwnerPawn, ViewLocation, ViewForward, Targets))
	{
		// 会话锁不到合法目标即放弃本次施力；无会话的直接激活则回落群体锥形，保留单独调试的手段
		if (Intent.bFromSession)
		{
			return;
		}

		GatherTargetsInCone(OwnerPawn, ViewLocation, ViewForward, Targets);
	}

	for (AActor* Target : Targets)
	{
		const FVector Velocity = Intent.Destination
		                             ? ResolveVelocityTowardDestination(Target->GetActorLocation(), Intent.Destination)
		                             : ComputeVelocityForTarget(Target->GetActorLocation(), ViewLocation, AimIntentDirection, Intent.RadialSign);

		if (Velocity.IsNearlyZero())
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(Target);
		if (!TargetASC)
		{
			continue;
		}

		// 瞬时速度无持续语义，DurationMs 传 0
		UYanMoverAngelscriptLibrary::ApplyLaunchEffectToTarget(InstigatorASC, TargetASC, KinesisGameplayEffect, Velocity, /*DurationMs=*/0.f);
	}
}

bool UYanKinesisAbility::EvaluateConeFit(const FVector& ViewLocation, const FVector& ViewForward, const FVector& TargetLocation, float MaxDistance, float CosHalfAngle, float& OutAlignment)
{
	const FVector ToTarget = TargetLocation - ViewLocation;
	const float   Distance = ToTarget.Size();

	if (Distance > MaxDistance || Distance < UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutAlignment = FVector::DotProduct(ViewForward, ToTarget / Distance);

	return OutAlignment >= CosHalfAngle;
}

AActor* UYanKinesisAbility::FindKinesisTarget(APawn* OwnerPawn, float MaxDistance, float ScreenRadiusRatio, AActor* IgnoredActor)
{
	APlayerController* OwnerController = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	if (!OwnerController)
	{
		return nullptr;
	}

	UYanKinesisRegistrySubsystem* Registry = UYanKinesisRegistrySubsystem::Get(OwnerPawn);
	if (!Registry)
	{
		return nullptr;
	}

	FVector2D ViewportSize = FVector2D::ZeroVector;
	if (!YanKinesis::GetPlayerViewportSize(OwnerController, ViewportSize))
	{
		return nullptr;
	}

	const FVector ViewLocation = OwnerPawn->GetPawnViewLocation();
	const FVector ViewForward  = OwnerController->GetControlRotation().Vector();

	TArray<UYanKinesisTargetComponent*> Candidates;
	Registry->GatherCandidates(OwnerPawn, ViewLocation, MaxDistance, Candidates);

	// 准心恒在玩家视口正中；半径以视口高度为基准，同一比例在任意分辨率下张成同样的视觉范围
	const FVector2D ScreenCenter    = ViewportSize * 0.5f;
	const float     MaxScreenDistSq = FMath::Square(ScreenRadiusRatio * ViewportSize.Y);

	AActor* BestTarget       = nullptr;
	float   BestScreenDistSq = TNumericLimits<float>::Max();

	for (const UYanKinesisTargetComponent* Candidate : Candidates)
	{
		AActor* TargetActor = Candidate->GetOwner();
		if (TargetActor == IgnoredActor)
		{
			continue;
		}

		const USceneComponent* Anchor         = Candidate->ResolveAnchorComponent();
		const FVector          AnchorLocation = Anchor ? Anchor->GetComponentLocation() : TargetActor->GetActorLocation();

		// 与指示器共用同一次投影口径，准心压住哪个图标就选中谁
		FVector2D ScreenPosition = FVector2D::ZeroVector;
		if (!YanKinesis::ProjectAnchorToScreen(OwnerController, ViewLocation, ViewForward, AnchorLocation, ScreenPosition))
		{
			continue;
		}

		// 取离准心最近者而非最贴合视线者：玩家的选择意图由屏幕上的指示器表达
		const float ScreenDistSq = FVector2D::DistSquared(ScreenPosition, ScreenCenter);
		if (ScreenDistSq > MaxScreenDistSq || ScreenDistSq >= BestScreenDistSq)
		{
			continue;
		}

		BestTarget       = TargetActor;
		BestScreenDistSq = ScreenDistSq;
	}

	return BestTarget;
}

void UYanKinesisAbility::NotifyKinesisControlState(AActor* Target, AActor* InInstigator, bool bBegin)
{
	UYanKinesisTargetComponent* TargetComponent = IsValid(Target) ? Target->FindComponentByClass<UYanKinesisTargetComponent>() : nullptr;
	if (!TargetComponent)
	{
		return;
	}

	if (bBegin)
	{
		TargetComponent->NotifyControlBegin(InInstigator);
	}
	else
	{
		TargetComponent->NotifyControlEnd(InInstigator);
	}
}

void UYanKinesisAbility::GatherTargetsInCone(APawn* OwnerPawn, const FVector& ViewLocation, const FVector& ViewForward, TArray<AActor*>& OutTargets) const
{
	UYanKinesisRegistrySubsystem* Registry = UYanKinesisRegistrySubsystem::Get(OwnerPawn);
	if (!Registry)
	{
		return;
	}

	TArray<UYanKinesisTargetComponent*> Candidates;
	Registry->GatherCandidates(OwnerPawn, ViewLocation, MaxDistance, Candidates);

	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(ConeHalfAngleDegrees));

	for (const UYanKinesisTargetComponent* Candidate : Candidates)
	{
		AActor* TargetActor = Candidate->GetOwner();

		float Alignment = 0.f;
		if (!EvaluateConeFit(ViewLocation, ViewForward, TargetActor->GetActorLocation(), MaxDistance, CosHalfAngle, Alignment))
		{
			continue;
		}

		OutTargets.Add(TargetActor);

		if (OutTargets.Num() >= MaxTargets)
		{
			break;
		}
	}
}

bool UYanKinesisAbility::AcceptNominatedTarget(const AActor* NominatedTarget, APawn* OwnerPawn, const FVector& ViewLocation, const FVector& ViewForward, TArray<AActor*>& OutTargets) const
{
	// EventData 的目标为只读引用，施力需要可变指针；GAS 侧对该字段的常规取用方式
	AActor* TargetActor = const_cast<AActor*>(NominatedTarget);

	if (!IsValid(TargetActor) || TargetActor == OwnerPawn)
	{
		return false;
	}

	// 提名必须是登记在册的可控物：客户端不能凭空指认任意 Actor
	const UYanKinesisTargetComponent* TargetComponent = TargetActor->FindComponentByClass<UYanKinesisTargetComponent>();
	if (!TargetComponent || !TargetComponent->IsControllableBy(OwnerPawn))
	{
		return false;
	}

	// 服务器只做宽松的下限校验：它既没有客户端的视口参数，视角还滞后一个 RTT，
	// 复核的意义在于拦下明显越界的提名，而非复现客户端的选取
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(FMath::Min(ConeHalfAngleDegrees + ServerConeSlackDegrees, 180.f)));

	float Alignment = 0.f;
	if (!EvaluateConeFit(ViewLocation, ViewForward, TargetActor->GetActorLocation(), MaxDistance, CosHalfAngle, Alignment))
	{
		return false;
	}

	OutTargets.Add(TargetActor);

	return true;
}

float UYanKinesisAbility::EncodeAimScreenDirection(FVector2D AimScreenDirection, float MinMagnitude)
{
	if (AimScreenDirection.Size() < FMath::Max(MinMagnitude, UE_KINDA_SMALL_NUMBER))
	{
		return -1.f;
	}

	// Atan2 返回 (-180, 180]，统一抬到 [0, 360) 以便用负值单独表达无手势
	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Atan2(AimScreenDirection.Y, AimScreenDirection.X));

	return (AngleDegrees < 0.f) ? (AngleDegrees + 360.f) : AngleDegrees;
}

FVector2D UYanKinesisAbility::DecodeAimScreenDirection(const FGameplayEventData* TriggerEventData)
{
	if (!TriggerEventData || TriggerEventData->EventMagnitude < 0.f)
	{
		return FVector2D::ZeroVector;
	}

	const float AngleRadians = FMath::DegreesToRadians(TriggerEventData->EventMagnitude);

	return FVector2D(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians));
}

FYanKinesisIntent UYanKinesisAbility::DecodeIntent(const FGameplayEventData* TriggerEventData) const
{
	FYanKinesisIntent Intent;

	Intent.AimScreenDirection = DecodeAimScreenDirection(TriggerEventData);
	Intent.RadialSign         = DecodeRadialSign(TriggerEventData);
	Intent.bFromSession       = (TriggerEventData != nullptr);

	if (TriggerEventData)
	{
		Intent.NominatedTarget = TriggerEventData->Target.Get();

		// 目的地只影响方向、不影响力度，伪造它的收益不超过手势本身能表达的范围，故只做类型判定
		Intent.Destination = Cast<AActor>(TriggerEventData->OptionalObject.Get());
	}

	return Intent;
}

float UYanKinesisAbility::DecodeRadialSign(const FGameplayEventData* TriggerEventData) const
{
	if (!TriggerEventData)
	{
		return bIsPush ? 1.f : -1.f;
	}

	const bool bPush = TriggerEventData->InstigatorTags.HasTagExact(GetPushModifierTag());
	const bool bPull = TriggerEventData->InstigatorTags.HasTagExact(GetPullModifierTag());

	// 两键并存或均未按下时前后意图不成立，径向分量归零，仅余手势的平面推拉
	return (bPush == bPull) ? 0.f : (bPush ? 1.f : -1.f);
}

FVector UYanKinesisAbility::ResolveAimIntentDirection(const FRotator& AimRotation, const FVector2D& AimScreenDirection)
{
	// 手势位移低于会话层阈值时以零向量传入，此处直接表达为无偏转
	if (AimScreenDirection.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	const FVector2D       ScreenDir = AimScreenDirection.GetSafeNormal();
	const FRotationMatrix AimBasis(AimRotation);

	// 取视线的右向量与上向量而非世界 Z：手势方向与玩家在屏幕上看到的位移方向保持一致，
	// 俯视或仰视时「向上划」仍是屏幕的上方而非世界的上方
	const FVector WorldDir = AimBasis.GetUnitAxis(EAxis::Y) * ScreenDir.X + AimBasis.GetUnitAxis(EAxis::Z) * ScreenDir.Y;

	return WorldDir.GetSafeNormal();
}

FVector UYanKinesisAbility::ComputeVelocityForTarget(const FVector& TargetLocation, const FVector& ViewLocation, const FVector& AimIntentDirection, float RadialSign) const
{
	// RadialSign 为零时本项消失，方向完全由手势决定，目标只沿屏幕平面滑动
	const FVector RadialDir = (TargetLocation - ViewLocation).GetSafeNormal() * RadialSign;

	// 无手势时 AimIntentDirection 为零向量，公式自动退化为纯径向；两者恰好抵消时回落径向
	FVector FinalDir = (RadialDir + AimIntentDirection * AimIntentWeight).GetSafeNormal();
	if (FinalDir.IsNearlyZero())
	{
		FinalDir = RadialDir;
	}

	// 前后与手势皆无（或恰好抵消至零）时返回零向量，调用方据此跳过该目标
	return FinalDir * Speed;
}

FVector UYanKinesisAbility::ResolveVelocityTowardDestination(const FVector& TargetLocation, const AActor* Destination) const
{
	// 目的地与被控目标重合时方向不成立，返回零向量交由调用方跳过
	return (Destination->GetActorLocation() - TargetLocation).GetSafeNormal() * Speed;
}

void UYanKinesisAbility::PlayLocalFeedback() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!OwnerPawn)
	{
		return;
	}

	if (CastSound)
	{
		UGameplayStatics::PlaySoundAtLocation(OwnerPawn, CastSound, OwnerPawn->GetActorLocation());
	}

	if (CastMontage)
	{
		if (const USkeletalMeshComponent* Mesh = OwnerPawn->FindComponentByClass<USkeletalMeshComponent>())
		{
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
			{
				AnimInstance->Montage_Play(CastMontage);
			}
		}
	}
}
