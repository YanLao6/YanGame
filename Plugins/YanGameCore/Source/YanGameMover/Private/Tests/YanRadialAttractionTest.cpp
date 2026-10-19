#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "MoveLibrary/AirMovementUtils.h"
#include "MoverDataModelTypes.h"
#include "MoverLayeredMove/YanRadialAttractionMove.h"
#include "MoverMode/Attraction/ChaosAttractionMode.h"
#include "MoverMode/Attraction/YanAttractionState.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if WITH_EDITOR
#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/Character/Settings/SharedChaosCharacterMovementSettings.h"
#include "MoverComponent.h"
#include "UObject/UnrealType.h"
#endif

namespace YanRadialAttractionTest
{
	FYanAttractionSettings MakeSettings()
	{
		FYanAttractionSettings Settings;
		Settings.SourceId       = 1;
		Settings.UpdateSequence = 1;
		return Settings;
	}

	FMoverTickStartData MakeStart(const FVector& Location)
	{
		FMoverTickStartData Start;
		Start.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>().SetTransforms_WorldSpace(Location, FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
		return Start;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYanAttractionRefreshTest, "YanGame.Mover.RadialAttraction.Refresh", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FYanAttractionRefreshTest::RunTest(const FString& Parameters)
{
	FMoverTimeStep Time;
	Time.BaseSimTimeMs = 0.0;
	Time.StepMs        = 10.0f;
	FApplyMovementEffectParams_Async Params{};
	Params.TimeStep                        = &Time;
	FMoverSyncState                  State = YanRadialAttractionTest::MakeStart(FVector(40.0, 0.0, 0.0)).SyncState;
	FYanRadialAttractionUpdateEffect Update;
	Update.Settings = YanRadialAttractionTest::MakeSettings();
	TestTrue(TEXT("首次写入牵引意图"), Update.ApplyMovementEffect_Async(Params, State));
	FYanAttractionState& Control = *State.SyncStateCollection.FindMutableDataByType<FYanAttractionState>();
	Control.UpdateCapture(FVector(40.0, 0.0, 0.0), 0.0);
	FMoverSyncState History;
	History                        = State;
	Update.Settings.UpdateSequence = 2;
	Update.Settings.Location       = FVector(1000.0, 1000.0, 1000.0);
	TestTrue(TEXT("刷新中心"), Update.ApplyMovementEffect_Async(Params, State));
	TestEqual(TEXT("同源刷新只有一个记录"), Control.Sources.Num(), 1);
	TestEqual(TEXT("刷新不写入角色位置"), State.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>()->GetLocation_WorldSpace(), FVector(40.0, 0.0, 0.0));
	TestEqual(TEXT("刷新不写入角色速度"), State.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>()->GetVelocity_WorldSpace(), FVector::ZeroVector);
	TestEqual(TEXT("刷新不污染回滚历史"), History.SyncStateCollection.FindDataByType<FYanAttractionState>()->Sources[0].Settings.Location, FVector::ZeroVector);
	Update.Settings.UpdateSequence = 1;
	TestFalse(TEXT("同源旧序号被忽略"), Update.ApplyMovementEffect_Async(Params, State));
	Update.Settings.SourceId = 2;
	Update.Settings.Location = FVector::ZeroVector;
	Update.ApplyMovementEffect_Async(Params, State);
	TestEqual(TEXT("第二来源独立记录"), Control.Sources.Num(), 2);
	TestEqual(TEXT("新来源不抢占已捕获来源"), Control.SelectSource(FVector::ZeroVector, 0.0)->Settings.SourceId, 1);
	Update.Settings.SourceId       = 1;
	Update.Settings.UpdateSequence = 3;
	Update.Settings.Radius         = 0.0f;
	Update.ApplyMovementEffect_Async(Params, State);
	TestEqual(TEXT("释放仅删除指定来源"), Control.Sources.Num(), 1);
	TestEqual(TEXT("释放不污染历史捕获关系"), History.SyncStateCollection.FindDataByType<FYanAttractionState>()->Sources.Num(), 1);
	TestTrue(TEXT("历史捕获保持有效"), History.SyncStateCollection.FindDataByType<FYanAttractionState>()->Sources[0].bCaptured);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYanAttractionLifecycleTest, "YanGame.Mover.RadialAttraction.LockLifecycle", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FYanAttractionLifecycleTest::RunTest(const FString& Parameters)
{
	FYanAttractionState    State;
	FYanAttractionSettings Settings = YanRadialAttractionTest::MakeSettings();
	State.Refresh(Settings, 0.0);
	TestNull(TEXT("外圈之外不进入"), State.SelectSource(FVector(501.0, 0.0, 0.0), 0.0));
	State.UpdateCapture(FVector(200.0, 0.0, 0.0), 0.0);
	TestFalse(TEXT("外圈拉近尚未锁定"), State.Sources[0].bCaptured);
	TestNull(TEXT("外圈允许离开"), State.SelectSource(FVector(501.0, 0.0, 0.0), 0.0));
	TestNull(TEXT("未捕获来源到期后无效"), State.SelectSource(FVector(200.0, 0.0, 0.0), 150.0));
	State.UpdateCapture(FVector(40.0, 0.0, 0.0), 10.0);
	TestTrue(TEXT("进入内圈锁定"), State.Sources[0].bCaptured);
	TestNotNull(TEXT("锁定后离开范围及租期仍跟随"), State.SelectSource(FVector(10000.0, 0.0, 0.0), 10000.0));
	Settings.UpdateSequence = 2;
	Settings.Location       = FVector(2000.0, 0.0, 0.0);
	State.Refresh(Settings, 10000.0);
	TestTrue(TEXT("刷新不重置锁定"), State.Sources[0].bCaptured);

	UChaosAttractionEnterCheck* Enter = NewObject<UChaosAttractionEnterCheck>();
	FSimulationTickParams       Params;
	Params.TimeStep.BaseSimTimeMs                                                                     = 10000.0;
	Params.TimeStep.StepMs                                                                            = 10.0f;
	Params.StartState                                                                                 = YanRadialAttractionTest::MakeStart(FVector::ZeroVector);
	Params.StartState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FYanAttractionState>() = State;
	TestTrue(TEXT("未配置模式名称时不切换"), Enter->Evaluate_Implementation(Params).NextMode.IsNone());
	Enter->TransitionToMode = FName(TEXT("ConfiguredByBlueprint"));
	TestEqual(TEXT("使用蓝图配置的模式名称"), Enter->Evaluate_Implementation(Params).NextMode, Enter->TransitionToMode);
	Params.StartState.SyncState.MovementMode = Enter->TransitionToMode;
	TestTrue(TEXT("全局进入检查不会反复重入"), Enter->Evaluate_Implementation(Params).NextMode.IsNone());

	Settings.UpdateSequence = 3;
	Settings.Radius         = 0.0f;
	State.Refresh(Settings, 10000.0);
	TestNull(TEXT("释放后没有牵引"), State.SelectSource(FVector::ZeroVector, 10000.0));
	Params.StartState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FYanAttractionState>() = State;
	UChaosAttractionMode* Mode                                                                        = NewObject<UChaosAttractionMode>();
	FMoverTickEndData     End(&Params.StartState.SyncState, &Params.StartState.AuxState);
	Mode->SimulationTick_Implementation(Params, End);
	TestEqual(TEXT("释放返回配置的退出模式"), End.MovementEndState.NextModeName, Mode->ExitMovementMode);
	TestEqual(TEXT("释放将整步交给正常移动"), End.MovementEndState.RemainingMs, Params.TimeStep.StepMs);
	Params.StartState.SyncState.MovementMode = Mode->ExitMovementMode;
	TestTrue(TEXT("释放后进入检查不再触发"), Enter->Evaluate_Implementation(Params).NextMode.IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYanAttractionFlightTest, "YanGame.Mover.RadialAttraction.Flight", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FYanAttractionFlightTest::RunTest(const FString& Parameters)
{
	UChaosAttractionMode* Mode = NewObject<UChaosAttractionMode>();
	for (const float StepMs : {5.0f, 10.0f, 25.0f})
	{
		FYanAttractionSource Source;
		Source.Settings          = YanRadialAttractionTest::MakeSettings();
		Source.Settings.Location = FVector(1000.0, 1000.0, 1000.0);
		Source.bCaptured         = true;
		FVector Position         = FVector::ZeroVector;
		FVector Velocity         = FVector::ZeroVector;
		for (int32 Index = 0; Index < FMath::RoundToInt(2000.0f / StepMs); ++Index)
		{
			const double    SimTimeMs = Index * StepMs;
			FFreeMoveParams Flight;
			Flight.MoveInputType     = EMoveInputType::DirectionalIntent;
			Flight.MoveInput         = Mode->ComputeMoveInput(Source, Position, SimTimeMs, 800.0f);
			Flight.PriorVelocity     = Velocity;
			Flight.MaxSpeed          = 800.0f;
			Flight.Acceleration      = 4000.0f;
			Flight.Deceleration      = 8000.0f;
			Flight.DeltaSeconds      = StepMs * 0.001f;
			const FProposedMove Move = UAirMovementUtils::ComputeControlledFreeMove(Flight);
			Velocity                 = Move.LinearVelocity;
			const FVector Delta      = Velocity * Flight.DeltaSeconds;
			TestTrue(TEXT("每个物理步位移受速度限制"), Delta.Size() <= Source.Settings.PullSpeed * Flight.DeltaSeconds + 0.001);
			TestTrue(TEXT("同一外部快照在连续物理步产生移动"), Delta.X > 0.0 && Delta.Y > 0.0 && Delta.Z > 0.0);
			Position += Delta;
		}
		TestTrue(TEXT("两秒后仍未瞬间贴到远处球心"), FVector::Distance(Position, Source.Settings.Location) > 100.0);
	}

	FYanAttractionSource Source;
	Source.Settings                = YanRadialAttractionTest::MakeSettings();
	Source.Settings.Location       = FVector(10000.0, 0.0, 0.0);
	Source.Settings.CenterVelocity = FVector(20000.0, 0.0, 0.0);
	TestTrue(TEXT("中心跳变及高速快照仍只产生有界输入"), Mode->ComputeMoveInput(Source, FVector::ZeroVector, 10.0, 800.0f).Size() <= 0.75001);
	Source.Settings.CenterVelocity = FVector::ZeroVector;
	TestTrue(TEXT("到达中心时输入归零"), Mode->ComputeMoveInput(Source, Source.Settings.Location, 10.0, 800.0f).IsNearlyZero());
	const FVector NearInput = Mode->ComputeMoveInput(Source, Source.Settings.Location - FVector(10.0, 0.0, 0.0), 10.0, 800.0f);
	TestTrue(TEXT("接近中心时减小输入幅度"), NearInput.Size() > 0.0 && NearInput.Size() < 0.1);
	Source.Settings.CenterVelocity = FVector(100.0, 0.0, 0.0);
	TestEqual(TEXT("旧快照外推在租期末停止"), Source.Settings.GetCenter(10000.0), Source.Settings.GetCenter(Source.Settings.DurationMs));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYanAttractionSerializationTest, "YanGame.Mover.RadialAttraction.Serialization", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FYanAttractionSerializationTest::RunTest(const FString& Parameters)
{
	FYanAttractionState    State;
	FYanAttractionSettings Settings = YanRadialAttractionTest::MakeSettings();
	Settings.Location               = FVector(10.25, 20.5, 30.75);
	Settings.CenterVelocity         = FVector(100.0, 200.0, 300.0);
	State.Refresh(Settings, 0.0);
	State.UpdateCapture(Settings.Location, 0.0);
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes, true);
	bool          bSuccess = false;
	State.NetSerialize(Writer, nullptr, bSuccess);
	TestTrue(TEXT("状态写出成功"), bSuccess);
	FMemoryReader       Reader(Bytes, true);
	FYanAttractionState Restored;
	Restored.NetSerialize(Reader, nullptr, bSuccess);
	TestTrue(TEXT("状态读回成功"), bSuccess);
	TestFalse(TEXT("序列化往返不产生纠正"), State.ShouldReconcile(Restored));
	TestTrue(TEXT("恢复后仍锁定"), Restored.Sources[0].bCaptured);
	UChaosAttractionMode* Mode = NewObject<UChaosAttractionMode>();
	TestEqual(TEXT("恢复后飞行输入可重演"), Mode->ComputeMoveInput(State.Sources[0], FVector::ZeroVector, 50.0, 800.0f), Mode->ComputeMoveInput(Restored.Sources[0], FVector::ZeroVector, 50.0, 800.0f));
	Restored.Sources[0].bCaptured = false;
	TestTrue(TEXT("锁定状态不同触发纠正"), State.ShouldReconcile(Restored));
	Restored                               = State;
	Restored.Sources[0].Settings.PullSpeed += 1.0f;
	TestTrue(TEXT("控制参数不同触发纠正"), State.ShouldReconcile(Restored));

	FYanRadialAttractionUpdateEffect Effect;
	Effect.Settings = Settings;
	Bytes.Reset();
	FMemoryWriter EffectWriter(Bytes, true);
	Effect.NetSerialize(EffectWriter);
	FYanRadialAttractionUpdateEffect RestoredEffect;
	FMemoryReader                    EffectReader(Bytes, true);
	RestoredEffect.NetSerialize(EffectReader);
	TestFalse(TEXT("动作序列化无错误"), EffectWriter.IsError() || EffectReader.IsError());
	TestTrue(TEXT("动作保留全部控制数据"), Effect.Settings == RestoredEffect.Settings);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYanRadialAttractionLegacyTest, "YanGame.Mover.RadialAttraction.Additive", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FYanRadialAttractionLegacyTest::RunTest(const FString& Parameters)
{
	FYanLayeredMove_RadialAttraction Move;
	Move.Magnitude = 600.0f;
	Move.Radius    = 500.0f;
	FMoverTimeStep Time;
	Time.StepMs = 10.0f;
	FProposedMove Proposed;
	Move.GenerateMove_Async(YanRadialAttractionTest::MakeStart(FVector(250.0, 0.0, 0.0)), Time, nullptr, Proposed);
	TestEqual(TEXT("普通拉扯保持距离衰减"), Proposed.LinearVelocity, FVector(-300.0, 0.0, 0.0));
	TestTrue(TEXT("普通拉扯仍叠加自身移动"), Proposed.MixMode == EMoveMixMode::AdditiveVelocity);
	Move.bIsPush = true;
	Move.GenerateMove_Async(YanRadialAttractionTest::MakeStart(FVector(250.0, 0.0, 0.0)), Time, nullptr, Proposed);
	TestEqual(TEXT("推开方向保留"), Proposed.LinearVelocity, FVector(300.0, 0.0, 0.0));
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYanAttractionInputIsolationTest, "YanGame.Mover.RadialAttraction.InputIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FYanAttractionInputIsolationTest::RunTest(const FString& Parameters)
{
	/*UMoverComponent* Mover = NewObject<UMoverComponent>();
	UChaosAttractionMode* Mode = NewObject<UChaosAttractionMode>(Mover);
	const FName ModeName(TEXT("TestAttraction"));
	Mover->MovementModes.Add(ModeName, Mode);
	FPropertyChangedEvent Changed(FindFProperty<FMapProperty>(UMoverComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(UMoverComponent, MovementModes)));
	static_cast<UObject*>(Mover)->PostEditChangeProperty(Changed);
	USharedChaosCharacterMovementSettings* Settings = Mover->FindSharedSettings_Mutable<USharedChaosCharacterMovementSettings>();
	if (!TestNotNull(TEXT("蓝图属性变更创建飞行共享设置"), Settings))
	{
		return false;
	}
	Settings->MaxSpeed = 800.0f;
	Settings->Acceleration = 4000.0f;
	UChaosMoverSimulation* Simulation = NewObject<UChaosMoverSimulation>();
	FChaosMoverSimulationDefaultInputs& LocalInputs = Simulation->GetLocalSimInput_Mutable().FindOrAddMutableDataByType<FChaosMoverSimulationDefaultInputs>();
	LocalInputs.UpDir = FVector::UpVector;
	LocalInputs.PhysicsObjectGravity = 0.0f;
	FMoverSimContext Context;
	Context.Simulation = Simulation;
	Context.SimulationOwner = Mover;
	Mode->SetSimulation(Simulation);
	Mode->OnRegistered(ModeName, Context);

	FMoverTickStartData Start = YanRadialAttractionTest::MakeStart(FVector::ZeroVector);
	Start.SyncState.MovementMode = ModeName;
	FCharacterDefaultInputs& PlayerInput = Start.InputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	PlayerInput.SetMoveInput(EMoveInputType::DirectionalIntent, -FVector::YAxisVector);
	PlayerInput.ControlRotation = FRotator(20.0, 60.0, 0.0);
	PlayerInput.bIsJumpPressed = true;
	const FCharacterDefaultInputs OriginalInput = PlayerInput;
	FYanAttractionSettings Source = YanRadialAttractionTest::MakeSettings();
	Source.Location = FVector(200.0, 0.0, 0.0);
	Start.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FYanAttractionState>().Refresh(Source, 0.0);
	FMoverTimeStep Time;
	Time.BaseSimTimeMs = 0.0;
	Time.StepMs = 10.0f;
	FProposedMove Proposed;
	Mode->GenerateMove_Implementation(Context, Start, Time, Proposed);
	TestTrue(TEXT("外部输入生成朝向球心的实际飞行速度"), Proposed.LinearVelocity.X > 0.0 && FMath::IsNearlyZero(Proposed.LinearVelocity.Y));
	TestTrue(TEXT("原始客户端输入包保持不变"), OriginalInput == *Start.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>());
	TestTrue(TEXT("实际提议受牵引速度限制"), Proposed.LinearVelocity.Size() <= Source.PullSpeed);
	Mode->OnUnregistered(Context);*/
	return true;
}
#endif

#endif
