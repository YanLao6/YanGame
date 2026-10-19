#include "YanChaosMoverSimulation.h"

#include "Chaos/ContactModification.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Chaos/ShapeInstance.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "Engine/EngineTypes.h"
#include "Framework/Threading.h"
#include "MoverDataModelTypes.h"
#include "MoverMode/DoubleJump/YanJumpPulseState.h"
#include "MoverMode/Fatigue/YanFatigueState.h"
#include "MovementMode.h"
#include "MoverTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanChaosMoverSimulation)

namespace YanMoverCVars
{
	static bool bBlockPawnPushImpulse = true;
	static FAutoConsoleVariableRef CVarBlockPawnPushImpulse(
		TEXT("p.YanMover.Pawn.BlockPushImpulse"),
		bBlockPawnPushImpulse,
		TEXT("角色相撞时是否把被动一方视作无限质量，使其不被撞开"));

	static float PawnApproachSpeedThreshold = 10.0f;
	static FAutoConsoleVariableRef CVarPawnApproachSpeedThreshold(
		TEXT("p.YanMover.Pawn.ApproachSpeedThreshold"),
		PawnApproachSpeedThreshold,
		TEXT("判定角色正朝对方接近的水平速度阈值（cm/s）"));
}

void UYanChaosMoverSimulation::OnPostSimulationTick(const FMoverTimeStep& TimeStep, UE::ChaosMover::FSimulationOutputData& OutputData)
{
	// 须在 Super 之前改写：基类在此函数内把 OutputData.SyncState 存为下一步的起始状态
	FMoverDataCollection& SyncStateCollection = OutputData.SyncState.SyncStateCollection;
	if (FYanFatigueState* FatigueState = SyncStateCollection.FindMutableDataByType<FYanFatigueState>())
	{
		// 以本步结束时所处模式是否带地面标签判定落地，滑铲等自定义地面模式同样生效
		const TWeakObjectPtr<const UBaseMovementMode> EndMode = FindMovementModeByName(OutputData.SyncState.MovementMode);
		const bool bOnGround = EndMode.IsValid() && EndMode->HasGameplayTag(Mover_IsOnGround, true);

		FatigueState->Recover(TimeStep.StepMs * 0.001f, bOnGround);
		if (FatigueState->IsEmpty())
		{
			// 全部恢复后整个条目出集合，不再随每帧状态复制
			SyncStateCollection.RemoveDataByType(FYanFatigueState::StaticStruct());
		}
	}

	// 供空中读跳跃脉冲的 Transition 识别上升沿，见 FYanJumpPulseState
	const FCharacterDefaultInputs* CharacterInputs = OutputData.LastUsedInputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	if (CharacterInputs && CharacterInputs->bIsJumpJustPressed)
	{
		SyncStateCollection.FindOrAddMutableDataByType<FYanJumpPulseState>();
	}
	else
	{
		SyncStateCollection.RemoveDataByType(FYanJumpPulseState::StaticStruct());
	}

	Super::OnPostSimulationTick(TimeStep, OutputData);
}

void UYanChaosMoverSimulation::OnModifyContacts(const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationInputData& InputData, const UE::ChaosMover::FSimulationOutputData& OutputData, Chaos::FCollisionContactModifier& Modifier)
{
	Super::OnModifyContacts(TimeStep, InputData, OutputData, Modifier);

	if (!YanMoverCVars::bBlockPawnPushImpulse)
	{
		return;
	}

	Chaos::EnsureIsInPhysicsThreadContext();

	const FChaosMoverSimulationDefaultInputs* SimInputs = GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>();
	if (!SimInputs)
	{
		return;
	}

	Chaos::FGeometryParticleHandle* SelfParticle = nullptr;
	{
		Chaos::FReadPhysicsObjectInterface_Internal ReadInterface = Chaos::FPhysicsObjectInternalInterface::GetRead();
		SelfParticle = ReadInterface.GetParticle(SimInputs->PhysicsObject);
	}

	if (!SelfParticle)
	{
		return;
	}

	for (Chaos::FContactPairModifier& PairModifier : Modifier.GetContacts(SelfParticle))
	{
		const int32 SelfIdx  = (SelfParticle == PairModifier.GetParticlePair()[0]) ? 0 : 1;
		const int32 OtherIdx = 1 - SelfIdx;

		// 只处理角色之间的接触，其余物件仍按原生物理交换冲量
		const Chaos::FShapeInstance* OtherShape = PairModifier.GetShape(OtherIdx);
		if (!OtherShape)
		{
			continue;
		}

		const Chaos::Filter::FCombinedShapeFilterData& CombinedShapeFilter = OtherShape->GetCombinedShapeFilterData();
		const Chaos::Filter::FShapeFilterData&         FilterData          = CombinedShapeFilter.GetShapeFilterData();
		if (FilterData.GetCollisionChannelIndex() != static_cast<uint8>(ECC_Pawn))
		{
			continue;
		}

		// 朝向与速度均取水平分量：垂直方向保留原生表现，踩踏与坠落砸击不受此规则干预
		FVector ToOther = PairModifier.GetParticlePosition(OtherIdx) - PairModifier.GetParticlePosition(SelfIdx);
		ToOther.Z       = 0.0;
		if (!ToOther.Normalize())
		{
			continue;
		}

		FVector SelfVelocity  = PairModifier.GetParticleVelocity(SelfIdx);
		FVector OtherVelocity = PairModifier.GetParticleVelocity(OtherIdx);
		SelfVelocity.Z        = 0.0;
		OtherVelocity.Z       = 0.0;

		const double SelfApproachSpeed  = FVector::DotProduct(SelfVelocity, ToOther);
		const double OtherApproachSpeed = FVector::DotProduct(OtherVelocity, -ToOther);
		const double Threshold          = YanMoverCVars::PawnApproachSpeedThreshold;

		// 单方接近时该方为主动方，被动方按无限质量参与求解；
		// 双方对冲或均未接近则不干预，交由原生求解，避免任一方被凭空钉住
		int32 PassiveIdx = INDEX_NONE;
		if (SelfApproachSpeed > Threshold && OtherApproachSpeed <= Threshold)
		{
			PassiveIdx = OtherIdx;
		}
		else if (OtherApproachSpeed > Threshold && SelfApproachSpeed <= Threshold)
		{
			PassiveIdx = SelfIdx;
		}

		if (PassiveIdx != INDEX_NONE)
		{
			PairModifier.ModifyInvMassScale(0.0, PassiveIdx);
		}
	}
}
