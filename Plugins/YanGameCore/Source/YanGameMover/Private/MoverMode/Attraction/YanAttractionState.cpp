#include "MoverMode/Attraction/YanAttractionState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanAttractionState)

FVector FYanAttractionSettings::GetCenter(double SimTimeMs) const
{
	const double ElapsedMs = FMath::Clamp(SimTimeMs - SnapshotSimTimeMs, 0.0, static_cast<double>(DurationMs));
	return Location + CenterVelocity * (ElapsedMs * 0.001);
}

void FYanAttractionSettings::NetSerialize(FArchive& Ar)
{
	Ar << SourceId << UpdateSequence << Location << CenterVelocity << SnapshotSimTimeMs;
	Ar << Radius << InnerRadius << PullSpeed << DurationMs;
}

bool FYanAttractionSettings::operator==(const FYanAttractionSettings& Other) const
{
	return SourceId == Other.SourceId && UpdateSequence == Other.UpdateSequence
		&& Location == Other.Location && CenterVelocity == Other.CenterVelocity && SnapshotSimTimeMs == Other.SnapshotSimTimeMs
		&& Radius == Other.Radius && InnerRadius == Other.InnerRadius && PullSpeed == Other.PullSpeed && DurationMs == Other.DurationMs;
}

bool FYanAttractionSource::CanControl(const FVector& PawnLocation, double SimTimeMs) const
{
	return Settings.Radius > 0.0f && (bCaptured || (SimTimeMs < ExpirationSimTimeMs
		&& FVector::DistSquared(PawnLocation, Settings.GetCenter(SimTimeMs)) <= FMath::Square(Settings.Radius)));
}

const FYanAttractionSource* FYanAttractionState::SelectSource(const FVector& PawnLocation, double SimTimeMs) const
{
	const FYanAttractionSource* Selected = nullptr;
	for (const FYanAttractionSource& Source : Sources)
	{
		if (Source.CanControl(PawnLocation, SimTimeMs))
		{
			if (Source.Settings.SourceId == ActiveSourceId)
			{
				return &Source;
			}
			if (!Selected || Source.Settings.SourceId < Selected->Settings.SourceId)
			{
				Selected = &Source;
			}
		}
	}
	return Selected;
}

bool FYanAttractionState::Refresh(const FYanAttractionSettings& Settings, double SimTimeMs)
{
	FYanAttractionSource* Source = Sources.FindByPredicate([&Settings](const FYanAttractionSource& Entry)
	{
		return Entry.Settings.SourceId == Settings.SourceId;
	});
	if (Source && Settings.UpdateSequence <= Source->Settings.UpdateSequence)
	{
		return false;
	}
	if (Settings.Radius <= 0.0f)
	{
		Sources.RemoveAll([&Settings](const FYanAttractionSource& Entry) { return Entry.Settings.SourceId == Settings.SourceId; });
		if (ActiveSourceId == Settings.SourceId)
		{
			ActiveSourceId = 0;
		}
		return true;
	}
	if (!Source)
	{
		Source = &Sources.AddDefaulted_GetRef();
	}
	Source->Settings = Settings;
	Source->ExpirationSimTimeMs = SimTimeMs + Settings.DurationMs;
	Sources.Sort([](const FYanAttractionSource& A, const FYanAttractionSource& B) { return A.Settings.SourceId < B.Settings.SourceId; });
	RemoveExpired(SimTimeMs);
	return true;
}

void FYanAttractionState::UpdateCapture(const FVector& PawnLocation, double SimTimeMs)
{
	RemoveExpired(SimTimeMs);
	const FYanAttractionSource* Selected = SelectSource(PawnLocation, SimTimeMs);
	ActiveSourceId = Selected ? Selected->Settings.SourceId : 0;
	for (FYanAttractionSource& Source : Sources)
	{
		if (Source.Settings.SourceId == ActiveSourceId
			&& FVector::DistSquared(PawnLocation, Source.Settings.GetCenter(SimTimeMs)) <= FMath::Square(Source.Settings.InnerRadius))
		{
			Source.bCaptured = true;
		}
	}
}

void FYanAttractionState::RemoveExpired(double SimTimeMs)
{
	Sources.RemoveAll([SimTimeMs](const FYanAttractionSource& Source) { return !Source.bCaptured && SimTimeMs >= Source.ExpirationSimTimeMs; });
}

FMoverDataStructBase* FYanAttractionState::Clone() const
{
	return new FYanAttractionState(*this);
}

UScriptStruct* FYanAttractionState::GetScriptStruct() const
{
	return StaticStruct();
}

bool FYanAttractionState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);
	Ar << ActiveSourceId;
	int32 NumSources = Sources.Num();
	Ar << NumSources;
	if (NumSources < 0 || NumSources > MAX_uint16)
	{
		Ar.SetError();
		bOutSuccess = false;
		return false;
	}
	if (Ar.IsLoading())
	{
		Sources.SetNum(NumSources);
	}
	for (FYanAttractionSource& Source : Sources)
	{
		Source.Settings.NetSerialize(Ar);
		Ar << Source.ExpirationSimTimeMs;
		Ar.SerializeBits(&Source.bCaptured, 1);
	}
	bOutSuccess = !Ar.IsError();
	return bOutSuccess;
}

bool FYanAttractionState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FYanAttractionState& Other = static_cast<const FYanAttractionState&>(AuthorityState);
	if (ActiveSourceId != Other.ActiveSourceId || Sources.Num() != Other.Sources.Num())
	{
		return true;
	}
	for (int32 Index = 0; Index < Sources.Num(); ++Index)
	{
		const FYanAttractionSource& A = Sources[Index];
		const FYanAttractionSource& B = Other.Sources[Index];
		if (!(A.Settings == B.Settings) || A.bCaptured != B.bCaptured || A.ExpirationSimTimeMs != B.ExpirationSimTimeMs)
		{
			return true;
		}
	}
	return false;
}

void FYanAttractionState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	// 控制来源与捕获关系是离散状态，位置平滑由角色的 DefaultSyncState 负责。
	*this = static_cast<const FYanAttractionState&>(Pct < 1.0f ? From : To);
}

void FYanAttractionState::Merge(const FMoverDataStructBase& From)
{
	*this = static_cast<const FYanAttractionState&>(From);
}

bool FYanRadialAttractionUpdateEffect::ApplyMovementEffect_Async(FApplyMovementEffectParams_Async& ApplyEffectParams, FMoverSyncState& OutputState)
{
	if (!ApplyEffectParams.TimeStep || Settings.SourceId == 0 || Settings.UpdateSequence == 0)
	{
		return false;
	}
	FYanAttractionState& State = OutputState.SyncStateCollection.FindOrAddMutableDataByType<FYanAttractionState>();
	return State.Refresh(Settings, ApplyEffectParams.TimeStep->BaseSimTimeMs);
}

FInstantMovementEffect* FYanRadialAttractionUpdateEffect::Clone() const
{
	return new FYanRadialAttractionUpdateEffect(*this);
}

void FYanRadialAttractionUpdateEffect::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	Settings.NetSerialize(Ar);
}

UScriptStruct* FYanRadialAttractionUpdateEffect::GetScriptStruct() const
{
	return StaticStruct();
}

FString FYanRadialAttractionUpdateEffect::ToSimpleString() const
{
	return FString::Printf(TEXT("AttractionInput Source=%d Sequence=%u"), Settings.SourceId, Settings.UpdateSequence);
}
