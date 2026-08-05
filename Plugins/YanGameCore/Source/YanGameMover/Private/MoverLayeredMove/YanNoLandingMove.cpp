#include "MoverLayeredMove/YanNoLandingMove.h"

#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanNoLandingMove)

FYanLayeredMove_NoLanding::FYanLayeredMove_NoLanding()
{
	DurationMs = 300.f;

	// 本 move 不产生速度，混合模式只对首帧的零向量生效，取叠加以免夺走当帧的其他提议
	MixMode = EMoveMixMode::AdditiveVelocity;
}

bool FYanLayeredMove_NoLanding::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	return bExactMatch ? TagToFind.MatchesTagExact(Mover_DisableLanding) : TagToFind.MatchesTag(Mover_DisableLanding);
}

void FYanLayeredMove_NoLanding::GetGameplayTags(FGameplayTagContainer& InOutTags) const
{
	InOutTags.AddTag(Mover_DisableLanding);
}

bool FYanLayeredMove_NoLanding::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	return GenerateMove_Async(StartState, TimeStep, SimBlackboard, OutProposedMove);
}

bool FYanLayeredMove_NoLanding::GenerateMove_Async(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	// 首帧之后不再参与移动混合：滞空由 Mover.DisableLanding 标签维持，
	// 逐帧提交零向量只会平白让混合器把本 move 计入贡献
	if (StartSimTimeMs < TimeStep.BaseSimTimeMs)
	{
		return false;
	}

	OutProposedMove.MixMode        = MixMode;
	OutProposedMove.LinearVelocity = FVector::ZeroVector;
	OutProposedMove.PreferredMode  = ForceMovementMode;

	return true;
}

FLayeredMoveBase* FYanLayeredMove_NoLanding::Clone() const
{
	return new FYanLayeredMove_NoLanding(*this);
}

void FYanLayeredMove_NoLanding::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	Ar << ForceMovementMode;
}

UScriptStruct* FYanLayeredMove_NoLanding::GetScriptStruct() const
{
	return FYanLayeredMove_NoLanding::StaticStruct();
}

FString FYanLayeredMove_NoLanding::ToSimpleString() const
{
	return FString::Printf(TEXT("NoLanding"));
}

void FYanLayeredMove_NoLanding::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}
