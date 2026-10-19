#include "Misc/AutomationTest.h"
#include "MoverDataModelTypes.h"
#include "MoverMode/Grappling/ChaosGrapplingEnterCheck.h"
#include "MoverMode/Grappling/ChaosGrapplingExitCheck.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "YanMoverDataModelTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYanGrapplingExitCheckTest, "YanGame.Mover.GrapplingExitCheck", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FYanGrapplingExitCheckTest::RunTest(const FString& Parameters)
{
	UChaosGrapplingExitCheck* ExitCheck = NewObject<UChaosGrapplingExitCheck>();
	UChaosGrapplingEnterCheck* EnterCheck = NewObject<UChaosGrapplingEnterCheck>();
	FSimulationTickParams Params;
	FMoverDefaultSyncState& SyncState = Params.StartState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	FYanCharacterInputs& Inputs = Params.StartState.InputCmd.InputCollection.FindOrAddMutableDataByType<FYanCharacterInputs>();
	FCharacterDefaultInputs& CharacterInputs = Params.StartState.InputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	const FVector CapsuleLocation(100.0, 200.0, 300.0);
	const FRotator CapsuleOrientation(0.0, 37.0, 0.0);
	SyncState.SetTransforms_WorldSpace(CapsuleLocation, CapsuleOrientation, FVector::ZeroVector, FVector::ZeroVector);
	CharacterInputs.ControlRotation = CapsuleOrientation;
	Inputs.bIsGrapplingActive = true;

	for (const double Angle : {-179.0, -91.0, -90.0, -89.0, 0.0, 89.0, 90.0, 91.0, 179.0})
	{
		Inputs.GrappleHookLocation = CapsuleLocation + FRotator(0.0, CapsuleOrientation.Yaw + Angle, 0.0).Vector() * 1000.0;
		// 汇合点故意放在反方向，验证退出依据钩子而非牵引终点。
		Inputs.GrappleAnchorPoint = CapsuleLocation - CapsuleOrientation.Vector() * 500.0;
		const bool bShouldExit = FMath::Abs(Angle) >= 90.0;
		TestEqual(FString::Printf(TEXT("夹角 %.0f° 的退出模式"), Angle), ExitCheck->Evaluate_Implementation(Params).NextMode,
			bShouldExit ? ExitCheck->TransitionToMode : NAME_None);
		TestEqual(FString::Printf(TEXT("夹角 %.0f° 的进入模式"), Angle), EnterCheck->Evaluate_Implementation(Params).NextMode,
			bShouldExit ? NAME_None : EnterCheck->TransitionToMode);
	}

	Inputs.GrappleHookLocation = CapsuleLocation;
	TestEqual(TEXT("到达钩子后退出"), ExitCheck->Evaluate_Implementation(Params).NextMode, ExitCheck->TransitionToMode);
	Inputs.GrappleHookLocation = CapsuleLocation + CapsuleOrientation.Vector() * 1000.0;
	Inputs.bIsGrapplingActive = false;
	TestEqual(TEXT("松键或超时清除意图后退出"), ExitCheck->Evaluate_Implementation(Params).NextMode, ExitCheck->TransitionToMode);
	TestEqual(TEXT("没有牵引意图时不进入"), EnterCheck->Evaluate_Implementation(Params).NextMode, NAME_None);
	Inputs.bGrapplePullsTarget = true;
	Inputs.GrappleHookLocation = CapsuleLocation - CapsuleOrientation.Vector() * 1000.0;
	TestTrue(TEXT("仅拉被钩者时也能判定方向失效"), UChaosGrapplingExitCheck::IsHookOutsideForwardHemisphere(Params.StartState.SyncState, Params.StartState.InputCmd));

	Inputs.bIsGrapplingActive = true;
	CharacterInputs.ControlRotation.Yaw = 0.0;
	SyncState.SetTransforms_WorldSpace(CapsuleLocation, FRotator(0.0, 180.0, 0.0), FVector::ZeroVector, FVector::ZeroVector);
	for (const double Pitch : {-90.0, -45.0, 0.0, 45.0, 90.0})
	{
		CharacterInputs.ControlRotation.Pitch = Pitch;
		for (const double HookHeight : {-1000000.0, 0.0, 1000000.0})
		{
			Inputs.GrappleHookLocation = CapsuleLocation + FVector(100.0, 0.0, HookHeight);
			TestTrue(TEXT("视角前方有效，不受 Pitch、高度或胶囊朝向影响"), ExitCheck->Evaluate_Implementation(Params).NextMode.IsNone());
			Inputs.GrappleHookLocation = CapsuleLocation + FVector(-100.0, 0.0, HookHeight);
			TestEqual(TEXT("视角后方退出，不受 Pitch 或高度影响"), ExitCheck->Evaluate_Implementation(Params).NextMode, ExitCheck->TransitionToMode);
		}
	}
	CharacterInputs.ControlRotation = FRotator(90.0, 180.0, 0.0);
	Inputs.GrappleHookLocation = CapsuleLocation + FVector(100.0, 0.0, 100.0);
	TestEqual(TEXT("只旋转视角 Yaw 即改变分界面方向"), ExitCheck->Evaluate_Implementation(Params).NextMode, ExitCheck->TransitionToMode);
	for (const double HookHeight : {-1000.0, 1000.0})
	{
		Inputs.GrappleHookLocation = CapsuleLocation + FVector(0.0, 0.0, HookHeight);
		TestEqual(TEXT("正上方和正下方位于竖直分界面上"), ExitCheck->Evaluate_Implementation(Params).NextMode, ExitCheck->TransitionToMode);
	}

	FSimulationTickParams MissingViewParams;
	MissingViewParams.StartState.SyncState = Params.StartState.SyncState;
	MissingViewParams.StartState.InputCmd.InputCollection.FindOrAddMutableDataByType<FYanCharacterInputs>().bIsGrapplingActive = true;
	TestEqual(TEXT("缺少视角输入时退出"), ExitCheck->Evaluate_Implementation(MissingViewParams).NextMode, ExitCheck->TransitionToMode);
	TestEqual(TEXT("缺少视角输入时不进入"), EnterCheck->Evaluate_Implementation(MissingViewParams).NextMode, NAME_None);

	FSimulationTickParams EmptyParams;
	TestEqual(TEXT("缺少输入时退出"), ExitCheck->Evaluate_Implementation(EmptyParams).NextMode, ExitCheck->TransitionToMode);
	EmptyParams.StartState.InputCmd.InputCollection.FindOrAddMutableDataByType<FYanCharacterInputs>().bIsGrapplingActive = true;
	TestEqual(TEXT("缺少同步状态时退出"), ExitCheck->Evaluate_Implementation(EmptyParams).NextMode, ExitCheck->TransitionToMode);
	TestEqual(TEXT("缺少同步状态时不进入"), EnterCheck->Evaluate_Implementation(EmptyParams).NextMode, NAME_None);

	FYanCharacterInputs SourceInputs;
	SourceInputs.bIsGrapplingActive = true;
	SourceInputs.GrappleHookLocation = FVector(123.0, 456.0, 789.0);
	TArray<uint8> Bytes;
	bool bSuccess = false;
	FMemoryWriter Writer(Bytes);
	SourceInputs.NetSerialize(Writer, nullptr, bSuccess);
	TestTrue(TEXT("钩子位置序列化成功"), bSuccess && !Writer.IsError());
	FYanCharacterInputs ReceivedInputs;
	FMemoryReader Reader(Bytes);
	ReceivedInputs.NetSerialize(Reader, nullptr, bSuccess);
	TestTrue(TEXT("钩子位置反序列化成功"), bSuccess && !Reader.IsError());
	TestTrue(TEXT("钩子位置网络往返保持一致"), ReceivedInputs.GrappleHookLocation.Equals(SourceInputs.GrappleHookLocation));
	TestFalse(TEXT("网络往返无需校正"), SourceInputs.ShouldReconcile(ReceivedInputs));
	ReceivedInputs.GrappleHookLocation.X += 10.0;
	TestTrue(TEXT("钩子位置不一致时需要校正"), SourceInputs.ShouldReconcile(ReceivedInputs));

	FYanCharacterInputs MergedInputs;
	MergedInputs.Merge(SourceInputs);
	TestTrue(TEXT("牵引意图合并时保留钩子位置"), MergedInputs.GrappleHookLocation.Equals(SourceInputs.GrappleHookLocation));
	FYanCharacterInputs UnrelatedInputs;
	UnrelatedInputs.GrappleHookLocation = FVector(-100.0);
	MergedInputs.Merge(UnrelatedInputs);
	TestTrue(TEXT("其他输入源不覆盖钩子位置"), MergedInputs.GrappleHookLocation.Equals(SourceInputs.GrappleHookLocation));
	FYanCharacterInputs InterpolatedInputs;
	InterpolatedInputs.Interpolate(SourceInputs, ReceivedInputs, 0.75f);
	TestTrue(TEXT("插值采用所选输入帧的钩子位置"), InterpolatedInputs.GrappleHookLocation.Equals(ReceivedInputs.GrappleHookLocation));

	return true;
}

#endif
