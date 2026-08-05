#include "Kinesis/YanKinesisIndicatorComponent.h"

#include "Kinesis/YanKinesisProjection.h"
#include "Kinesis/YanKinesisRegistrySubsystem.h"
#include "Kinesis/YanKinesisTargetComponent.h"
#include "IndicatorSystem/IndicatorDescriptor.h"
#include "IndicatorSystem/IndicatorManagerComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanKinesisIndicatorComponent)

UYanKinesisIndicatorComponent::UYanKinesisIndicatorComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UYanKinesisIndicatorComponent::BeginPlay()
{
	Super::BeginPlay();

	// 指示器是纯本地表现，专用服务器与远端客户端上跑这套扫描没有意义
	const APlayerController* OwningController = GetController<APlayerController>();
	if (!OwningController || !OwningController->IsLocalController())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ScanTimer, this, &UYanKinesisIndicatorComponent::RefreshIndicators, ScanInterval, /*bLoop=*/true);
	}
}

void UYanKinesisIndicatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ScanTimer);
	}

	RemoveAllIndicators();

	Super::EndPlay(EndPlayReason);
}

void UYanKinesisIndicatorComponent::RefreshIndicators()
{
	APlayerController* OwningController = GetController<APlayerController>();
	APawn*             OwnerPawn        = OwningController ? OwningController->GetPawn() : nullptr;

	UIndicatorManagerComponent*   Manager  = UIndicatorManagerComponent::GetComponent(OwningController);
	UYanKinesisRegistrySubsystem* Registry = UYanKinesisRegistrySubsystem::Get(this);

	FVector2D ViewportSize = FVector2D::ZeroVector;

	// 无 Pawn（观战、重生间隙）或视口未就绪时收起全部指示器，而非留下一批指向旧位置的图标
	if (!OwnerPawn || !Manager || !Registry || !YanKinesis::GetPlayerViewportSize(OwningController, ViewportSize))
	{
		RemoveAllIndicators();
		return;
	}

	const FVector ViewLocation = OwnerPawn->GetPawnViewLocation();
	const FVector ViewForward  = OwningController->GetControlRotation().Vector();
	const float   MarginPixels = ScreenMarginRatio * ViewportSize.Y;

	TArray<UYanKinesisTargetComponent*> Candidates;
	Registry->GatherCandidates(OwnerPawn, ViewLocation, MaxDistance, Candidates);

	TSet<TWeakObjectPtr<UYanKinesisTargetComponent>> DesiredTargets;
	DesiredTargets.Reserve(Candidates.Num());

	for (UYanKinesisTargetComponent* Candidate : Candidates)
	{
		const USceneComponent* Anchor = Candidate->ResolveAnchorComponent();
		if (!Anchor)
		{
			continue;
		}

		if (IsOnScreen(OwningController, ViewLocation, ViewForward, Anchor->GetComponentLocation(), ViewportSize, MarginPixels))
		{
			DesiredTargets.Add(Candidate);
		}
	}

	for (auto It = ActiveIndicators.CreateIterator(); It; ++It)
	{
		if (DesiredTargets.Contains(It.Key()))
		{
			continue;
		}

		if (UIndicatorDescriptor* Indicator = It.Value().Get())
		{
			Manager->RemoveIndicator(Indicator);
		}

		It.RemoveCurrent();
	}

	for (const TWeakObjectPtr<UYanKinesisTargetComponent>& Desired : DesiredTargets)
	{
		if (!ActiveIndicators.Contains(Desired))
		{
			AddIndicatorFor(Desired.Get());
		}
	}
}

bool UYanKinesisIndicatorComponent::IsOnScreen(const APlayerController* OwningController, const FVector& ViewLocation, const FVector& ViewForward, const FVector& AnchorLocation, const FVector2D& ViewportSize, float MarginPixels)
{
	FVector2D ScreenPosition = FVector2D::ZeroVector;
	if (!YanKinesis::ProjectAnchorToScreen(OwningController, ViewLocation, ViewForward, AnchorLocation, ScreenPosition))
	{
		return false;
	}

	// 容差向外扩：贴近画面边界的目标不会因一两像素的抖动而反复增删指示器
	return ScreenPosition.X >= -MarginPixels
	       && ScreenPosition.Y >= -MarginPixels
	       && ScreenPosition.X <= ViewportSize.X + MarginPixels
	       && ScreenPosition.Y <= ViewportSize.Y + MarginPixels;
}

void UYanKinesisIndicatorComponent::AddIndicatorFor(UYanKinesisTargetComponent* Target)
{
	UIndicatorManagerComponent* Manager = UIndicatorManagerComponent::GetComponent(GetController<APlayerController>());
	if (!Manager || !IsValid(Target))
	{
		return;
	}

	// 目标可指定自己的图标，未指定则用统一的默认外观
	const TSoftClassPtr<UUserWidget> WidgetClass = Target->IndicatorWidgetClass.IsNull() ? DefaultIndicatorWidgetClass : Target->IndicatorWidgetClass;
	if (WidgetClass.IsNull())
	{
		return;
	}

	UIndicatorDescriptor* Indicator = NewObject<UIndicatorDescriptor>();
	Indicator->SetDataObject(Target->GetOwner());
	Indicator->SetSceneComponent(Target->ResolveAnchorComponent());
	Indicator->SetIndicatorClass(WidgetClass);

	// 锚点随 Actor 一同销毁时自行摘除，无须等到下一轮扫描
	Indicator->SetAutoRemoveWhenIndicatorComponentIsNull(true);

	Manager->AddIndicator(Indicator);

	ActiveIndicators.Add(Target, Indicator);
}

void UYanKinesisIndicatorComponent::RemoveAllIndicators()
{
	if (UIndicatorManagerComponent* Manager = UIndicatorManagerComponent::GetComponent(GetController<APlayerController>()))
	{
		for (const auto& Entry : ActiveIndicators)
		{
			if (UIndicatorDescriptor* Indicator = Entry.Value.Get())
			{
				Manager->RemoveIndicator(Indicator);
			}
		}
	}

	ActiveIndicators.Reset();
}
