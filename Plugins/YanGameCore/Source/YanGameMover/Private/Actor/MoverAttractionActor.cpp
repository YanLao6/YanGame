#include "Actor/MoverAttractionActor.h"

#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverMode/Attraction/YanAttractionState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "MoverLibrary/MoverAuthorityLibrary.h"
#include "MoverLibrary/MoverQueryLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverAttractionActor)

AMoverAttractionActor::AMoverAttractionActor()
{
	bReplicates = true;
	SetReplicatingMovement(true);
	PrimaryActorTick.bCanEverTick          = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AMoverAttractionActor::StartControl()
{
	if (!HasAuthority() || bControlled || (!HasActorBegunPlay() && !IsActorBeginningPlay()) || IsActorBeingDestroyed())
	{
		return;
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Throttle    = 0;
	AimDistance = -1.0f;
	bControlled = true;
	OnRep_Controlled();
	ForceNetUpdate();
}

void AMoverAttractionActor::StopControl()
{
	if (!HasAuthority() || !bControlled)
	{
		return;
	}

	bControlled = false;
	Throttle    = 0;
	AimDistance = -1.0f;
	OnRep_Controlled();
	ForceNetUpdate();
}

void AMoverAttractionActor::SubmitControlInput(int32 InThrottle)
{
	const APawn* Caster = GetInstigator();
	if (!bControlled || !Caster || !Caster->IsLocallyControlled() || IsActorBeingDestroyed())
	{
		return;
	}

	const int8 NewThrottle = static_cast<int8>(FMath::Clamp(InThrottle, -1, 1));
	if (NewThrottle != LastSentThrottle)
	{
		ServerUpdateControl(NewThrottle);
		LastSentThrottle = NewThrottle;
	}
}

void AMoverAttractionActor::ServerUpdateControl_Implementation(int8 InThrottle)
{
	const APawn* Caster = GetInstigator();
	if (!bControlled || !Caster || !Caster->GetController() || IsActorBeingDestroyed() || InThrottle < -1 || InThrottle > 1)
	{
		return;
	}

	Throttle = InThrottle;
}

void AMoverAttractionActor::OnRep_Controlled()
{
	LastSentThrottle = 2;
	if (bControlled)
	{
		SetActorTickEnabled(true);
	}
}

void AMoverAttractionActor::SendLocalControl()
{}

void AMoverAttractionActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bControlled)
	{
		return;
	}

	const APawn* Caster = GetInstigator();
	if (Caster && Caster->IsLocallyControlled())
	{
		SendLocalControl();
	}

	if (HasAuthority() && bControlled && !IsActorBeingDestroyed())
	{
		MoveTowardsAim(DeltaSeconds);
	}
}

void AMoverAttractionActor::MoveTowardsAim(float DeltaSeconds)
{
	const APawn* Caster = GetInstigator();
	if (!Caster || !Caster->GetController() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
	{
		return;
	}

	UMoverComponent*               Mover  = Caster->FindComponentByClass<UMoverComponent>();
	const FCharacterDefaultInputs* Inputs = Mover ? Mover->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>() : nullptr;
	// 未取得上传的视角输入时悬停，不使用身体朝向替代玩家视角。
	if (!Inputs || Inputs->ControlRotation.ContainsNaN())
	{
		return;
	}

	FVector AimStart;
	FVector AimDirection;
	if (!UMoverQueryLibrary::GetOwnerViewLocationAndForward(Mover, AimStart, AimDirection)
	    || AimStart.ContainsNaN() || AimDirection.ContainsNaN())
	{
		return;
	}

	if (AimDistance < 0.0f)
	{
		AimDistance = FMath::Clamp((GetActorLocation() - AimStart) | AimDirection, AimDistanceRange.X, AimDistanceRange.Y);
	}

	AimDistance               = FMath::Clamp(AimDistance + Throttle * FMath::Max(PushSpeed, 0.0f) * DeltaSeconds, AimDistanceRange.X, AimDistanceRange.Y);
	const FVector NewLocation = FMath::VInterpConstantTo(GetActorLocation(), AimStart + AimDirection * AimDistance, DeltaSeconds, FMath::Max(MoveSpeed, 0.0f));
	SetActorLocation(NewLocation);
}

void AMoverAttractionActor::StartAttraction()
{
	if (!HasAuthority() || (!HasActorBegunPlay() && !IsActorBeginningPlay()) || IsActorBeingDestroyed())
	{
		return;
	}

	FTimerManager& TimerManager = GetWorldTimerManager();
	if (TimerManager.IsTimerActive(AttractionTimer))
	{
		return;
	}

	LastAttractionSampleTime = -1.0;
	TimerManager.SetTimer(AttractionTimer, this, &ThisClass::RefreshAttraction, FMath::Max(AttractionRefreshInterval, 0.01f), /*bLoop=*/true);
}

void AMoverAttractionActor::StopAttraction()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(AttractionTimer);
	if (!AttractedMovers.IsEmpty())
	{
		++AttractionUpdateSequence;
		for (const TWeakObjectPtr<UMoverComponent>& WeakMover : AttractedMovers)
		{
			if (UMoverComponent* Mover = WeakMover.Get())
			{
				ReleaseAttraction(Mover);
			}
		}
		AttractedMovers.Reset();
	}

	LastAttractionSampleTime = -1.0;
}

void AMoverAttractionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopControl();
	StopAttraction();
	Super::EndPlay(EndPlayReason);
}

void AMoverAttractionActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMoverAttractionActor, bControlled);
}

float AMoverAttractionActor::GetAttractRadius_Implementation() const
{
	return 0.0f;
}

TSet<APawn*> AMoverAttractionActor::GatherAttractedPawns() const
{
	TSet<APawn*> Pawns;
	const float  Radius = GetAttractRadius();
	if (Radius <= 0.0f)
	{
		return Pawns;
	}

	const TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes = {UEngineTypes::ConvertToObjectType(ECC_Pawn)};
	TArray<AActor*>                             ActorsToIgnore;
	if (APawn* InstigatorPawn = GetInstigator())
	{
		ActorsToIgnore.Add(InstigatorPawn);
	}

	TArray<AActor*> OverlappingActors;
	UKismetSystemLibrary::SphereOverlapActors(this, GetActorLocation(), Radius, ObjectTypes, APawn::StaticClass(), ActorsToIgnore, OverlappingActors);
	for (AActor* Actor : OverlappingActors)
	{
		Pawns.Add(CastChecked<APawn>(Actor));
	}
	return Pawns;
}

void AMoverAttractionActor::RefreshAttraction()
{
	const FVector CenterLocation = GetActorLocation();
	const double  SampleTime     = GetWorld()->GetTimeSeconds();
	const double  SampleDelta    = SampleTime - LastAttractionSampleTime;
	const FVector CenterVelocity = LastAttractionSampleTime >= 0.0 && SampleDelta > UE_KINDA_SMALL_NUMBER ? (CenterLocation - LastAttractionLocation) / SampleDelta : FVector::ZeroVector;
	LastAttractionLocation       = CenterLocation;
	LastAttractionSampleTime     = SampleTime;
	++AttractionUpdateSequence;

	FYanAttractionSettings Settings;
	Settings.SourceId       = static_cast<int32>(GetUniqueID());
	Settings.UpdateSequence = AttractionUpdateSequence;
	Settings.Location       = CenterLocation;
	Settings.CenterVelocity = CenterVelocity;
	Settings.Radius         = FMath::Max(GetAttractRadius(), 0.0f);
	Settings.PullSpeed      = FMath::Max(PullSpeed, 0.0f);
	Settings.InnerRadius    = FMath::Clamp(InnerRadius, 0.0f, Settings.Radius);
	// 租期只约束外圈与快照外推；内圈锁定持续到来源显式取消。
	Settings.DurationMs     = FMath::Max(AttractionRefreshInterval, 0.01f) * 3000.0f;

	for (APawn* Pawn : GatherAttractedPawns())
	{
		if (UMoverComponent* Mover = Pawn->FindComponentByClass<UMoverComponent>())
		{
			AttractedMovers.Add(Mover);
		}
	}

	// 捕获在异步模拟中发生，范围扫描不能判定目标已脱离；已接触目标持续接收中心快照。
	for (auto It = AttractedMovers.CreateIterator(); It; ++It)
	{
		if (UMoverComponent* Mover = It->Get())
		{
			UMoverAuthorityLibrary::RefreshRadialAttraction(Mover, Settings);
		}
		else
		{
			It.RemoveCurrent();
		}
	}
}

void AMoverAttractionActor::ReleaseAttraction(UMoverComponent* Mover)
{
	FYanAttractionSettings Settings;
	Settings.SourceId       = static_cast<int32>(GetUniqueID());
	Settings.UpdateSequence = AttractionUpdateSequence;
	Settings.Radius         = 0.0f;
	UMoverAuthorityLibrary::RefreshRadialAttraction(Mover, Settings);
}
