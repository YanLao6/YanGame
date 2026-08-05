#include "MoverLayeredMove/YanRadialAttractionMove.h"

#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanRadialAttractionMove)

FYanLayeredMove_RadialAttraction::FYanLayeredMove_RadialAttraction()
{
	DurationMs = 0.f;

	// 叠加而非覆盖：被拉住的人仍能靠自己的移动挣扎，牵引是拉扯而非夺取操控权
	MixMode = EMoveMixMode::AdditiveVelocity;
}

bool FYanLayeredMove_RadialAttraction::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	return GenerateMove_Async(StartState, TimeStep, SimBlackboard, OutProposedMove);
}

bool FYanLayeredMove_RadialAttraction::GenerateMove_Async(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	const FMoverDefaultSyncState* SyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!SyncState || Radius <= 0.f)
	{
		return false;
	}

	const FVector ToCenter = Location - SyncState->GetLocation_WorldSpace();
	const float   Distance = ToCenter.Size();

	// 出了半径不产生任何提议，交由上层当作本帧无此牵引
	if (Distance >= Radius || Distance < UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float Closeness      = 1.f - (Distance / Radius);
	const float FalloffFactor  = FMath::Pow(Closeness, FalloffExponent);
	const float CurrentStrength = Magnitude * FalloffFactor;

	FVector Velocity = (ToCenter / Distance) * CurrentStrength;
	if (bIsPush)
	{
		Velocity *= -1.f;
	}

	OutProposedMove.MixMode        = MixMode;
	OutProposedMove.LinearVelocity = Velocity;

	return true;
}

FLayeredMoveBase* FYanLayeredMove_RadialAttraction::Clone() const
{
	return new FYanLayeredMove_RadialAttraction(*this);
}

void FYanLayeredMove_RadialAttraction::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	SerializePackedVector<10, 24>(Location, Ar);

	Ar << Radius;
	Ar << Magnitude;
	Ar << FalloffExponent;
	Ar.SerializeBits(&bIsPush, 1);
}

UScriptStruct* FYanLayeredMove_RadialAttraction::GetScriptStruct() const
{
	return FYanLayeredMove_RadialAttraction::StaticStruct();
}

FString FYanLayeredMove_RadialAttraction::ToSimpleString() const
{
	return FString::Printf(TEXT("RadialAttraction"));
}

void FYanLayeredMove_RadialAttraction::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}
