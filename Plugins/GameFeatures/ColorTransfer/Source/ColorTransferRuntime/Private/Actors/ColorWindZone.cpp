#include "Actors/ColorWindZone.h"

#include "Abilities/Ability_ColorShot.h"
#include "AbilitySystemComponent.h"
#include "Attributes/ColorPigmentSet.h"
#include "Effects/ColorPigmentEffects.h"
#include "ColorTransferQueries.h"
#include "Components/BoxComponent.h"
#include "Components/ColorPigmentRulesComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ColorWindZone)

namespace ColorWindZone
{
	constexpr float UpdateInterval = 0.1f;

	// 风线的飞行速度，只影响表现
	constexpr float EffectWindSpeed = 1500.0f;

	const FName WindColorParam(TEXT("WindColor"));
	const FName BoxSizeParam(TEXT("BoxSize"));
	const FName WindVelocityParam(TEXT("WindVelocity"));
}

AColorWindZone::AColorWindZone()
{
	WindVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("WindVolume"));
	WindVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WindVolume->SetCollisionObjectType(ECC_WorldDynamic);
	WindVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	WindVolume->SetCollisionResponseToChannel(ColorTransfer::ShotTraceChannel, ECR_Block);
	WindVolume->SetGenerateOverlapEvents(false);
	RootComponent = WindVolume;

	WindEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("WindEffect"));
	WindEffect->SetupAttachment(WindVolume);
	// 范围由 BoxSize 参数传入，不随 Actor 缩放拉伸粒子
	WindEffect->SetUsingAbsoluteScale(true);

	bReplicates = true;
	SetReplicatingMovement(false);

	PrimaryActorTick.bCanEverTick          = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AColorWindZone::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() != NM_DedicatedServer)
	{
		WindEffect->SetVariableLinearColor(ColorWindZone::WindColorParam, WindColor);
		WindEffect->SetVariableVec3(ColorWindZone::BoxSizeParam, WindVolume->GetScaledBoxExtent() * 2.0);
		WindEffect->SetVariableVec3(ColorWindZone::WindVelocityParam, GetActorRightVector() * ColorWindZone::EffectWindSpeed);
	}

	if (HasAuthority())
	{
		PulseStartTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
		GetWorldTimerManager().SetTimer(UpdateTimer, this, &ThisClass::AuthUpdatePawnsInVolume, ColorWindZone::UpdateInterval, /*bLoop=*/true);
	}

	SetActorTickEnabled(PulsePeriod > 0.0f);
	ApplyWindState(IsWindBlowing());
}

void AColorWindZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(UpdateTimer);
		AuthSetBlockedAbilitySystems({});
	}

	Super::EndPlay(EndPlayReason);
}

void AColorWindZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const bool bBlowing = IsWindBlowing();
	if (bBlowing != bAppliedBlowing)
	{
		ApplyWindState(bBlowing);
	}
}

bool AColorWindZone::IsWindBlowing() const
{
	if (PulsePeriod <= 0.0f)
	{
		return true;
	}

	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double          Now       = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const double          Phase     = FMath::Fmod(FMath::Max(Now - PulseStartTime, 0.0), static_cast<double>(PulsePeriod));
	return Phase < PulsePeriod - PulseOffDuration;
}

void AColorWindZone::AuthDeactivate()
{
	if (!HasAuthority())
	{
		return;
	}

	// 定时器与禁射标签由 EndPlay 统一清理
	Destroy();
}

void AColorWindZone::ApplyWindState(bool bBlowing)
{
	bAppliedBlowing = bBlowing;

	WindVolume->SetCollisionEnabled(bBlowing ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

	if (GetNetMode() != NM_DedicatedServer)
	{
		if (bBlowing)
		{
			WindEffect->Activate();
		}
		else
		{
			WindEffect->Deactivate();
		}
	}
}

void AColorWindZone::AuthUpdatePawnsInVolume()
{
	TSet<TWeakObjectPtr<UAbilitySystemComponent>> InsideAbilitySystems;

	if (IsWindBlowing())
	{
		for (APawn* Pawn : ColorTransfer::GatherPawnsInVolume(WindVolume))
		{
			UColorPigmentSet* PigmentSet = UColorPigmentSet::FindForActor(Pawn);
			if (!PigmentSet)
			{
				continue;
			}

			InsideAbilitySystems.Add(PigmentSet->GetOwningAbilitySystemComponent());

			if (AllowedPigment != EColorPigment::None && PigmentSet->GetOwnPigment() != AllowedPigment)
			{
				const APlayerState* PlayerState = Pawn->GetPlayerState();
				if (UColorPigmentRulesComponent* Rules = PlayerState ? PlayerState->FindComponentByClass<UColorPigmentRulesComponent>() : nullptr)
				{
					Rules->AuthRequestDeath();
				}
				continue;
			}

			ColorTransfer::AuthApplyForeignDecay(*PigmentSet, DecayPerSecond * ColorWindZone::UpdateInterval, this);
		}
	}

	AuthSetBlockedAbilitySystems(MoveTemp(InsideAbilitySystems));
}

void AColorWindZone::AuthSetBlockedAbilitySystems(TSet<TWeakObjectPtr<UAbilitySystemComponent>>&& NewBlocked)
{
	// ASC 在 PlayerState 上跨越死亡存活，按 ASC 而非 Pawn 记录，重生后的新 Pawn 不在区域内即被撤销
	for (const TWeakObjectPtr<UAbilitySystemComponent>& ASC : NewBlocked)
	{
		if (!BlockedAbilitySystems.Contains(ASC))
		{
			ASC->AddLooseGameplayTag(Status_ColorShotBlocked, 1, EGameplayTagReplicationState::TagOnly);
		}
	}

	for (const TWeakObjectPtr<UAbilitySystemComponent>& ASC : BlockedAbilitySystems)
	{
		if (ASC.IsValid() && !NewBlocked.Contains(ASC))
		{
			ASC->RemoveLooseGameplayTag(Status_ColorShotBlocked, 1, EGameplayTagReplicationState::TagOnly);
		}
	}

	BlockedAbilitySystems = MoveTemp(NewBlocked);
}

void AColorWindZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AColorWindZone, PulseStartTime);
}
