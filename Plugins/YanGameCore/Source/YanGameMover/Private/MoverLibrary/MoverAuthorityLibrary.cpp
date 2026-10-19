#include "MoverLibrary/MoverAuthorityLibrary.h"

#include "MoverComponent.h"
#include "ChaosMover/Character/Effects/ChaosCharacterApplyVelocityEffect.h"
#include "ChaosMover/ChaosMoverActionTypes.h"
#include "MoverEffect/YanTeleportEffect.h"
#include "MoverLayeredMove/YanBlinkToMove.h"
#include "MoverLayeredMove/YanClinchMove.h"
#include "MoverLayeredMove/YanGrapplePullMove.h"
#include "MoverLayeredMove/YanNoLandingMove.h"
#include "MoverLayeredMove/YanRadialAttractionMove.h"
#include "Physics/NetworkPhysicsComponent.h"
#include "Physics/NetworkPhysicsSettingsComponent.h"
#include "PhysicsEngine/PhysicsSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverAuthorityLibrary)

namespace
{
	// sim action 的调度延迟：须大于最大 RTT，各端才能在同一物理帧应用而不触发修正
	float GetEventSchedulingDelaySeconds(AActor* Owner)
	{
		if (UNetworkPhysicsSettingsComponent* SettingsComp = Owner->FindComponentByClass<UNetworkPhysicsSettingsComponent>())
		{
			return SettingsComp->GetSettings().GeneralSettings.EventSchedulingMinDelaySeconds;
		}

		return UPhysicsSettings::Get()->PhysicsPrediction.MaxSupportedLatencyPrediction;
	}
}

bool UMoverAuthorityLibrary::ApplyChaosVelocity(UMoverComponent* MoverComp, FVector WorldSpaceVelocity, bool bOverrideVelocity, bool bScheduleForSync)
{
	AActor* Owner = MoverComp ? MoverComp->GetOwner() : nullptr;
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	UNetworkPhysicsComponent* NetPhysics = Owner->FindComponentByClass<UNetworkPhysicsComponent>();
	if (!NetPhysics)
	{
		return false;
	}

	FChaosCharacterApplyVelocityEffect VelocityEffect;
	VelocityEffect.VelocityOrImpulseToApply = WorldSpaceVelocity;
	VelocityEffect.Mode                     = bOverrideVelocity ? EChaosMoverVelocityEffectMode::OverrideVelocity : EChaosMoverVelocityEffectMode::AdditiveVelocity;

	// Authority 风格是跨 Pawn 施力的前提：默认的 Proposed 风格会把本次效果当作目标端的提议去校验，
	// 而目标端根本没有产生过它。
	FChaosMoverInstantMovementEffectAction Action;
	Action.AuthorStyle = FNetworkPhysicsActionPayload::EActionAuthorStyle::Authority;
	Action.Effect.InitializeAsScriptStruct(FChaosCharacterApplyVelocityEffect::StaticStruct(), reinterpret_cast<const uint8*>(&VelocityEffect));

	const float DelaySeconds = bScheduleForSync ? GetEventSchedulingDelaySeconds(Owner) : 0.f;
	NetPhysics->EnqueueScheduledAction_External(Action, /*SourceId=*/0u, DelaySeconds, /*bReliable=*/true);

	return true;
}

bool UMoverAuthorityLibrary::ApplyTeleport(UMoverComponent* MoverComp,
                                                   FVector          TargetLocation,
                                                   FRotator         TargetRotation,
                                                   bool             bOverrideVelocity,
                                                   FVector          VelocityAfterTeleport,
                                                   bool             bScheduleForSync)
{
	AActor* Owner = MoverComp ? MoverComp->GetOwner() : nullptr;
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	UNetworkPhysicsComponent* NetPhysics = Owner->FindComponentByClass<UNetworkPhysicsComponent>();
	if (!NetPhysics)
	{
		return false;
	}

	FYanTeleportEffect TeleportEffect;
	TeleportEffect.TargetLocation                 = TargetLocation;
	TeleportEffect.TargetRotation                 = TargetRotation;
	TeleportEffect.bUseActorRotation              = false;
	TeleportEffect.bOverrideVelocityAfterTeleport = bOverrideVelocity;
	TeleportEffect.VelocityAfterTeleport          = VelocityAfterTeleport;

	// 同 ApplyAuthoritativeVelocityToTarget：传送由世界 Actor 发起，目标端没有产生过它
	FChaosMoverInstantMovementEffectAction Action;
	Action.AuthorStyle = FNetworkPhysicsActionPayload::EActionAuthorStyle::Authority;
	Action.Effect.InitializeAsScriptStruct(FYanTeleportEffect::StaticStruct(), reinterpret_cast<const uint8*>(&TeleportEffect));

	const float DelaySeconds = bScheduleForSync ? GetEventSchedulingDelaySeconds(Owner) : 0.f;
	NetPhysics->EnqueueScheduledAction_External(Action, /*SourceId=*/0u, DelaySeconds, /*bReliable=*/true);

	return true;
}

bool UMoverAuthorityLibrary::ApplyBlink(UMoverComponent* MoverComp, FVector TargetLocation, float DurationMs, float EaseExponent, FName ForceMovementMode)
{
	AActor* Owner = MoverComp ? MoverComp->GetOwner() : nullptr;
	if (!Owner || !Owner->HasAuthority() || DurationMs <= 0.f)
	{
		return false;
	}

	UNetworkPhysicsComponent* NetPhysics = Owner->FindComponentByClass<UNetworkPhysicsComponent>();
	if (!NetPhysics)
	{
		return false;
	}

	FYanLayeredMove_BlinkTo Move;
	Move.StartLocation     = MoverComp->GetUpdatedComponentTransform().GetLocation();
	Move.TargetLocation    = TargetLocation;
	Move.DurationMs        = DurationMs;
	Move.EaseExponent      = EaseExponent;
	Move.ForceMovementMode = ForceMovementMode;

	// 同 ApplyRadialAttractionToTarget：位移作用于他人，默认的 Proposed 风格
	// 会把它当作目标端的提议去校验，而目标端根本没有产生过它
	FChaosMoverLayeredMoveAction Action;
	Action.AuthorStyle = FNetworkPhysicsActionPayload::EActionAuthorStyle::Authority;
	Action.Move.InitializeAsScriptStruct(FYanLayeredMove_BlinkTo::StaticStruct(), reinterpret_cast<const uint8*>(&Move));

	NetPhysics->EnqueueScheduledAction_External(Action, /*SourceId=*/0u, GetEventSchedulingDelaySeconds(Owner), /*bReliable=*/true);

	return true;
}

bool UMoverAuthorityLibrary::ApplyRadialAttraction(UMoverComponent* MoverComp,
                                                           FVector          AttractionCenter,
                                                           float            Radius,
                                                           float            Magnitude,
                                                           float            FalloffExponent,
                                                           float            DurationMs,
                                                           bool             bIsPush)
{
	AActor* Owner = MoverComp ? MoverComp->GetOwner() : nullptr;
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	UNetworkPhysicsComponent* NetPhysics = Owner->FindComponentByClass<UNetworkPhysicsComponent>();
	if (!NetPhysics)
	{
		return false;
	}

	FYanLayeredMove_RadialAttraction Move;
	Move.Location        = AttractionCenter;
	Move.Radius          = Radius;
	Move.Magnitude       = Magnitude;
	Move.FalloffExponent = FalloffExponent;
	Move.bIsPush         = bIsPush;
	Move.DurationMs      = DurationMs;

	// 同 ApplyAuthoritativeVelocityToTarget：牵引作用于他人，默认的 Proposed 风格
	// 会把它当作目标端的提议去校验，而目标端根本没有产生过它
	FChaosMoverLayeredMoveAction Action;
	Action.AuthorStyle = FNetworkPhysicsActionPayload::EActionAuthorStyle::Authority;
	Action.Move.InitializeAsScriptStruct(FYanLayeredMove_RadialAttraction::StaticStruct(), reinterpret_cast<const uint8*>(&Move));

	NetPhysics->EnqueueScheduledAction_External(Action, /*SourceId=*/0u, GetEventSchedulingDelaySeconds(Owner), /*bReliable=*/true);

	return true;
}

bool UMoverAuthorityLibrary::RefreshRadialAttraction(UMoverComponent* MoverComp, const FYanAttractionSettings& Settings, bool bScheduleForSync)
{
	AActor* Owner = MoverComp ? MoverComp->GetOwner() : nullptr;
	if (!Owner || !Owner->HasAuthority() || Settings.SourceId == 0 || Settings.UpdateSequence == 0 || (Settings.Radius > 0.0f && Settings.DurationMs <= 0.0f))
	{
		return false;
	}

	UNetworkPhysicsComponent* NetPhysics = Owner->FindComponentByClass<UNetworkPhysicsComponent>();
	if (!NetPhysics)
	{
		return false;
	}

	FYanRadialAttractionUpdateEffect Update;
	Update.Settings                    = Settings;
	const FMoverTimeStep& LastTimeStep = MoverComp->GetLastTimeStep();
	Update.Settings.SnapshotSimTimeMs  = LastTimeStep.BaseSimTimeMs + LastTimeStep.StepMs;

	FChaosMoverInstantMovementEffectAction Action;
	Action.AuthorStyle = FNetworkPhysicsActionPayload::EActionAuthorStyle::Authority;
	Action.Effect.InitializeAsScriptStruct(FYanRadialAttractionUpdateEffect::StaticStruct(), reinterpret_cast<const uint8*>(&Update));
	const float DelaySeconds = bScheduleForSync ? GetEventSchedulingDelaySeconds(Owner) : 0.0f;
	NetPhysics->EnqueueScheduledAction_External(Action, static_cast<uint32>(Settings.SourceId), DelaySeconds, /*bReliable=*/true);
	return true;
}

bool UMoverAuthorityLibrary::ApplyGrapplePull(UMoverComponent* MoverComp,
                                                      FVector          MeetPoint,
                                                      float            PullAcceleration,
                                                      float            MaxPullSpeed,
                                                      float            StopDistance,
                                                      float            DurationMs)
{
	AActor* Owner = MoverComp ? MoverComp->GetOwner() : nullptr;
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	UNetworkPhysicsComponent* NetPhysics = Owner->FindComponentByClass<UNetworkPhysicsComponent>();
	if (!NetPhysics)
	{
		return false;
	}

	FYanLayeredMove_GrapplePull Move;
	Move.MeetPoint        = MeetPoint;
	Move.PullAcceleration = PullAcceleration;
	Move.MaxPullSpeed     = MaxPullSpeed;
	Move.StopDistance     = StopDistance;
	Move.DurationMs       = DurationMs;

	// 同 ApplyRadialAttractionToTarget：牵引作用于他人，默认的 Proposed 风格
	// 会把它当作目标端的提议去校验，而目标端根本没有产生过它
	FChaosMoverLayeredMoveAction Action;
	Action.AuthorStyle = FNetworkPhysicsActionPayload::EActionAuthorStyle::Authority;
	Action.Move.InitializeAsScriptStruct(FYanLayeredMove_GrapplePull::StaticStruct(), reinterpret_cast<const uint8*>(&Move));

	NetPhysics->EnqueueScheduledAction_External(Action, /*SourceId=*/0u, GetEventSchedulingDelaySeconds(Owner), /*bReliable=*/true);

	return true;
}

bool UMoverAuthorityLibrary::ApplyNoLanding(UMoverComponent* MoverComp, float DurationSeconds)
{
	AActor* Owner = MoverComp ? MoverComp->GetOwner() : nullptr;
	if (!Owner || !Owner->HasAuthority() || DurationSeconds <= 0.f)
	{
		return false;
	}

	UNetworkPhysicsComponent* NetPhysics = Owner->FindComponentByClass<UNetworkPhysicsComponent>();
	if (!NetPhysics)
	{
		return false;
	}

	FYanLayeredMove_NoLanding Move;
	Move.DurationMs = DurationSeconds * 1000.f;

	// 同 ApplyAuthoritativeVelocityToTarget：效果作用于他人，默认的 Proposed 风格
	// 会把它当作目标端的提议去校验，而目标端根本没有产生过它
	FChaosMoverLayeredMoveAction Action;
	Action.AuthorStyle = FNetworkPhysicsActionPayload::EActionAuthorStyle::Authority;
	Action.Move.InitializeAsScriptStruct(FYanLayeredMove_NoLanding::StaticStruct(), reinterpret_cast<const uint8*>(&Move));

	NetPhysics->EnqueueScheduledAction_External(Action, /*SourceId=*/0u, GetEventSchedulingDelaySeconds(Owner), /*bReliable=*/true);

	return true;
}

bool UMoverAuthorityLibrary::ApplyClinch(UMoverComponent* MoverComp,
                                                 FVector          AnchorLocation,
                                                 double           FallStartSimTimeMs,
                                                 float            TargetYaw,
                                                 float            MaxHoldSpeed,
                                                 float            TurnRateDegrees,
                                                 float            MaxDurationSeconds)
{
	AActor* Owner = MoverComp ? MoverComp->GetOwner() : nullptr;
	if (!Owner || !Owner->HasAuthority() || MaxDurationSeconds <= 0.f)
	{
		return false;
	}

	UNetworkPhysicsComponent* NetPhysics = Owner->FindComponentByClass<UNetworkPhysicsComponent>();
	if (!NetPhysics)
	{
		return false;
	}

	FYanLayeredMove_Clinch Move;
	// 被抓方原地锁定，起点与落位点重合，缓动段因而退化为零
	Move.StartLocation             = AnchorLocation;
	Move.AnchorLocation            = AnchorLocation;
	Move.PullDurationMs            = 0.f;
	Move.FallStartSimTimeMs        = FallStartSimTimeMs;
	// 重力在 game thread 取定并随效果下发，与发起方那一份同源，双方的下落解算据此一致
	Move.Gravity                   = MoverComp->GetGravityAcceleration();
	Move.MaxHoldSpeed              = MaxHoldSpeed;
	Move.TargetYaw                 = TargetYaw;
	Move.TurnRateDegrees           = TurnRateDegrees;
	Move.bControlOrientation       = true;
	Move.bCanBreakFree             = true;
	// 被抓方从不写抓取意图，若据此判定会一诞生就自我终结
	Move.bEndOnClinchIntentCleared = false;
	Move.DurationMs                = MaxDurationSeconds * 1000.f;

	// 同 ApplyAuthoritativeVelocityToTarget：效果作用于他人，默认的 Proposed 风格
	// 会把它当作目标端的提议去校验，而目标端根本没有产生过它
	FChaosMoverLayeredMoveAction Action;
	Action.AuthorStyle = FNetworkPhysicsActionPayload::EActionAuthorStyle::Authority;
	Action.Move.InitializeAsScriptStruct(FYanLayeredMove_Clinch::StaticStruct(), reinterpret_cast<const uint8*>(&Move));

	NetPhysics->EnqueueScheduledAction_External(Action, /*SourceId=*/0u, GetEventSchedulingDelaySeconds(Owner), /*bReliable=*/true);

	return true;
}
