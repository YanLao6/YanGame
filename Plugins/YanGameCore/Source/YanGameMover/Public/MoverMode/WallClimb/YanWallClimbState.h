#pragma once

#include "CoreMinimal.h"
#include "MoverTypes.h"

#include "YanWallClimbState.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 爬墙模式的内部状态，随 SyncStateCollection 参与 rollback/resim 与复制。
 *
 * 存放于 SyncState 而非黑板：ChaosMover 的回滚备份（FChaosMoverPredictionStateBackup）
 * 只覆盖 LastFloorResult / LastWaterResult / GroundDynamicsInfo 三项，自定义黑板 key
 * 不会随重模拟回退，跨帧累计量放黑板会导致客户端与服务器的超时判定漂移。
 *
 * 疲劳与否不入状态，由 UChaosWallClimbMode::MaxClimbDurationMs 与累计时长推导，
 * 避免出现可推导量与来源不一致的可能。
 */
USTRUCT(BlueprintType)
struct FYanWallClimbState : public FMoverDataStructBase
{
	GENERATED_USTRUCT_BODY()

	/** 本次爬墙已持续的时长（ms）。进入模式时清零，超过模式设定的上限后失去爬升能力转为下滑。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Yan|Mover|WallClimb")
	float ClimbedTimeMs = 0.f;

	/**
	 * 当前附着墙面的外法线（世界空间，零向量表示尚未附着）。
	 *
	 * 逐帧探墙以本法线的反向为探测方向：视角可以转离墙面而角色仍附着，
	 * 故不能用视角或角色朝向作为探测方向，否则转头即判定墙面丢失。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Yan|Mover|WallClimb")
	FVector WallNormal = FVector::ZeroVector;

	/**
	 * 最后一次由爬墙模式更新本状态的服务器帧号。
	 *
	 * SyncStateCollection 中的数据一经写入即随角色持续传递，切出爬墙模式后不会被清除，
	 * 而 Transition 的 Trigger 无法访问输出状态、无处重置。故以帧号连续性判断中间是否
	 * 有帧不在爬墙模式，不连续即视为新一轮爬墙并清零计时。
	 *
	 * 用帧号而非模拟时刻：整数比较无浮点精度问题，且同一帧的多个子步天然判定为连续。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Yan|Mover|WallClimb")
	int32 LastUpdateServerFrame = INDEX_NONE;

	/** 本状态是否属于 CurrentServerFrame 所在的这一轮爬墙（帧号连续） */
	bool IsCurrentFor(int32 CurrentServerFrame) const
	{
		return LastUpdateServerFrame != INDEX_NONE && (CurrentServerFrame - LastUpdateServerFrame) <= 1;
	}

	//~Begin FMoverDataStructBase Interface
	UE_API virtual FMoverDataStructBase* Clone() const override;
	UE_API virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	UE_API virtual UScriptStruct* GetScriptStruct() const override;
	UE_API virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	UE_API virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
	UE_API virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	//~End FMoverDataStructBase Interface
};

template <>
struct TStructOpsTypeTraits<FYanWallClimbState> : public TStructOpsTypeTraitsBase2<FYanWallClimbState>
{
	enum
	{
		WithNetSerializer = true
	};
};

#undef UE_API
