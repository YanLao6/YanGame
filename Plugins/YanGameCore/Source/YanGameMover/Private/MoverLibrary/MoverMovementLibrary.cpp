#include "MoverLibrary/MoverMovementLibrary.h"

#include "MoverComponent.h"
#include "DefaultMovementSet/LayeredMoves/LaunchMove.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "MoverModifier/YanMaxSpeedScaleModifier.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverMovementLibrary)

FMovementModifierHandle UMoverMovementLibrary::QueueMaxSpeedScale(UMoverComponent* MoverComp, float SpeedScale)
{
	if (!MoverComp)
	{
		return FMovementModifierHandle(MODIFIER_INVALID_HANDLE);
	}

	TSharedPtr<FYanMaxSpeedScaleModifier> Modifier = MakeShared<FYanMaxSpeedScaleModifier>();
	Modifier->SpeedScale                           = SpeedScale;
	return MoverComp->QueueMovementModifier(Modifier);
}

void UMoverMovementLibrary::QueueApplyVelocityEffect(UMoverComponent* MoverComp, FName NewModeName)
{
	if (!MoverComp)
	{
		return;
	}
	// 零速度叠加 = 保持当前速度，ForceMovementMode 强制切换模式
	TSharedPtr<FApplyVelocityEffect> Effect = MakeShared<FApplyVelocityEffect>();
	Effect->VelocityToApply                 = FVector::ZeroVector;
	Effect->bAdditiveVelocity               = true;
	Effect->ForceMovementMode               = NewModeName;
	MoverComp->QueueInstantMovementEffect(Effect);
}

void UMoverMovementLibrary::QueueLaunchMove(UMoverComponent* MoverComp, FVector LaunchVelocity, FName ForceMovementMode, float DurationMs)
{
	if (!MoverComp)
	{
		return;
	}

	TSharedPtr<FLayeredMove_Launch> LaunchMove = MakeShared<FLayeredMove_Launch>();
	LaunchMove->LaunchVelocity                 = LaunchVelocity;
	LaunchMove->ForceMovementMode              = ForceMovementMode;
	LaunchMove->DurationMs                     = DurationMs;
	MoverComp->QueueLayeredMove(LaunchMove);
}

void UMoverMovementLibrary::QueueLinearVelocityMove(UMoverComponent* MoverComp, FVector Velocity, float DurationMs, bool bOverrideVelocity)
{
	if (!MoverComp)
	{
		return;
	}

	TSharedPtr<FLayeredMove_LinearVelocity> LinearMove = MakeShared<FLayeredMove_LinearVelocity>();
	LinearMove->Velocity                               = Velocity;
	LinearMove->DurationMs                             = DurationMs;
	// Override 覆盖本帧提议速度（屏蔽方向输入与摩擦，直线冲刺）；否则默认叠加到正常移动上
	LinearMove->MixMode                                = bOverrideVelocity ? EMoveMixMode::OverrideVelocity : EMoveMixMode::AdditiveVelocity;
	MoverComp->QueueLayeredMove(LinearMove);
}
