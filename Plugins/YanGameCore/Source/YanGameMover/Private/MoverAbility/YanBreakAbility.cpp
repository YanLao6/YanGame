#include "MoverAbility/YanBreakAbility.h"

#include "AbilitySystemComponent.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverLibrary/MoverAuthorityLibrary.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanBreakAbility)

UE_DEFINE_GAMEPLAY_TAG(Status_Break_Window, "Status.Break.Window");

UYanBreakAbility::UYanBreakAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 消耗须在本地即时扣除，否则玩家按下与能量条变化之间隔一个 RTT
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UYanBreakAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 基类在此提交消耗；能量不足即直接结束，本次卸力不成立
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!IsActive())
	{
		return;
	}

	if (ActorInfo != nullptr && ActorInfo->IsLocallyControlled())
	{
		PlayLocalFeedback();
		OnBreakPerformed();
	}

	if (ActorInfo != nullptr && ActorInfo->IsNetAuthority())
	{
		ApplyBreakToSelf();
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

void UYanBreakAbility::ApplyBreakToSelf() const
{
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn == nullptr)
	{
		return;
	}

	UMoverComponent* MoverComp = OwnerPawn->FindComponentByClass<UMoverComponent>();
	if (MoverComp == nullptr)
	{
		return;
	}

	const FVector CurrentVelocity = MoverComp->GetVelocity();
	if (CurrentVelocity.Size() < MinBreakableSpeed)
	{
		return;
	}

	// 偏转方向取输入包上传的移动意图，而非服务器本地的任何视角量：
	// 「朝哪推摇杆」是玩家的选择，只有他自己的输入能表达
	const FCharacterDefaultInputs* CharInputs = MoverComp->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FVector                  DeflectDir = CharInputs ? CharInputs->GetMoveInput_WorldSpace() : FVector::ZeroVector;

	const FVector BrokenVelocity = ComputeBrokenVelocity(CurrentVelocity, DeflectDir);

	// 与击退走同一条权威通道：一个预测一个权威会在同一条速度上互相覆盖
	UMoverAuthorityLibrary::ApplyChaosVelocity(MoverComp, BrokenVelocity, /*bOverrideVelocity=*/true, /*bScheduleForSync=*/true);

	// 消费掉窗口，使一次受击只能卸一次力
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(Status_Break_Window));
	}
}

FVector UYanBreakAbility::ComputeBrokenVelocity(const FVector& CurrentVelocity, const FVector& DeflectDirection) const
{
	const FVector Reduced = CurrentVelocity * FMath::Clamp(1.f - SpeedReduction, 0.f, 1.f);

	const FVector FromDir = Reduced.GetSafeNormal();
	const FVector ToDir   = DeflectDirection.GetSafeNormal();
	if (FromDir.IsNearlyZero() || ToDir.IsNearlyZero() || MaxDeflectionDegrees <= 0.f)
	{
		return Reduced;
	}

	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(FromDir, ToDir), -1.f, 1.f)));
	if (AngleDegrees <= KINDA_SMALL_NUMBER)
	{
		return Reduced;
	}

	// 正好反向时叉积为零轴，此时绕水平右向量转——转到哪都比原路返回强，方向不必确定
	FVector RotationAxis = FVector::CrossProduct(FromDir, ToDir).GetSafeNormal();
	if (RotationAxis.IsNearlyZero())
	{
		RotationAxis = FVector::CrossProduct(FromDir, FVector::UpVector).GetSafeNormal();
		if (RotationAxis.IsNearlyZero())
		{
			return Reduced;
		}
	}

	const float AppliedDegrees = FMath::Min(AngleDegrees, MaxDeflectionDegrees);
	return FromDir.RotateAngleAxis(AppliedDegrees, RotationAxis) * Reduced.Size();
}

void UYanBreakAbility::PlayLocalFeedback() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn == nullptr || BreakSound == nullptr)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(OwnerPawn, BreakSound, OwnerPawn->GetActorLocation());
}
