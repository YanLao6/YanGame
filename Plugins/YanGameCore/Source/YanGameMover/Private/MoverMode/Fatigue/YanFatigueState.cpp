#include "MoverMode/Fatigue/YanFatigueState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanFatigueState)

namespace
{
	// 疲劳度按 uint8 量化上传；两端各自预测出的浮点值与量化往返后的值最多相差一个量化步，
	// 比对容差须留出该余量，否则每帧都判不等
	constexpr float FatigueQuantScale     = 255.f;
	constexpr float FatigueReconcileTolerance = 1.5f / FatigueQuantScale;
}

const FYanFatigueEntry* FYanFatigueState::FindEntry(const FMoverSyncState& SyncState, EYanFatigueChannel Channel)
{
	const FYanFatigueState* State = SyncState.SyncStateCollection.FindDataByType<FYanFatigueState>();
	return State ? State->FindEntry(Channel) : nullptr;
}

float FYanFatigueState::GetFatigue(const FMoverSyncState& SyncState, EYanFatigueChannel Channel)
{
	const FYanFatigueEntry* Entry = FindEntry(SyncState, Channel);
	return Entry ? Entry->Fatigue : 0.f;
}

float FYanFatigueState::ScaleByFatigue(const FMoverSyncState& SyncState, EYanFatigueChannel Channel, float RestedValue, float ExhaustedValue)
{
	return FMath::Lerp(RestedValue, ExhaustedValue, GetFatigue(SyncState, Channel));
}

void FYanFatigueState::Accumulate(EYanFatigueChannel Channel, float Cost, float RecoveryPerSecond, bool bRecoverAfterLanding)
{
	FYanFatigueEntry* Entry = FindEntry(Channel);
	if (!Entry)
	{
		Entry          = &Entries.AddDefaulted_GetRef();
		Entry->Channel = Channel;
	}
	Entry->Fatigue              = FMath::Min(Entry->Fatigue + Cost, 1.f);
	Entry->RecoveryPerSecond    = RecoveryPerSecond;
	Entry->bRecoverAfterLanding = bRecoverAfterLanding;
}

void FYanFatigueState::Recover(float DeltaSeconds, bool bOnGround)
{
	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		FYanFatigueEntry& Entry = Entries[Index];
		if (Entry.bRecoverAfterLanding && bOnGround)
		{
			Entry.bRecoverAfterLanding = false;
		}

		if (!Entry.bRecoverAfterLanding)
		{
			Entry.Fatigue -= Entry.RecoveryPerSecond * DeltaSeconds;
		}

		if (Entry.Fatigue <= 0.f)
		{
			Entries.RemoveAt(Index);
		}
	}
}

const FYanFatigueEntry* FYanFatigueState::FindEntry(EYanFatigueChannel Channel) const
{
	return Entries.FindByPredicate([Channel](const FYanFatigueEntry& Entry) { return Entry.Channel == Channel; });
}

FYanFatigueEntry* FYanFatigueState::FindEntry(EYanFatigueChannel Channel)
{
	return Entries.FindByPredicate([Channel](const FYanFatigueEntry& Entry) { return Entry.Channel == Channel; });
}

FMoverDataStructBase* FYanFatigueState::Clone() const
{
	return new FYanFatigueState(*this);
}

bool FYanFatigueState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	uint8 NumEntries = static_cast<uint8>(Entries.Num());
	Ar << NumEntries;
	if (Ar.IsLoading())
	{
		Entries.SetNum(NumEntries);
	}

	for (FYanFatigueEntry& Entry : Entries)
	{
		Ar << Entry.Channel;

		uint8 QuantizedFatigue = static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(Entry.Fatigue, 0.f, 1.f) * FatigueQuantScale));
		Ar << QuantizedFatigue;
		if (Ar.IsLoading())
		{
			Entry.Fatigue = QuantizedFatigue / FatigueQuantScale;
		}

		Ar << Entry.RecoveryPerSecond;
		Ar.SerializeBits(&Entry.bRecoverAfterLanding, 1);
	}

	bOutSuccess = true;
	return true;
}

UScriptStruct* FYanFatigueState::GetScriptStruct() const
{
	return StaticStruct();
}

bool FYanFatigueState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FYanFatigueState& Auth = static_cast<const FYanFatigueState&>(AuthorityState);

	// 按通道比对疲劳度，缺失的通道视为 0：两端恢复到 0 的时刻可能错开一帧，
	// 条目数不同本身不构成分歧。恢复速率来自配置，两端天然一致，不参与比对；
	// 是否等待落地决定此后能否恢复，两端不一致即会分歧，须参与比对
	auto HasDivergedChannel = [](const FYanFatigueState& Lhs, const FYanFatigueState& Rhs)
	{
		for (const FYanFatigueEntry& Entry : Lhs.Entries)
		{
			const FYanFatigueEntry* Other = Rhs.FindEntry(Entry.Channel);
			const float OtherFatigue = Other ? Other->Fatigue : 0.f;
			if (!FMath::IsNearlyEqual(Entry.Fatigue, OtherFatigue, FatigueReconcileTolerance))
			{
				return true;
			}

			if (Other && Other->bRecoverAfterLanding != Entry.bRecoverAfterLanding)
			{
				return true;
			}
		}
		return false;
	};

	return HasDivergedChannel(Auth, *this) || HasDivergedChannel(*this, Auth);
}

void FYanFatigueState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	// 通道集合不可逐元素插值，按 Pct 取离散值
	const FYanFatigueState& Src = static_cast<const FYanFatigueState&>((Pct < 0.5f) ? From : To);
	Entries = Src.Entries;
}

void FYanFatigueState::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);
	for (const FYanFatigueEntry& Entry : Entries)
	{
		Out.Appendf("Fatigue[%u]: %.3f (recover %.3f/s%s) | ", static_cast<uint8>(Entry.Channel), Entry.Fatigue, Entry.RecoveryPerSecond, Entry.bRecoverAfterLanding ? " after landing" : "");
	}
	Out.Append("\n");
}
