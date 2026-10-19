#include "YanMoverDataModelTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanMoverDataModelTypes)

FMoverDataStructBase* FYanCharacterInputs::Clone() const
{
	return new FYanCharacterInputs(*this);
}

bool FYanCharacterInputs::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	// 每个 bool 只占 1 bit，极致节省带宽
	Ar.SerializeBits(&bWantsToSlide, 1);
	Ar.SerializeBits(&bIsSprintJustPressed, 1);
	Ar.SerializeBits(&bIsSprintPressed, 1);
	Ar.SerializeBits(&bIsGrapplingActive, 1);
	Ar.SerializeBits(&bWantsToDetachFromWall, 1);
	Ar.SerializeBits(&bWantsToAirDash, 1);
	Ar.SerializeBits(&bWantsToBlink, 1);
	Ar.SerializeBits(&bWantsToClinch, 1);
	Ar.SerializeBits(&bWantsToBreakFree, 1);
	Ar.SerializeBits(&bGrapplePullsTarget, 1);
	Ar.SerializeBits(&bWantsToWallGrind, 1);
	// 锚点仅在钩锁激活时有意义，始终随输入上传以支持服务器 re-simulation
	Ar << GrappleAnchorPoint;
	Ar << GrappleHookLocation;
	Ar << GrappleRopeLength;
	// 目标位置只在钩到人时有效，仅此时上传以省去其余帧的带宽
	if (bGrapplePullsTarget)
	{
		Ar << GrappleTargetLocation;
	}
	// 瞬移终点只在脉冲帧有效，仅此时上传以省去其余帧的带宽
	if (bWantsToBlink)
	{
		Ar << BlinkTargetPoint;
	}
	// 贴身落位点与下落基准同理，只在脉冲帧有效
	if (bWantsToClinch)
	{
		Ar << ClinchAnchorPoint;
		Ar << ClinchFallStartSimTimeMs;
	}
	if (bWantsToWallGrind)
	{
		// 计数先行，发起方停止填充折线后本项退化为一个字节。
		// 折线点不作量化：本结构参与 ShouldReconcile 比对，有损往返会让两端恒判不等
		uint8 NumPathPoints = Ar.IsLoading() ? 0 : static_cast<uint8>(FMath::Min(WallGrindPath.Num(), YanMoverInputs::MaxWallGrindPathPoints));
		Ar << NumPathPoints;

		if (Ar.IsLoading())
		{
			WallGrindPath.SetNum(NumPathPoints);
		}
		for (uint8 PointIndex = 0; PointIndex < NumPathPoints; ++PointIndex)
		{
			Ar << WallGrindPath[PointIndex];
		}
		if (Ar.IsLoading() && WallGrindPath.Num() > YanMoverInputs::MaxWallGrindPathPoints)
		{
			// 超额的点先读完再丢弃：中途截断会让归档流错位，后续字段一并读坏
			WallGrindPath.SetNum(YanMoverInputs::MaxWallGrindPathPoints);
		}

		Ar << WallGrindEntrySpeed;
		Ar << WallGrindStartSimTimeMs;
	}
	// 世界触发器的载荷只在脉冲帧有效，仅此时上传
	Ar.SerializeBits(&bWantsWorldLaunch, 1);
	if (bWantsWorldLaunch)
	{
		Ar << WorldLaunchRequestId;
		Ar << WorldLaunchVelocity;
	}
	Ar.SerializeBits(&bWantsWorldTeleport, 1);
	if (bWantsWorldTeleport)
	{
		Ar << WorldTeleportRequestId;
		Ar << WorldTeleportLocation;
		Ar << WorldTeleportRotation;
		Ar.SerializeBits(&bWorldTeleportOverridesVelocity, 1);
		if (bWorldTeleportOverridesVelocity)
		{
			Ar << WorldTeleportExitVelocity;
		}
	}
	bOutSuccess = true;
	return true;
}

UScriptStruct* FYanCharacterInputs::GetScriptStruct() const
{
	return StaticStruct();
}

namespace
{
	// 逐点比对折线。与其余几何量一致取容差比较，不用 TArray 的精确相等
	bool ArePathsEqual(const TArray<FVector>& Lhs, const TArray<FVector>& Rhs)
	{
		if (Lhs.Num() != Rhs.Num())
		{
			return false;
		}
		for (int32 PointIndex = 0; PointIndex < Lhs.Num(); ++PointIndex)
		{
			if (!Lhs[PointIndex].Equals(Rhs[PointIndex]))
			{
				return false;
			}
		}
		return true;
	}
}

bool FYanCharacterInputs::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FYanCharacterInputs& Auth = static_cast<const FYanCharacterInputs&>(AuthorityState);
	return Auth.bWantsToSlide != bWantsToSlide
	       || Auth.bIsSprintJustPressed != bIsSprintJustPressed
	       || Auth.bIsSprintPressed != bIsSprintPressed
	       || Auth.bIsGrapplingActive != bIsGrapplingActive
	       || Auth.bWantsToDetachFromWall != bWantsToDetachFromWall
	       || Auth.bWantsToAirDash != bWantsToAirDash
	       || Auth.bWantsToBlink != bWantsToBlink
	       || Auth.bWantsToClinch != bWantsToClinch
	       || Auth.bWantsToBreakFree != bWantsToBreakFree
	       || Auth.bGrapplePullsTarget != bGrapplePullsTarget
	       || !Auth.GrappleAnchorPoint.Equals(GrappleAnchorPoint)
	       || !Auth.GrappleHookLocation.Equals(GrappleHookLocation)
	       || !FMath::IsNearlyEqual(Auth.GrappleRopeLength, GrappleRopeLength)
	       // 终点只在脉冲帧上传，其余帧两端各自残留的旧值不具可比性
	       || (bGrapplePullsTarget && !Auth.GrappleTargetLocation.Equals(GrappleTargetLocation))
	       || (bWantsToBlink && !Auth.BlinkTargetPoint.Equals(BlinkTargetPoint))
	       || (bWantsToClinch && !Auth.ClinchAnchorPoint.Equals(ClinchAnchorPoint))
	       || (bWantsToClinch && !FMath::IsNearlyEqual(Auth.ClinchFallStartSimTimeMs, ClinchFallStartSimTimeMs))
	       || Auth.bWantsToWallGrind != bWantsToWallGrind
	       || (bWantsToWallGrind && !ArePathsEqual(Auth.WallGrindPath, WallGrindPath))
	       || (bWantsToWallGrind && !FMath::IsNearlyEqual(Auth.WallGrindEntrySpeed, WallGrindEntrySpeed))
	       || (bWantsToWallGrind && !FMath::IsNearlyEqual(Auth.WallGrindStartSimTimeMs, WallGrindStartSimTimeMs))
	       || Auth.bWantsWorldLaunch != bWantsWorldLaunch
	       || (bWantsWorldLaunch && Auth.WorldLaunchRequestId != WorldLaunchRequestId)
	       || (bWantsWorldLaunch && !Auth.WorldLaunchVelocity.Equals(WorldLaunchVelocity))
	       || Auth.bWantsWorldTeleport != bWantsWorldTeleport
	       || (bWantsWorldTeleport && Auth.WorldTeleportRequestId != WorldTeleportRequestId)
	       || (bWantsWorldTeleport && !Auth.WorldTeleportLocation.Equals(WorldTeleportLocation))
	       || (bWantsWorldTeleport && !Auth.WorldTeleportRotation.Equals(WorldTeleportRotation))
	       || (bWantsWorldTeleport && Auth.bWorldTeleportOverridesVelocity != bWorldTeleportOverridesVelocity)
	       || (bWantsWorldTeleport && bWorldTeleportOverridesVelocity && !Auth.WorldTeleportExitVelocity.Equals(WorldTeleportExitVelocity));
}

void FYanCharacterInputs::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	// bool 无法线性插值，直接按 Pct 取 From 或 To 的离散值
	const FYanCharacterInputs& Src = static_cast<const FYanCharacterInputs&>((Pct < 0.5f) ? From : To);
	bWantsToSlide                  = Src.bWantsToSlide;
	bIsSprintJustPressed           = Src.bIsSprintJustPressed;
	bIsSprintPressed               = Src.bIsSprintPressed;
	bIsGrapplingActive             = Src.bIsGrapplingActive;
	bWantsToDetachFromWall         = Src.bWantsToDetachFromWall;
	bWantsToAirDash                = Src.bWantsToAirDash;
	bWantsToBlink                  = Src.bWantsToBlink;
	bWantsToClinch                 = Src.bWantsToClinch;
	bWantsToBreakFree              = Src.bWantsToBreakFree;
	bGrapplePullsTarget            = Src.bGrapplePullsTarget;
	GrappleAnchorPoint             = Src.GrappleAnchorPoint;
	GrappleHookLocation            = Src.GrappleHookLocation;
	GrappleRopeLength              = Src.GrappleRopeLength;
	GrappleTargetLocation          = Src.GrappleTargetLocation;
	BlinkTargetPoint               = Src.BlinkTargetPoint;
	ClinchAnchorPoint              = Src.ClinchAnchorPoint;
	ClinchFallStartSimTimeMs       = Src.ClinchFallStartSimTimeMs;
	bWantsToWallGrind              = Src.bWantsToWallGrind;
	WallGrindPath                  = Src.WallGrindPath;
	WallGrindEntrySpeed            = Src.WallGrindEntrySpeed;
	WallGrindStartSimTimeMs        = Src.WallGrindStartSimTimeMs;
	bWantsWorldLaunch              = Src.bWantsWorldLaunch;
	WorldLaunchRequestId           = Src.WorldLaunchRequestId;
	WorldLaunchVelocity            = Src.WorldLaunchVelocity;
	bWantsWorldTeleport            = Src.bWantsWorldTeleport;
	WorldTeleportRequestId         = Src.WorldTeleportRequestId;
	WorldTeleportLocation          = Src.WorldTeleportLocation;
	WorldTeleportRotation          = Src.WorldTeleportRotation;
	bWorldTeleportOverridesVelocity = Src.bWorldTeleportOverridesVelocity;
	WorldTeleportExitVelocity      = Src.WorldTeleportExitVelocity;
}

void FYanCharacterInputs::Merge(const FMoverDataStructBase& From)
{
	// 多个 InputProducer 同帧写入时取并集（任意一方按下即视为按下）
	const FYanCharacterInputs& Src = static_cast<const FYanCharacterInputs&>(From);
	bWantsToSlide                  |= Src.bWantsToSlide;
	bIsSprintJustPressed           |= Src.bIsSprintJustPressed;
	bIsSprintPressed               |= Src.bIsSprintPressed;
	bIsGrapplingActive             |= Src.bIsGrapplingActive;
	bWantsToDetachFromWall         |= Src.bWantsToDetachFromWall;
	bWantsToAirDash                |= Src.bWantsToAirDash;
	bWantsToBlink                  |= Src.bWantsToBlink;
	bWantsToClinch                 |= Src.bWantsToClinch;
	bWantsToBreakFree              |= Src.bWantsToBreakFree;
	bGrapplePullsTarget            |= Src.bGrapplePullsTarget;
	bWantsToWallGrind              |= Src.bWantsToWallGrind;
	// 汇合点只由钩锁 Ability 写入。发起者不该被牵引（对方悬空而自己站在地上）时
	// bIsGrapplingActive 为 false，但被钩者仍在被拉，故两个标志有其一即取值
	if (Src.bIsGrapplingActive || Src.bGrapplePullsTarget)
	{
		GrappleAnchorPoint = Src.GrappleAnchorPoint;
		GrappleHookLocation = Src.GrappleHookLocation;
		GrappleRopeLength  = Src.GrappleRopeLength;
	}
	if (Src.bGrapplePullsTarget)
	{
		GrappleTargetLocation = Src.GrappleTargetLocation;
	}
	// 终点同理，只取真正发起瞬移的那一方写入的值
	if (Src.bWantsToBlink)
	{
		BlinkTargetPoint = Src.BlinkTargetPoint;
	}
	if (Src.bWantsToClinch)
	{
		ClinchAnchorPoint        = Src.ClinchAnchorPoint;
		ClinchFallStartSimTimeMs = Src.ClinchFallStartSimTimeMs;
	}
	if (Src.bWantsToWallGrind)
	{
		WallGrindPath           = Src.WallGrindPath;
		WallGrindEntrySpeed     = Src.WallGrindEntrySpeed;
		WallGrindStartSimTimeMs = Src.WallGrindStartSimTimeMs;
	}
	// 世界触发器的载荷只由触发器输入组件写入，取脉冲那一方的值
	bWantsWorldLaunch   |= Src.bWantsWorldLaunch;
	bWantsWorldTeleport |= Src.bWantsWorldTeleport;
	if (Src.bWantsWorldLaunch)
	{
		WorldLaunchRequestId = Src.WorldLaunchRequestId;
		WorldLaunchVelocity  = Src.WorldLaunchVelocity;
	}
	if (Src.bWantsWorldTeleport)
	{
		WorldTeleportRequestId          = Src.WorldTeleportRequestId;
		WorldTeleportLocation           = Src.WorldTeleportLocation;
		WorldTeleportRotation           = Src.WorldTeleportRotation;
		bWorldTeleportOverridesVelocity = Src.bWorldTeleportOverridesVelocity;
		WorldTeleportExitVelocity       = Src.WorldTeleportExitVelocity;
	}
}

void FYanCharacterInputs::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);
	Out.Appendf("GrappleHook: (%.1f,%.1f,%.1f) | ", GrappleHookLocation.X, GrappleHookLocation.Y, GrappleHookLocation.Z);
	Out.Appendf("bWantsToSlide: %i | bIsSprintJustPressed: %i | bIsSprintPressed: %i | bIsGrapplingActive: %i | bWantsToDetachFromWall: %i | bWantsToAirDash: %i | bWantsToBlink: %i | bWantsToClinch: %i | bWantsToBreakFree: %i | bGrapplePullsTarget: %i | GrappleAnchor: (%.1f,%.1f,%.1f) | GrappleRope: %.1f | GrappleTarget: (%.1f,%.1f,%.1f) | BlinkTarget: (%.1f,%.1f,%.1f) | ClinchAnchor: (%.1f,%.1f,%.1f) | ClinchFallStart: %.1f | bWantsToWallGrind: %i | WallGrindPathNum: %i | WallGrindEntrySpeed: %.1f | WallGrindStart: %.1f\n",
	            bWantsToSlide, bIsSprintJustPressed, bIsSprintPressed, bIsGrapplingActive, bWantsToDetachFromWall, bWantsToAirDash, bWantsToBlink, bWantsToClinch, bWantsToBreakFree, bGrapplePullsTarget,
	            GrappleAnchorPoint.X, GrappleAnchorPoint.Y, GrappleAnchorPoint.Z, GrappleRopeLength,
	            GrappleTargetLocation.X, GrappleTargetLocation.Y, GrappleTargetLocation.Z,
	            BlinkTargetPoint.X, BlinkTargetPoint.Y, BlinkTargetPoint.Z,
	            ClinchAnchorPoint.X, ClinchAnchorPoint.Y, ClinchAnchorPoint.Z, ClinchFallStartSimTimeMs,
	            bWantsToWallGrind, WallGrindPath.Num(), WallGrindEntrySpeed, WallGrindStartSimTimeMs);
	Out.Appendf("bWantsWorldLaunch: %i | WorldLaunchRequestId: %u | WorldLaunchVelocity: (%.1f,%.1f,%.1f) | bWantsWorldTeleport: %i | WorldTeleportRequestId: %u | WorldTeleportLocation: (%.1f,%.1f,%.1f) | bWorldTeleportOverridesVelocity: %i\n",
	            bWantsWorldLaunch, WorldLaunchRequestId, WorldLaunchVelocity.X, WorldLaunchVelocity.Y, WorldLaunchVelocity.Z,
	            bWantsWorldTeleport, WorldTeleportRequestId, WorldTeleportLocation.X, WorldTeleportLocation.Y, WorldTeleportLocation.Z, bWorldTeleportOverridesVelocity);
}

void FYanCharacterInputs::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}
