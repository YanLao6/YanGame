#include "MoverMode/Fatigue/YanFatigueJumpCheck.h"

#include "Chaos/ParticleHandle.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Chaos/Utilities.h"
#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "DefaultMovementSet/CharacterMoverSimulationTypes.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "MoverEffect/YanFatigueEffects.h"
#include "MoverMode/Fatigue/YanFatigueState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanFatigueJumpCheck)

namespace
{
	// 对地面刚体施加带位置的冲量，与引擎 ChaosCharacterJumpCheck 的私有实现一致
	void ApplyImpulseAtLocation(Chaos::FPBDRigidParticleHandle* Particle, const Chaos::FVec3& Impulse, const Chaos::FVec3& Location)
	{
		const Chaos::FRigidTransform3 ComTransform = Particle->GetTransformXRCom();
		const Chaos::FVec3            Offset       = Location - ComTransform.GetLocation();
		const Chaos::FMatrix33        WorldInvI    = Chaos::Utilities::ComputeWorldSpaceInertia(ComTransform.GetRotation(), Particle->ConditionedInvI());
		Particle->SetW(Particle->GetW() + WorldInvI * Offset.Cross(Impulse));
		Particle->SetV(Particle->GetV() + Particle->InvM() * Impulse);
	}
}

UYanFatigueJumpCheck::UYanFatigueJumpCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UYanFatigueJumpCheck::Trigger_Implementation(const FSimulationTickParams& Params)
{
	if (!Simulation)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UYanFatigueJumpCheck 缺少 Simulation，请确认使用 Chaos 后端"));
		return;
	}

	const FChaosMoverSimulationDefaultInputs* SimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>();
	if (!SimInputs)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("UYanFatigueJumpCheck 需要 FChaosMoverSimulationDefaultInputs"));
		return;
	}

	const float UpwardsSpeed = FYanFatigueState::ScaleByFatigue(Params.StartState.SyncState, EYanFatigueChannel::Jump, JumpUpwardsSpeed, MinJumpUpwardsSpeed);
	FVector     JumpVelocity = UpwardsSpeed * SimInputs->UpDir;

	// 扣掉当前竖直分量，使起跳后的竖直速度恒为换算值
	const FMoverDefaultSyncState* StartingSyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (StartingSyncState)
	{
		JumpVelocity -= StartingSyncState->GetVelocity_WorldSpace().Dot(SimInputs->UpDir) * SimInputs->UpDir;
	}

	// 叠加地面竖直速度，使起跳相对于地面
	FFloorCheckResult                  FloorResult;
	UE::ChaosMover::FGroundDynamicsInfo GroundDynamicsInfo;
	const UMoverBlackboard*            Blackboard = Simulation->GetBlackboard();
	if (Blackboard && Blackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult) && Blackboard->TryGet(UE::ChaosMover::Blackboard::GroundDynamicsInfo, GroundDynamicsInfo))
	{
		if (FloorResult.HitResult.PhysicsObject && StartingSyncState)
		{
			JumpVelocity += GroundDynamicsInfo.LinearVelocity.Dot(SimInputs->UpDir) * SimInputs->UpDir;
		}
	}

	TSharedPtr<FYanFatigueVelocityEffect> JumpMove = MakeShared<FYanFatigueVelocityEffect>();
	JumpMove->VelocityOrImpulseToApply = JumpVelocity;
	JumpMove->Mode                     = EChaosMoverVelocityEffectMode::AdditiveVelocity;
	JumpMove->Channel                  = EYanFatigueChannel::Jump;
	JumpMove->FatigueCost              = JumpFatigueCost;
	JumpMove->RecoveryPerSecond        = JumpFatigueRecoveryPerSecond;
	Simulation->QueueInstantMovementEffect_Internal(JumpMove);

	const float StartingHeight = StartingSyncState ? StartingSyncState->GetLocation_WorldSpace().Z : 0.0f;
	Simulation->AddEvent(MakeShared<FJumpedEventData>(Params.TimeStep, StartingHeight));

	// 对地面施加等大反向的冲量
	if (FractionalGroundReactionImpulse > 0.0f)
	{
		if (Chaos::FPhysicsObjectHandle GroundObject = FloorResult.HitResult.PhysicsObject)
		{
			Chaos::FWritePhysicsObjectInterface_Internal Interface     = Chaos::FPhysicsObjectInternalInterface::GetWrite();
			const float                                  CharacterMass = Interface.GetMass({ SimInputs->PhysicsObject });
			const FVector                                ImpulseToApply = -FractionalGroundReactionImpulse * CharacterMass * JumpVelocity;
			if (Chaos::FPBDRigidParticleHandle* GroundParticle = Interface.GetRigidParticle(GroundObject))
			{
				if (GroundParticle->IsDynamic())
				{
					ApplyImpulseAtLocation(GroundParticle, ImpulseToApply, FloorResult.HitResult.ImpactPoint);
				}
			}
		}
	}
}
