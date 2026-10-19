#include "Clinch/YanClinchAuthorityComponent.h"

#include "AbilitySystemComponent.h"
#include "ChaosMover/ChaosMoverLog.h"
#include "GameFramework/Pawn.h"
#include "Messages/VerbMessageHelpers.h"
#include "MoverComponent.h"
#include "MoverLibrary/MoverAuthorityLibrary.h"
#include "MoverLibrary/MoverTargetingLibrary.h"
#include "YanMoverDataModelTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanClinchAuthorityComponent)

UE_DEFINE_GAMEPLAY_TAG(Status_Grabbed, "Status.Grabbed");

UYanClinchAuthorityComponent::UYanClinchAuthorityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UYanClinchAuthorityComponent::BeginPlay()
{
	Super::BeginPlay();

	// 裁决是服务器独有的职责，客户端上连 tick 都不必开
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		SetComponentTickEnabled(false);
		return;
	}

	CachedMoverComp = Owner->FindComponentByClass<UMoverComponent>();
	if (!CachedMoverComp)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UYanClinchAuthorityComponent 所在 Actor 未挂载 MoverComponent，抓取裁决不会生效"));
		SetComponentTickEnabled(false);
	}
}

void UYanClinchAuthorityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CachedMoverComp)
	{
		return;
	}

	const FYanCharacterInputs* YanInputs = CachedMoverComp->GetLastInputCmd().InputCollection.FindDataByType<FYanCharacterInputs>();
	if (!YanInputs || !YanInputs->bWantsToClinch)
	{
		// 意图落回则本轮结束，下一次按下方可再次裁决
		bClinchResolved = false;
		return;
	}

	if (bClinchResolved)
	{
		return;
	}

	// 无论成败都只判一次：复检失败意味着服务器视角下这次抓取不成立，逐帧重试只会徒增开销
	bClinchResolved = true;
	ResolveClinch(*YanInputs);
}

void UYanClinchAuthorityComponent::ResolveClinch(const FYanCharacterInputs& YanInputs)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 服务器自行扫描即构成复检：发起端只上传落位点与下落时刻，「抓到了谁」一律由此处认定
	FVector UnusedAnchor;
	APawn*  Target = UMoverTargetingLibrary::FindClinchTarget(CachedMoverComp, VerifyRadius, VerifyHalfAngle, /*SurfaceClearance=*/0.f, UnusedAnchor);
	if (!Target)
	{
		UE_LOG(LogChaosMover, Verbose, TEXT("%s 的抓取未通过服务器复检：附近无有效目标"), *Owner->GetName());
		return;
	}

	UMoverComponent* TargetMoverComp = Target->FindComponentByClass<UMoverComponent>();
	if (!TargetMoverComp)
	{
		return;
	}

	const FVector TargetLocation = Target->GetActorLocation();

	// 被抓方扳向发起方；两者水平重合时保持原朝向，避免零向量退化成任意角度
	const FVector ToInstigator = (Owner->GetActorLocation() - TargetLocation).GetSafeNormal2D();
	const float   TargetYaw    = ToInstigator.IsNearlyZero() ? Target->GetActorRotation().Yaw : ToInstigator.Rotation().Yaw;
	// 落位点取服务器上的权威位置，下落时刻则取发起方上传的值——后者决定双方能否同时起落
	UMoverAuthorityLibrary::ApplyClinch(TargetMoverComp,
	                                    TargetLocation,
	                                    YanInputs.ClinchFallStartSimTimeMs,
	                                    TargetYaw,
	                                    MaxHoldSpeed,
	                                    TurnRateDegrees,
	                                    MaxDurationSeconds);

	ApplyClinchEffects(Target);
}

void UYanClinchAuthorityComponent::ApplyClinchEffects(APawn* Target) const
{
	UAbilitySystemComponent* InstigatorASC = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(GetOwner());
	UAbilitySystemComponent* TargetASC     = UVerbMessageHelpers::GetAbilitySystemComponentFromObject(Target);
	if (!InstigatorASC || !TargetASC)
	{
		return;
	}

	const TSubclassOf<UGameplayEffect> Effects[] = {StunEffect, GrabbedEffect};
	for (const TSubclassOf<UGameplayEffect>& EffectClass : Effects)
	{
		if (!EffectClass)
		{
			continue;
		}

		FGameplayEffectContextHandle Context = InstigatorASC->MakeEffectContext();
		Context.AddInstigator(InstigatorASC->GetAvatarActor(), InstigatorASC->GetAvatarActor());

		const FGameplayEffectSpecHandle Spec = InstigatorASC->MakeOutgoingSpec(EffectClass, /*Level=*/1.f, Context);
		if (Spec.IsValid())
		{
			InstigatorASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
		}
	}
}
