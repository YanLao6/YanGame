#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"

class UWorld;

/**
 * 爬墙的墙面查询与几何判定。
 *
 * 判定口径集中于此，进入判定、爬墙模式与爬墙跳三方共用同一套谓词，
 * 避免各处阈值实现不一致而出现「能进不能维持」之类的空档。
 * 查询走物理线程接口，与 ChaosMover 的地面查询同源，可在 sim 内确定性求值。
 */
namespace UE::YanMover::WallClimb
{
	struct FWallSweepParams
	{
		FCollisionResponseParams ResponseParams;
		FCollisionQueryParams    QueryParams;
		/** 角色胶囊中心（世界空间） */
		FVector Location = FVector::ZeroVector;
		/** 水平探测方向，需已归一化 */
		FVector ProbeDir = FVector::ZeroVector;
		FVector UpDir    = FVector::UpVector;
		const UWorld* World = nullptr;
		/** 自胶囊表面向外的探测距离 */
		float ProbeDistance = 60.f;
		float ProbeRadius   = 20.f;
		/** 角色胶囊半径，用于把探测起点推到胶囊表面 */
		float PawnCollisionRadius            = 40.f;
		ECollisionChannel CollisionChannel = ECC_Pawn;
	};

	struct FWallCheckResult
	{
		bool       bBlockingHit = false;
		/** 命中面外法线（世界空间） */
		FVector    Normal = FVector::ZeroVector;
		/** 自胶囊表面到命中点的距离 */
		float      Distance = 0.f;
		FHitResult HitResult;
	};

	/** 沿 ProbeDir 做球体扫描寻找墙面，命中时填充 OutResult 并返回 true */
	YANGAMEMOVER_API bool WallSweep_Internal(const FWallSweepParams& Params, FWallCheckResult& OutResult);

	/** 墙面倾角（法线与 UpDir 夹角）是否落在 [MinDegrees, MaxDegrees] 内 */
	YANGAMEMOVER_API bool IsClimbableAngle(const FVector& WallNormal, const FVector& UpDir, float MinDegrees, float MaxDegrees);

	/**
	 * 方向的水平分量是否朝向墙面，即与墙内法线（-WallNormal 的水平分量）夹角小于 MaxDegrees。
	 *
	 * 一律取水平投影比较：否则玩家抬头俯视时会因 Pitch 分量掉出容差而误判为背离墙面。
	 * Direction 为零向量（无输入）时返回 false。
	 */
	YANGAMEMOVER_API bool IsFacingWall(const FVector& Direction, const FVector& WallNormal, const FVector& UpDir, float MaxDegrees);

	/** 取向量在墙面切平面上的投影 */
	YANGAMEMOVER_API FVector ProjectOntoWallPlane(const FVector& Vector, const FVector& WallNormal);

	namespace Blackboard
	{
		/**
		 * 帧内传递：GenerateMove 探得的墙面法线，供同帧 SimulationTick 落入 SyncState。
		 *
		 * 仅限帧内使用——黑板不参与 ChaosMover 的回滚备份，但重模拟会重新执行 GenerateMove
		 * 并覆盖本值，故同帧读写是安全的；跨帧存活的状态一律走 FYanWallClimbState。
		 */
		extern YANGAMEMOVER_API const FName WallClimbNormal;
	}
}
