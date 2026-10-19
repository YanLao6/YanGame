#include "GameMode/YanCollapsingPlatform.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameMode/YanDuelRoundComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanCollapsingPlatform)

namespace
{
	// 赛制组件随 Experience 注入，晚于关卡 Actor 的 BeginPlay，只能轮询等它出现
	constexpr float BindRetryIntervalSeconds = 0.25f;
}

AYanCollapsingPlatform::AYanCollapsingPlatform(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Tick 只在预警与下坠期间开启，由 UpdateTickEnabled 管理
	PrimaryActorTick.bCanEverTick          = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	SetReplicateMovement(false);

	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	SetRootComponent(PlatformMesh);
}

void AYanCollapsingPlatform::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AYanCollapsingPlatform, bCollapsed);
	DOREPLIFETIME(AYanCollapsingPlatform, bWarning);
}

void AYanCollapsingPlatform::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 下坠改的是 Actor 位置，复位要还原到这里。取值必须早于任何一次落状态——
	// 客户端的 OnRep 不保证晚于 BeginPlay，那时若原位还是零向量，平台会被瞬移到世界原点下方
	RestLocation = GetActorLocation();
}

void AYanCollapsingPlatform::BeginPlay()
{
	Super::BeginPlay();

	// 中途加入的客户端此时已带着复制来的状态，直接落到终态，不重演坠落
	ApplyCollapsedState(/*bImmediate=*/true);

	if (HasAuthority() && RingIndex >= 0)
	{
		TryBindRoundComponent();
	}
}

void AYanCollapsingPlatform::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimer);
		World->GetTimerManager().ClearTimer(WarningTimer);
		World->GetTimerManager().ClearTimer(CollapseTimer);
	}

	if (UYanDuelRoundComponent* RoundComp = BoundRoundComponent.Get())
	{
		RoundComp->OnRoundStarted.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AYanCollapsingPlatform::TryBindRoundComponent()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const AGameStateBase* GameState = World->GetGameState();
	UYanDuelRoundComponent* RoundComp = GameState ? GameState->FindComponentByClass<UYanDuelRoundComponent>() : nullptr;

	if (RoundComp == nullptr)
	{
		World->GetTimerManager().SetTimer(BindRetryTimer, this, &AYanCollapsingPlatform::TryBindRoundComponent, BindRetryIntervalSeconds, /*bLoop=*/false);
		return;
	}

	World->GetTimerManager().ClearTimer(BindRetryTimer);

	RoundComp->OnRoundStarted.AddDynamic(this, &AYanCollapsingPlatform::HandleRoundStarted);
	BoundRoundComponent = RoundComp;

	// 绑上时首个回合已经开始，那次广播错过了，就地补一次
	HandleRoundStarted();
}

void AYanCollapsingPlatform::HandleRoundStarted()
{
	Restore();
	ScheduleCollapse();
}

void AYanCollapsingPlatform::ScheduleCollapse()
{
	UWorld* World = GetWorld();
	if (World == nullptr || RingIndex < 0)
	{
		return;
	}

	const float CollapseAt = (RingIndex + 1) * CollapseIntervalSeconds;
	const float WarnAt     = FMath::Max(CollapseAt - WarningSeconds, KINDA_SMALL_NUMBER);

	World->GetTimerManager().SetTimer(WarningTimer, this, &AYanCollapsingPlatform::EnterWarning, WarnAt, /*bLoop=*/false);
	World->GetTimerManager().SetTimer(CollapseTimer, this, &AYanCollapsingPlatform::Collapse, CollapseAt, /*bLoop=*/false);
}

void AYanCollapsingPlatform::EnterWarning()
{
	bWarning = true;
	OnRep_Warning();
}

void AYanCollapsingPlatform::Collapse()
{
	bCollapsed = true;
	bWarning   = false;
	ApplyCollapsedState();
}

void AYanCollapsingPlatform::Restore()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WarningTimer);
		World->GetTimerManager().ClearTimer(CollapseTimer);
	}

	bCollapsed = false;
	bWarning   = false;
	ApplyCollapsedState();
}

void AYanCollapsingPlatform::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bWarning)
	{
		// 在常态色与警告色之间来回；越接近塌陷闪得越显眼这件事交给频率，不额外加曲线
		const float Phase = GetWorld() ? GetWorld()->GetTimeSeconds() * WarnPulseHz * 2.f * PI : 0.f;
		PushWarnAmount(0.5f + 0.5f * FMath::Sin(Phase));
	}

	if (bFalling)
	{
		FallElapsedSeconds += DeltaSeconds;

		const float Alpha = FMath::Clamp(FallElapsedSeconds / FMath::Max(CollapseFallSeconds, KINDA_SMALL_NUMBER), 0.f, 1.f);
		// 平方使起落像加速下坠而非匀速下沉
		SetActorLocation(RestLocation - FVector::UpVector * (CollapseFallDistance * Alpha * Alpha));

		if (Alpha >= 1.f)
		{
			bFalling = false;
			SetActorHiddenInGame(true);
			UpdateTickEnabled();
		}
	}
}

void AYanCollapsingPlatform::ApplyCollapsedState(bool bImmediate)
{
	// 首次落状态一律瞬时：中途加入者不该看见早已塌掉的平台重演一遍坠落
	const bool bSkipAnimation = bImmediate || !bVisualsInitialized;
	bVisualsInitialized = true;

	// 碰撞在塌陷当帧就消失，与下坠动画无关——玩法真相不等表现演完
	SetActorEnableCollision(!bCollapsed);

	if (bCollapsed)
	{
		bFalling           = !bSkipAnimation;
		FallElapsedSeconds = 0.f;

		if (bSkipAnimation)
		{
			SetActorLocation(RestLocation - FVector::UpVector * CollapseFallDistance);
			SetActorHiddenInGame(true);
		}

		PushWarnAmount(0.f);
		OnCollapsed();
	}
	else
	{
		bFalling           = false;
		FallElapsedSeconds = 0.f;

		SetActorLocation(RestLocation);
		SetActorHiddenInGame(false);
		PushWarnAmount(0.f);
		OnRestored();
	}

	UpdateTickEnabled();
}

void AYanCollapsingPlatform::UpdateTickEnabled()
{
	SetActorTickEnabled(bWarning || bFalling);
}

void AYanCollapsingPlatform::PushWarnAmount(float Amount) const
{
	if (PlatformMesh != nullptr && !WarnAmountParameterName.IsNone())
	{
		PlatformMesh->SetScalarParameterValueOnMaterials(WarnAmountParameterName, Amount);
	}
}

void AYanCollapsingPlatform::OnRep_Collapsed()
{
	ApplyCollapsedState();
}

void AYanCollapsingPlatform::OnRep_Warning()
{
	if (bWarning)
	{
		OnCollapseWarning();
	}
	else
	{
		PushWarnAmount(0.f);
	}

	UpdateTickEnabled();
}
