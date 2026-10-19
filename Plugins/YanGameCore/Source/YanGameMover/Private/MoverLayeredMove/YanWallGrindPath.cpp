#include "MoverLayeredMove/YanWallGrindPath.h"

namespace YanWallGrindPath
{
	namespace
	{
		// 叉积退化阈值：低于此长度即认为两向量近乎平行，须换一根参考轴
		constexpr double AxisDegenerateThreshold = 1e-3;
	}

	double ComputeTotalLength(TConstArrayView<FVector> Path)
	{
		double TotalLength = 0.0;
		for (int32 PointIndex = 1; PointIndex < Path.Num(); ++PointIndex)
		{
			TotalLength += FVector::Distance(Path[PointIndex - 1], Path[PointIndex]);
		}
		return TotalLength;
	}

	double ComputeStopDistance(double EntrySpeed, double Deceleration)
	{
		if (Deceleration <= 0.0)
		{
			return TNumericLimits<double>::Max();
		}
		return FMath::Square(FMath::Max(EntrySpeed, 0.0)) / (2.0 * Deceleration);
	}

	double ComputeDuration(double EntrySpeed, double Deceleration, double TotalLength)
	{
		const double Speed = FMath::Max(EntrySpeed, 0.0);
		if (Speed <= 0.0 || TotalLength <= 0.0)
		{
			return 0.0;
		}

		if (Deceleration <= 0.0)
		{
			return TotalLength / Speed;
		}

		// 速度耗尽先于走完折线：取自然停止时刻
		if (ComputeStopDistance(Speed, Deceleration) <= TotalLength)
		{
			return Speed / Deceleration;
		}

		// 折线先走完：解 s0·t − ½·a·t² = L 的较小正根。
		// 判别式在本分支必为正——停止距离大于 L 即等价于 s0² > 2·a·L
		const double Discriminant = FMath::Square(Speed) - 2.0 * Deceleration * TotalLength;
		return (Speed - FMath::Sqrt(FMath::Max(Discriminant, 0.0))) / Deceleration;
	}

	double ComputeTraveledDistance(double EntrySpeed, double Deceleration, double ElapsedSeconds, double TotalLength)
	{
		const double Speed = FMath::Max(EntrySpeed, 0.0);
		if (Speed <= 0.0 || TotalLength <= 0.0)
		{
			return 0.0;
		}

		// 先按总时长钳制再代入运动方程：越过顶点后抛物线会掉头，直接外推会让角色倒着走
		const double Duration = ComputeDuration(Speed, Deceleration, TotalLength);
		const double Elapsed  = FMath::Clamp(ElapsedSeconds, 0.0, Duration);

		const double Traveled = (Deceleration <= 0.0)
		                            ? Speed * Elapsed
		                            : Speed * Elapsed - 0.5 * Deceleration * FMath::Square(Elapsed);

		const double MaxDistance = FMath::Min(ComputeStopDistance(Speed, Deceleration), TotalLength);
		return FMath::Clamp(Traveled, 0.0, MaxDistance);
	}

	bool EvaluateAtDistance(TConstArrayView<FVector> Path, double Distance, FVector& OutLocation, FVector& OutTangent)
	{
		if (Path.Num() < 2)
		{
			OutLocation = Path.Num() == 1 ? Path[0] : FVector::ZeroVector;
			OutTangent  = FVector::ZeroVector;
			return false;
		}

		if (Distance <= 0.0)
		{
			OutLocation = Path[0];
			OutTangent  = (Path[1] - Path[0]).GetSafeNormal();
			return true;
		}

		double Remaining = Distance;
		for (int32 PointIndex = 1; PointIndex < Path.Num(); ++PointIndex)
		{
			const FVector Segment       = Path[PointIndex] - Path[PointIndex - 1];
			const double  SegmentLength = Segment.Size();
			if (SegmentLength <= UE_DOUBLE_KINDA_SMALL_NUMBER)
			{
				continue;
			}

			if (Remaining <= SegmentLength)
			{
				OutTangent  = Segment / SegmentLength;
				OutLocation = Path[PointIndex - 1] + OutTangent * Remaining;
				return true;
			}
			Remaining -= SegmentLength;
		}

		// 超出末端：钳制到末点，切线沿最后一段有效段
		OutLocation = Path.Last();
		OutTangent  = FVector::ZeroVector;
		for (int32 PointIndex = Path.Num() - 1; PointIndex >= 1; --PointIndex)
		{
			const FVector Segment = Path[PointIndex] - Path[PointIndex - 1];
			if (!Segment.IsNearlyZero())
			{
				OutTangent = Segment.GetSafeNormal();
				break;
			}
		}
		return true;
	}

	FVector TransformFrameOffset(const FVector& Tangent, const FVector& FrameOffset)
	{
		const FVector NormalizedTangent = Tangent.GetSafeNormal();
		if (NormalizedTangent.IsNearlyZero())
		{
			return FrameOffset;
		}

		// 依次换参考轴，直到叉积不再退化。三根世界轴两两正交，必有一根与切线不平行
		const FVector ReferenceAxes[] = {FVector::UpVector, FVector::ForwardVector, FVector::RightVector};

		FVector Outward = FVector::ZeroVector;
		for (const FVector& ReferenceAxis : ReferenceAxes)
		{
			const FVector Candidate = FVector::CrossProduct(NormalizedTangent, ReferenceAxis);
			if (Candidate.Size() > AxisDegenerateThreshold)
			{
				Outward = Candidate.GetSafeNormal();
				break;
			}
		}

		return NormalizedTangent * FrameOffset.X + Outward * FrameOffset.Y + FVector::UpVector * FrameOffset.Z;
	}
}
