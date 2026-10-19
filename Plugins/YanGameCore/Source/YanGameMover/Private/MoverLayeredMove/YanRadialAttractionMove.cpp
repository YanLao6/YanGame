#include "MoverLayeredMove/YanRadialAttractionMove.h"

#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"

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

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;
	if (DeltaSeconds <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const double CenterElapsedSeconds = FMath::Max(0.0, TimeStep.BaseSimTimeMs - SnapshotSimTimeMs) * 0.001;
	const FVector Center = Location + CenterVelocity * CenterElapsedSeconds;
	const FVector ToCenter = Center - SyncState->GetLocation_WorldSpace();
	const float   Distance = ToCenter.Size();

	if (bCaptureToCenter)
	{
		if (!bCaptured && Distance > Radius)
		{
			bHasEntryVelocity = false;
			DurationMs = 0.0f;
			return false;
		}

		const FVector PullDirection = ToCenter.GetSafeNormal();
		if (!bHasEntryVelocity)
		{
			RetainedVelocity = FVector::VectorPlaneProject(SyncState->GetVelocity_WorldSpace() - CenterVelocity, PullDirection) * (1.0f - EntryVelocityDamping);
			bHasEntryVelocity = true;
		}

		if (!bCaptured && Distance <= InnerRadius)
		{
			bCaptured = true;
			// 内圈捕获只由来源显式取消，不受后续距离和刷新租期影响。
			DurationMs = -1.0f;
			CaptureMove.StartLocation = -ToCenter;
			CaptureMove.TargetLocation = FVector::ZeroVector;
			CaptureMove.StartSimTimeMs = TimeStep.BaseSimTimeMs;
			CaptureMove.DurationMs = CaptureDurationMs;
			CaptureMove.EaseExponent = CaptureEaseExponent;
			CaptureMove.bRestrictSpeedToExpected = false;
		}

		OutProposedMove.MixMode = EMoveMixMode::OverrideVelocity;
		OutProposedMove.PreferredMode = DefaultModeNames::Falling;
		if (bCaptured)
		{
			const double CaptureElapsedMs = TimeStep.BaseSimTimeMs - CaptureMove.StartSimTimeMs;
			if (CaptureElapsedMs < CaptureMove.DurationMs)
			{
				FMoverTickStartData RelativeStartState;
				FMoverDefaultSyncState& RelativeSync = RelativeStartState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
				RelativeSync.SetTransforms_WorldSpace(-ToCenter, SyncState->GetOrientation_WorldSpace(), SyncState->GetVelocity_WorldSpace() - CenterVelocity, SyncState->GetAngularVelocityDegrees_WorldSpace());
				// 求本步结束时的位置，使收拢最后一拍抵达终点。
				FMoverTimeStep CaptureTimeStep = TimeStep;
				CaptureTimeStep.BaseSimTimeMs += TimeStep.StepMs;
				CaptureMove.GenerateMove_Async(RelativeStartState, CaptureTimeStep, SimBlackboard, OutProposedMove);
				OutProposedMove.LinearVelocity += CenterVelocity;
				OutProposedMove.PreferredMode = DefaultModeNames::Falling;
			}
			else
			{
				// 每步追回球心误差，PullSpeed 只约束捕获前的外圈吸引。
				OutProposedMove.LinearVelocity = CenterVelocity + ToCenter / DeltaSeconds;
			}
		}
		else
		{
			const float RadialSpeed = FMath::Min(FMath::Max(Magnitude, 0.0f), FMath::Max(0.0f, Distance - InnerRadius) / DeltaSeconds);
			OutProposedMove.LinearVelocity = CenterVelocity + PullDirection * RadialSpeed + FVector::VectorPlaneProject(RetainedVelocity, PullDirection);
		}
		return true;
	}

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

void FYanLayeredMove_RadialAttraction::Refresh(const FYanLayeredMove_RadialAttraction& Settings, double CurrentSimTimeMs)
{
	if (Settings.UpdateSequence <= UpdateSequence)
	{
		return;
	}

	Location = Settings.Location;
	CenterVelocity = Settings.CenterVelocity;
	SnapshotSimTimeMs = Settings.SnapshotSimTimeMs;
	Radius = Settings.Radius;
	Magnitude = Settings.Magnitude;
	FalloffExponent = Settings.FalloffExponent;
	bIsPush = Settings.bIsPush;
	bCaptureToCenter = Settings.bCaptureToCenter;
	InnerRadius = Settings.InnerRadius;
	EntryVelocityDamping = Settings.EntryVelocityDamping;
	CaptureDurationMs = Settings.CaptureDurationMs;
	CaptureEaseExponent = Settings.CaptureEaseExponent;
	MixMode = Settings.MixMode;
	Priority = Settings.Priority;
	UpdateSequence = Settings.UpdateSequence;
	const double ElapsedMs = StartSimTimeMs >= 0.0 ? FMath::Max(0.0, CurrentSimTimeMs - StartSimTimeMs) : 0.0;
	if (Radius <= 0.0f)
	{
		DurationMs = 0.0f;
	}
	else
	{
		DurationMs = bCaptured ? -1.0f : Settings.DurationMs + static_cast<float>(ElapsedMs);
	}
}

bool FYanLayeredMove_RadialAttraction::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	return bCaptureToCenter && Radius > 0.0f && DurationMs != 0.0f && (bExactMatch ? TagToFind.MatchesTagExact(Mover_DisableLanding) : TagToFind.MatchesTag(Mover_DisableLanding));
}

void FYanLayeredMove_RadialAttraction::GetGameplayTags(FGameplayTagContainer& InOutTags) const
{
	if (bCaptureToCenter && Radius > 0.0f && DurationMs != 0.0f)
	{
		InOutTags.AddTag(Mover_DisableLanding);
	}
}

FLayeredMoveBase* FYanLayeredMove_RadialAttraction::Clone() const
{
	return new FYanLayeredMove_RadialAttraction(*this);
}

void FYanLayeredMove_RadialAttraction::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	SerializeSettings(Ar);
	Ar << RetainedVelocity;
	Ar.SerializeBits(&bHasEntryVelocity, 1);
	Ar.SerializeBits(&bCaptured, 1);
	CaptureMove.NetSerialize(Ar);
}

void FYanLayeredMove_RadialAttraction::SerializeSettings(FArchive& Ar)
{
	SerializePackedVector<10, 24>(Location, Ar);

	Ar << Radius;
	Ar << Magnitude;
	Ar << FalloffExponent;
	Ar.SerializeBits(&bIsPush, 1);
	Ar << SourceId;
	Ar << UpdateSequence;
	Ar << SnapshotSimTimeMs;
	Ar << CenterVelocity;
	Ar.SerializeBits(&bCaptureToCenter, 1);
	Ar << InnerRadius;
	Ar << EntryVelocityDamping;
	Ar << CaptureDurationMs;
	Ar << CaptureEaseExponent;
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

bool FYanRadialAttractionUpdateEffect::ApplyMovementEffect_Async(FApplyMovementEffectParams_Async& ApplyEffectParams, FMoverSyncState& OutputState)
{
	if (!ApplyEffectParams.TimeStep || Settings.SourceId == 0)
	{
		return false;
	}

	const double CurrentSimTimeMs = ApplyEffectParams.TimeStep->BaseSimTimeMs;
	bool bFoundSource = false;
	OutputState.LayeredMoves.ForEachActiveMoveOfType<FYanLayeredMove_RadialAttraction>([&](FYanLayeredMove_RadialAttraction& Move)
	{
		if (Move.SourceId == Settings.SourceId)
		{
			if (Settings.UpdateSequence > Move.UpdateSequence && Move.IsFinished(CurrentSimTimeMs) && Settings.Radius > 0.0f)
			{
				Move = Settings;
				Move.StartSimTimeMs = CurrentSimTimeMs;
			}
			else
			{
				Move.Refresh(Settings, CurrentSimTimeMs);
			}
			bFoundSource = true;
		}
	});
	for (const TSharedPtr<FLayeredMoveBase>& QueuedMove : OutputState.LayeredMoves.GetQueuedMoves())
	{
		if (QueuedMove.IsValid() && QueuedMove->GetScriptStruct() == FYanLayeredMove_RadialAttraction::StaticStruct())
		{
			FYanLayeredMove_RadialAttraction& Move = *static_cast<FYanLayeredMove_RadialAttraction*>(QueuedMove.Get());
			if (Move.SourceId == Settings.SourceId)
			{
				if (Settings.UpdateSequence > Move.UpdateSequence)
				{
					Move.Refresh(Settings, CurrentSimTimeMs);
				}
				bFoundSource = true;
			}
		}
	}
	if (!bFoundSource && Settings.Radius > 0.0f && Settings.DurationMs > 0.0f)
	{
		OutputState.LayeredMoves.QueueLayeredMove(MakeShared<FYanLayeredMove_RadialAttraction>(Settings));
	}
	return true;
}

FInstantMovementEffect* FYanRadialAttractionUpdateEffect::Clone() const
{
	return new FYanRadialAttractionUpdateEffect(*this);
}

void FYanRadialAttractionUpdateEffect::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	Settings.SerializeSettings(Ar);
	Ar << Settings.DurationMs;
	Ar << Settings.MixMode;
	Ar << Settings.Priority;
}

UScriptStruct* FYanRadialAttractionUpdateEffect::GetScriptStruct() const
{
	return FYanRadialAttractionUpdateEffect::StaticStruct();
}

FString FYanRadialAttractionUpdateEffect::ToSimpleString() const
{
	return FString::Printf(TEXT("RadialAttractionUpdate Source=%d Sequence=%u"), Settings.SourceId, Settings.UpdateSequence);
}
