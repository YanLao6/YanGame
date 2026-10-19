#include "Misc/AutomationTest.h"
#include "MoverDataModelTypes.h"
#include "MoverLayeredMove/YanRadialAttractionMove.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace YanRadialAttractionTest
{
	FYanLayeredMove_RadialAttraction MakeCaptureSettings()
	{
		FYanLayeredMove_RadialAttraction Settings;
		Settings.SourceId = 1;
		Settings.UpdateSequence = 1;
		Settings.Radius = 500.0f;
		Settings.Magnitude = 600.0f;
		Settings.DurationMs = 150.0f;
		Settings.bCaptureToCenter = true;
		Settings.MixMode = EMoveMixMode::OverrideVelocity;
		return Settings;
	}

	FMoverTickStartData MakeStartState(const FVector& Location, const FVector& Velocity = FVector::ZeroVector)
	{
		FMoverTickStartData Start;
		Start.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>().SetTransforms_WorldSpace(Location, FRotator::ZeroRotator, Velocity, FVector::ZeroVector);
		return Start;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYanRadialAttractionRefreshTest, "YanGame.Mover.RadialAttraction.Refresh", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FYanRadialAttractionRefreshTest::RunTest(const FString& Parameters)
{
	FMoverTimeStep TimeStep;
	TimeStep.BaseSimTimeMs = 100.0;
	TimeStep.StepMs = 10.0f;
	FApplyMovementEffectParams_Async EffectParams{};
	EffectParams.TimeStep = &TimeStep;
	FMoverSyncState State;
	FYanRadialAttractionUpdateEffect Update;
	Update.Settings = YanRadialAttractionTest::MakeCaptureSettings();
	Update.Settings.EntryVelocityDamping = 0.5f;
	TestTrue(TEXT("首次刷新入队"), Update.ApplyMovementEffect_Async(EffectParams, State));
	Update.Settings.UpdateSequence = 2;
	Update.Settings.Location = FVector(10.0, 0.0, 0.0);
	Update.ApplyMovementEffect_Async(EffectParams, State);
	TestEqual(TEXT("激活前刷新不重复入队"), State.LayeredMoves.GetQueuedMoves().Num(), 1);
	TArray<uint8> UpdateBytes;
	FMemoryWriter UpdateWriter(UpdateBytes, true);
	Update.NetSerialize(UpdateWriter);
	FYanRadialAttractionUpdateEffect RestoredUpdate;
	FMemoryReader UpdateReader(UpdateBytes, true);
	RestoredUpdate.NetSerialize(UpdateReader);
	TestFalse(TEXT("刷新动作序列化无错误"), UpdateWriter.IsError() || UpdateReader.IsError());
	FMoverSyncState RestoredState;
	RestoredUpdate.ApplyMovementEffect_Async(EffectParams, RestoredState);
	RestoredState.LayeredMoves.GenerateActiveMoves_Async(TimeStep, nullptr);
	const FYanLayeredMove_RadialAttraction* RestoredSettings = RestoredState.LayeredMoves.FindActiveMove<FYanLayeredMove_RadialAttraction>();
	TestNotNull(TEXT("反序列化后创建牵引"), RestoredSettings);
	if (!RestoredSettings)
	{
		return false;
	}
	TestEqual(TEXT("刷新动作保留中心"), RestoredSettings->Location, Update.Settings.Location);
	TestEqual(TEXT("刷新动作保留来源"), RestoredSettings->SourceId, Update.Settings.SourceId);
	TestEqual(TEXT("刷新动作保留序号"), RestoredSettings->UpdateSequence, Update.Settings.UpdateSequence);
	TestEqual(TEXT("刷新动作保留租期"), RestoredSettings->DurationMs, Update.Settings.DurationMs);
	Update.Settings.UpdateSequence = 1;
	Update.Settings.DurationMs = 1.0f;
	Update.Settings.Location = FVector(999.0, 0.0, 0.0);
	Update.ApplyMovementEffect_Async(EffectParams, State);
	TArray<TSharedPtr<FLayeredMoveBase>> Active = State.LayeredMoves.GenerateActiveMoves_Async(TimeStep, nullptr);
	TestEqual(TEXT("只有一个活动牵引"), Active.Num(), 1);
	if (Active.Num() != 1)
	{
		return false;
	}
	FYanLayeredMove_RadialAttraction* Move = static_cast<FYanLayeredMove_RadialAttraction*>(Active[0].Get());
	TestEqual(TEXT("旧序号不覆盖中心"), Move->Location, FVector(10.0, 0.0, 0.0));
	TestEqual(TEXT("旧序号不缩短租期"), Move->DurationMs, 150.0f);
	FProposedMove Proposed;
	FMoverTickStartData Start = YanRadialAttractionTest::MakeStartState(FVector(200.0, 0.0, 0.0), FVector(0.0, 100.0, 0.0));
	Move->GenerateMove_Async(Start, TimeStep, nullptr, Proposed);
	TestEqual(TEXT("进入切向速度只衰减一次"), Proposed.LinearVelocity.Y, 50.0);

	FMoverSyncState History = State;
	TimeStep.BaseSimTimeMs = 150.0;
	Update.Settings = YanRadialAttractionTest::MakeCaptureSettings();
	Update.Settings.UpdateSequence = 3;
	Update.Settings.EntryVelocityDamping = 0.5f;
	Update.Settings.Location = FVector(20.0, 0.0, 0.0);
	Update.ApplyMovementEffect_Async(EffectParams, State);
	Start = YanRadialAttractionTest::MakeStartState(FVector(200.0, 0.0, 0.0), FVector(0.0, 1000.0, -100.0));
	Move->GenerateMove_Async(Start, TimeStep, nullptr, Proposed);
	TestEqual(TEXT("刷新仍是同一个活动牵引"), State.LayeredMoves.GetActiveMoves().Num(), 1);
	TestEqual(TEXT("刷新不重新采样切向速度"), Proposed.LinearVelocity.Y, 50.0);
	TestEqual(TEXT("刷新不累积重力速度"), Proposed.LinearVelocity.Z, 0.0);
	TestEqual(TEXT("刷新续期"), Move->DurationMs, 200.0f);
	TestEqual(TEXT("输出刷新不污染历史 SyncState"), History.LayeredMoves.FindActiveMove<FYanLayeredMove_RadialAttraction>()->Location, FVector(10.0, 0.0, 0.0));

	Update.Settings.SourceId = 2;
	Update.ApplyMovementEffect_Async(EffectParams, State);
	State.LayeredMoves.GenerateActiveMoves_Async(TimeStep, nullptr);
	TestEqual(TEXT("不同来源独立存活"), State.LayeredMoves.GetActiveMoves().Num(), 2);
	Update.Settings.SourceId = 1;
	Update.Settings.UpdateSequence = 4;
	Update.Settings.Radius = 0.0f;
	Update.ApplyMovementEffect_Async(EffectParams, State);
	State.LayeredMoves.GenerateActiveMoves_Async(TimeStep, nullptr);
	TestEqual(TEXT("取消仅移除指定来源"), State.LayeredMoves.GetActiveMoves().Num(), 1);
	TimeStep.BaseSimTimeMs = 300.0;
	State.LayeredMoves.GenerateActiveMoves_Async(TimeStep, nullptr);
	TestEqual(TEXT("停止刷新后租期自动结束"), State.LayeredMoves.GetActiveMoves().Num(), 0);
	FMoverSyncState ExpiredState = History;
	TimeStep.BaseSimTimeMs = 300.0;
	Update.Settings = YanRadialAttractionTest::MakeCaptureSettings();
	Update.Settings.UpdateSequence = 5;
	Update.Settings.EntryVelocityDamping = 0.5f;
	Update.ApplyMovementEffect_Async(EffectParams, ExpiredState);
	Active = ExpiredState.LayeredMoves.GenerateActiveMoves_Async(TimeStep, nullptr);
	TestEqual(TEXT("到期后刷新只创建一个新的牵引"), Active.Num(), 1);
	if (Active.Num() != 1)
	{
		return false;
	}
	Move = static_cast<FYanLayeredMove_RadialAttraction*>(Active[0].Get());
	Move->GenerateMove_Async(YanRadialAttractionTest::MakeStartState(FVector(200.0, 0.0, 0.0), FVector(0.0, 200.0, 0.0)), TimeStep, nullptr, Proposed);
	TestEqual(TEXT("到期后重新进入采样新的速度"), Proposed.LinearVelocity.Y, 100.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYanRadialAttractionCaptureTest, "YanGame.Mover.RadialAttraction.Capture", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FYanRadialAttractionCaptureTest::RunTest(const FString& Parameters)
{
	for (const float StepMs : {5.0f, 10.0f, 25.0f})
	{
		FYanLayeredMove_RadialAttraction Move = YanRadialAttractionTest::MakeCaptureSettings();
		Move.CenterVelocity = FVector(100.0, 0.0, 0.0);
		Move.StartSimTimeMs = 0.0;
		Move.DurationMs = 1000.0f;
		FMoverTimeStep TimeStep;
		TimeStep.StepMs = StepMs;
		FVector Position(40.0, 0.0, 0.0);
		FVector Velocity = FVector::ZeroVector;
		for (int32 Index = 0; Index < FMath::RoundToInt(300.0f / StepMs); ++Index)
		{
			TimeStep.BaseSimTimeMs = Index * StepMs;
			FProposedMove Proposed;
			TestTrue(TEXT("内圈持续产生位移"), Move.GenerateMove_Async(YanRadialAttractionTest::MakeStartState(Position, Velocity), TimeStep, nullptr, Proposed));
			Velocity = Proposed.LinearVelocity;
			Position += Velocity * (StepMs * 0.001f);
		}
		TestEqual(FString::Printf(TEXT("%.0f ms 物理步下收拢并跟随移动中心"), StepMs), Position, FVector(30.0, 0.0, 0.0), 0.01f);
		TestEqual(TEXT("到位后保持中心速度"), Velocity, Move.CenterVelocity, 0.01f);
		TestFalse(TEXT("内圈捕获不因租期结束"), Move.IsFinished(100000.0));
	}

	FYanLayeredMove_RadialAttraction Move = YanRadialAttractionTest::MakeCaptureSettings();
	Move.StartSimTimeMs = 0.0;
	Move.DurationMs = 1000.0f;
	FMoverTimeStep TimeStep;
	TimeStep.BaseSimTimeMs = 0.0;
	TimeStep.StepMs = 100.0f;
	FProposedMove Proposed;
	Move.GenerateMove_Async(YanRadialAttractionTest::MakeStartState(FVector(55.0, 0.0, 0.0)), TimeStep, nullptr, Proposed);
	TestEqual(TEXT("外圈一步不跨过内圈边界"), Proposed.LinearVelocity, FVector(-50.0, 0.0, 0.0));
	TimeStep.StepMs = 10.0f;
	Move.GenerateMove_Async(YanRadialAttractionTest::MakeStartState(FVector(40.0, 0.0, 0.0)), TimeStep, nullptr, Proposed);
	TimeStep.BaseSimTimeMs = 20.0;
	Move.GenerateMove_Async(YanRadialAttractionTest::MakeStartState(FVector(20.0, 0.0, 0.0)), TimeStep, nullptr, Proposed);

	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes, true);
	Move.NetSerialize(Writer);
	FYanLayeredMove_RadialAttraction Restored;
	FMemoryReader Reader(Bytes, true);
	Restored.NetSerialize(Reader);
	TimeStep.BaseSimTimeMs = 30.0;
	const FMoverTickStartData ReplayStart = YanRadialAttractionTest::MakeStartState(FVector(15.0, 0.0, 0.0));
	FProposedMove Replayed;
	Move.GenerateMove_Async(ReplayStart, TimeStep, nullptr, Proposed);
	Restored.GenerateMove_Async(ReplayStart, TimeStep, nullptr, Replayed);
	TestFalse(TEXT("完整序列化无错误"), Writer.IsError() || Reader.IsError());
	TestEqual(TEXT("恢复后内圈进度重演一致"), Replayed.LinearVelocity, Proposed.LinearVelocity, 0.001f);
	Restored.GenerateMove_Async(YanRadialAttractionTest::MakeStartState(FVector(1015.0, 0.0, 0.0)), TimeStep, nullptr, Replayed);
	TestEqual(TEXT("收拢期间超出外圈仍追回轨迹"), Replayed.LinearVelocity - Proposed.LinearVelocity, FVector(-100000.0, 0.0, 0.0), 0.01f);
	Restored.GenerateMove_Async(ReplayStart, TimeStep, nullptr, Replayed);
	Restored.UpdateSequence = 2;
	Restored.Location = FVector(10.0, 0.0, 0.0);
	Move.Refresh(Restored, TimeStep.BaseSimTimeMs);
	Move.GenerateMove_Async(YanRadialAttractionTest::MakeStartState(FVector(25.0, 0.0, 0.0)), TimeStep, nullptr, Proposed);
	TestEqual(TEXT("移动中心刷新不重启收拢进度"), Proposed.LinearVelocity, Replayed.LinearVelocity, 0.001f);

	TimeStep.BaseSimTimeMs = 200.0;
	Move.GenerateMove_Async(YanRadialAttractionTest::MakeStartState(FVector(110.0, 0.0, 0.0)), TimeStep, nullptr, Proposed);
	TestEqual(TEXT("捕获后按位置误差追赶，不受外圈速度限制"), Proposed.LinearVelocity, FVector(-10000.0, 0.0, 0.0), 0.01f);
	TestTrue(TEXT("捕获后超出外圈仍产生牵引"), Move.GenerateMove_Async(YanRadialAttractionTest::MakeStartState(FVector(1000.0, 0.0, 0.0)), TimeStep, nullptr, Proposed));
	TestTrue(TEXT("捕获后超出外圈仍禁止落地接管"), Move.HasGameplayTag(Mover_DisableLanding, true));
	TestFalse(TEXT("捕获后超出外圈不结束"), Move.IsFinished(TimeStep.BaseSimTimeMs));
	TimeStep.BaseSimTimeMs = 10000.0;
	FYanLayeredMove_RadialAttraction ReentrySettings = YanRadialAttractionTest::MakeCaptureSettings();
	ReentrySettings.UpdateSequence = 3;
	ReentrySettings.Location = FVector(5010.0, 0.0, 0.0);
	ReentrySettings.CenterVelocity = FVector(20000.0, 0.0, 0.0);
	ReentrySettings.SnapshotSimTimeMs = TimeStep.BaseSimTimeMs;
	Move.Refresh(ReentrySettings, TimeStep.BaseSimTimeMs);
	TestFalse(TEXT("刷新中断后续期仍保持捕获"), Move.IsFinished(TimeStep.BaseSimTimeMs + 10000.0));
	const FVector LaggingPosition(1000.0, 0.0, 0.0);
	Move.GenerateMove_Async(YanRadialAttractionTest::MakeStartState(LaggingPosition), TimeStep, nullptr, Proposed);
	TestEqual(TEXT("高速中心一步追回，误差不积累"), LaggingPosition + Proposed.LinearVelocity * (TimeStep.StepMs * 0.001f), FVector(5210.0, 0.0, 0.0), 0.01f);
	ReentrySettings.UpdateSequence = 4;
	ReentrySettings.Radius = 0.0f;
	Move.Refresh(ReentrySettings, TimeStep.BaseSimTimeMs);
	TestTrue(TEXT("来源销毁取消可结束捕获"), Move.IsFinished(TimeStep.BaseSimTimeMs));
	TestFalse(TEXT("取消后不再牵引"), Move.GenerateMove_Async(YanRadialAttractionTest::MakeStartState(LaggingPosition), TimeStep, nullptr, Proposed));
	TestFalse(TEXT("取消后移除禁止落地标签"), Move.HasGameplayTag(Mover_DisableLanding, true));

	Move = YanRadialAttractionTest::MakeCaptureSettings();
	Move.StartSimTimeMs = TimeStep.BaseSimTimeMs;
	TestFalse(TEXT("未捕获目标仍可离开外圈"), Move.GenerateMove_Async(YanRadialAttractionTest::MakeStartState(LaggingPosition), TimeStep, nullptr, Proposed));
	ReentrySettings = YanRadialAttractionTest::MakeCaptureSettings();
	ReentrySettings.UpdateSequence = 2;
	Move.Refresh(ReentrySettings, TimeStep.BaseSimTimeMs);
	Move.GenerateMove_Async(YanRadialAttractionTest::MakeStartState(FVector(200.0, 0.0, 0.0)), TimeStep, nullptr, Proposed);
	TestEqual(TEXT("重新进入恢复外圈恒速吸引"), Proposed.LinearVelocity, FVector(-600.0, 0.0, 0.0));

	FYanLayeredMove_RadialAttraction Legacy;
	Legacy.Magnitude = 600.0f;
	Legacy.Radius = 500.0f;
	Legacy.GenerateMove_Async(YanRadialAttractionTest::MakeStartState(FVector(250.0, 0.0, 0.0)), TimeStep, nullptr, Proposed);
	TestEqual(TEXT("原径向牵引保持距离衰减"), Proposed.LinearVelocity, FVector(-300.0, 0.0, 0.0));
	TestTrue(TEXT("原径向牵引保持叠加策略"), Proposed.MixMode == EMoveMixMode::AdditiveVelocity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYanRadialAttractionLockLifecycleTest, "YanGame.Mover.RadialAttraction.LockLifecycle", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FYanRadialAttractionLockLifecycleTest::RunTest(const FString& Parameters)
{
	FMoverTimeStep TimeStep;
	TimeStep.BaseSimTimeMs = 0.0;
	TimeStep.StepMs = 10.0f;
	FApplyMovementEffectParams_Async EffectParams{};
	EffectParams.TimeStep = &TimeStep;
	FMoverSyncState State;
	FYanRadialAttractionUpdateEffect Update;
	Update.Settings = YanRadialAttractionTest::MakeCaptureSettings();
	Update.ApplyMovementEffect_Async(EffectParams, State);
	TArray<TSharedPtr<FLayeredMoveBase>> Active = State.LayeredMoves.GenerateActiveMoves_Async(TimeStep, nullptr);
	if (!TestEqual(TEXT("创建单一牵引来源"), Active.Num(), 1))
	{
		return false;
	}
	FProposedMove Proposed;
	Active[0]->GenerateMove_Async(YanRadialAttractionTest::MakeStartState(FVector(40.0, 0.0, 0.0)), TimeStep, nullptr, Proposed);
	TimeStep.BaseSimTimeMs = 20000.0;
	TestEqual(TEXT("不刷新时已捕获来源仍在活动组中"), State.LayeredMoves.GenerateActiveMoves_Async(TimeStep, nullptr).Num(), 1);
	Update.Settings.UpdateSequence = 2;
	Update.Settings.Location = FVector(10000.0, 0.0, 0.0);
	Update.Settings.SnapshotSimTimeMs = TimeStep.BaseSimTimeMs;
	Update.ApplyMovementEffect_Async(EffectParams, State);
	Active = State.LayeredMoves.GenerateActiveMoves_Async(TimeStep, nullptr);
	if (!TestEqual(TEXT("长时间中断后的刷新不重新创建来源"), Active.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("刷新后超出外圈仍保留捕获"), Active[0]->GenerateMove_Async(YanRadialAttractionTest::MakeStartState(FVector::ZeroVector), TimeStep, nullptr, Proposed));
	TestEqual(TEXT("刷新后跟随新球心"), Proposed.LinearVelocity, FVector(1000000.0, 0.0, 0.0), 0.1f);
	FMoverSyncState History = State;
	Update.Settings.UpdateSequence = 3;
	Update.Settings.Radius = 0.0f;
	Update.ApplyMovementEffect_Async(EffectParams, State);
	TestEqual(TEXT("来源销毁动作从活动组移除锁定"), State.LayeredMoves.GenerateActiveMoves_Async(TimeStep, nullptr).Num(), 0);
	TestEqual(TEXT("取消不污染回滚历史中的捕获"), History.LayeredMoves.GenerateActiveMoves_Async(TimeStep, nullptr).Num(), 1);
	return true;
}

#endif
