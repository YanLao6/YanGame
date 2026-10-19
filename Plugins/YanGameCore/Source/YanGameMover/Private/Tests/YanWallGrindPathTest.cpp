#include "Misc/AutomationTest.h"
#include "MoverLayeredMove/YanWallGrindPath.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 贴墙推进路径求值的验证。
 *
 * 推进双方靠共用这组纯函数保持相对位置守恒，故其契约必须逐条钉死：
 * 弧长起止对得上、行进距离单调不倒退、路径坐标系偏移在切线竖直时不退化。
 * 这些性质与 Mover、网络、Actor 均无关，因而可脱离 PIE 单独验证。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYanWallGrindPathTest, "YanGame.Mover.WallGrindPath", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FYanWallGrindPathTest::RunTest(const FString& Parameters)
{
	// 一条带直角拐弯的折线，总长 200：既覆盖段内插值，也覆盖跨段与切线突变
	const TArray<FVector> BentPath = {
		FVector(0.0, 0.0, 0.0),
		FVector(100.0, 0.0, 0.0),
		FVector(100.0, 100.0, 0.0)};

	const double TotalLength = YanWallGrindPath::ComputeTotalLength(BentPath);
	TestEqual(TEXT("折线总弧长"), TotalLength, 200.0, UE_DOUBLE_KINDA_SMALL_NUMBER);

	{
		FVector Location;
		FVector Tangent;

		TestTrue(TEXT("弧长 0 求值成功"), YanWallGrindPath::EvaluateAtDistance(BentPath, 0.0, Location, Tangent));
		TestTrue(TEXT("弧长 0 落在折线首点"), Location.Equals(BentPath[0]));
		TestTrue(TEXT("弧长 0 的切线沿首段"), Tangent.Equals(FVector(1.0, 0.0, 0.0)));

		YanWallGrindPath::EvaluateAtDistance(BentPath, 50.0, Location, Tangent);
		TestTrue(TEXT("段内弧长线性插值"), Location.Equals(FVector(50.0, 0.0, 0.0)));

		YanWallGrindPath::EvaluateAtDistance(BentPath, 150.0, Location, Tangent);
		TestTrue(TEXT("跨段后落在第二段"), Location.Equals(FVector(100.0, 50.0, 0.0)));
		TestTrue(TEXT("跨段后切线随之转向"), Tangent.Equals(FVector(0.0, 1.0, 0.0)));

		YanWallGrindPath::EvaluateAtDistance(BentPath, TotalLength, Location, Tangent);
		TestTrue(TEXT("弧长等于总长时落在末点"), Location.Equals(BentPath.Last()));

		YanWallGrindPath::EvaluateAtDistance(BentPath, TotalLength * 10.0, Location, Tangent);
		TestTrue(TEXT("弧长超出总长时钳制在末点"), Location.Equals(BentPath.Last()));

		TestFalse(TEXT("点数不足两点时求值失败"), YanWallGrindPath::EvaluateAtDistance(TArray<FVector>{FVector::ZeroVector}, 10.0, Location, Tangent));
	}

	// 速度先耗尽：停止距离 80 小于折线总长，推进止于 s0/a
	{
		constexpr double EntrySpeed   = 400.0;
		constexpr double Deceleration = 1000.0;

		TestEqual(TEXT("停止距离 s0^2/(2a)"), YanWallGrindPath::ComputeStopDistance(EntrySpeed, Deceleration), 80.0, UE_DOUBLE_KINDA_SMALL_NUMBER);

		const double Duration = YanWallGrindPath::ComputeDuration(EntrySpeed, Deceleration, TotalLength);
		TestEqual(TEXT("速度受限时推进时长为 s0/a"), Duration, 0.4, 1e-6);

		TestEqual(TEXT("起滑瞬间行进距离为零"), YanWallGrindPath::ComputeTraveledDistance(EntrySpeed, Deceleration, 0.0, TotalLength), 0.0, UE_DOUBLE_KINDA_SMALL_NUMBER);
		TestEqual(TEXT("终点行进距离等于停止距离"), YanWallGrindPath::ComputeTraveledDistance(EntrySpeed, Deceleration, Duration, TotalLength), 80.0, 1e-4);

		// 越过抛物线顶点后若不钳制，位移会掉头倒退
		TestEqual(TEXT("超出时长后行进距离不再变化"), YanWallGrindPath::ComputeTraveledDistance(EntrySpeed, Deceleration, Duration * 5.0, TotalLength), 80.0, 1e-4);
	}

	// 折线先走完：停止距离 500 大于总长，推进止于走完全程
	{
		constexpr double EntrySpeed   = 1000.0;
		constexpr double Deceleration = 1000.0;

		const double Duration = YanWallGrindPath::ComputeDuration(EntrySpeed, Deceleration, TotalLength);
		TestTrue(TEXT("路径受限时推进时长短于自然停止时长"), Duration < EntrySpeed / Deceleration);
		TestEqual(TEXT("路径受限时终点行进距离等于总长"), YanWallGrindPath::ComputeTraveledDistance(EntrySpeed, Deceleration, Duration, TotalLength), TotalLength, 1e-4);

		// 单调性是相对位置守恒的前提：任一步倒退都会让双方的进度错开
		double PreviousDistance = -1.0;
		for (int32 SampleIndex = 0; SampleIndex <= 100; ++SampleIndex)
		{
			const double SampleTime = Duration * SampleIndex / 100.0;
			const double Distance   = YanWallGrindPath::ComputeTraveledDistance(EntrySpeed, Deceleration, SampleTime, TotalLength);

			TestTrue(TEXT("行进距离单调不减"), Distance >= PreviousDistance - UE_DOUBLE_KINDA_SMALL_NUMBER);
			PreviousDistance = Distance;
		}
	}

	// 减速度为零退化为匀速，不得除零
	{
		const double Duration = YanWallGrindPath::ComputeDuration(500.0, 0.0, TotalLength);
		TestEqual(TEXT("无减速时推进时长为 L/s0"), Duration, 0.4, 1e-6);
		TestEqual(TEXT("无减速时终点行进距离等于总长"), YanWallGrindPath::ComputeTraveledDistance(500.0, 0.0, Duration, TotalLength), TotalLength, 1e-4);
	}

	// 路径坐标系偏移
	{
		const FVector HorizontalTangent = FVector(1.0, 0.0, 0.0);
		const FVector OutwardOffset     = YanWallGrindPath::TransformFrameOffset(HorizontalTangent, FVector(0.0, 50.0, 0.0));

		TestEqual(TEXT("外向偏移保长"), OutwardOffset.Size(), 50.0, 1e-4);
		TestEqual(TEXT("外向偏移垂直于切线"), FVector::DotProduct(OutwardOffset.GetSafeNormal(), HorizontalTangent), 0.0, 1e-6);
		TestEqual(TEXT("外向偏移落在水平面内"), OutwardOffset.Z, 0.0, 1e-6);

		// 切线竖直时外向的首选参考轴退化，须由兜底轴接管而非产生零向量
		const FVector VerticalTangent = FVector(0.0, 0.0, 1.0);
		const FVector FallbackOffset  = YanWallGrindPath::TransformFrameOffset(VerticalTangent, FVector(0.0, 50.0, 0.0));

		TestFalse(TEXT("切线竖直时外向不退化为零"), FallbackOffset.IsNearlyZero());
		TestEqual(TEXT("切线竖直时外向仍保长"), FallbackOffset.Size(), 50.0, 1e-4);
		TestTrue(TEXT("切线竖直时外向不含 NaN"), !FallbackOffset.ContainsNaN());

		const FVector TangentialOffset = YanWallGrindPath::TransformFrameOffset(HorizontalTangent, FVector(30.0, 0.0, 0.0));
		TestTrue(TEXT("切向偏移沿切线"), TangentialOffset.Equals(HorizontalTangent * 30.0));

		const FVector UpOffset = YanWallGrindPath::TransformFrameOffset(HorizontalTangent, FVector(0.0, 0.0, 20.0));
		TestTrue(TEXT("竖向偏移沿世界上"), UpOffset.Equals(FVector(0.0, 0.0, 20.0)));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
