#pragma once

#include "CoreMinimal.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"

#include "YanFatigueState.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 疲劳通道。每个通道是一个独立的疲劳池，互不影响；新增会疲劳的动作时在此追加枚举项。
 * 以 uint8 枚举而非 FName 标识：通道随 SyncState 逐帧复制，FName 非硬编码名会按字符串上传。
 */
UENUM(BlueprintType)
enum class EYanFatigueChannel : uint8
{
	Jump,
	Slide,
	/** 空中冲刺冷却：有条目即禁止空中冲刺，落地后才开始恢复 */
	AirDash,
	/** 二段跳冷却：有条目即禁止二段跳，落地后才开始恢复 */
	DoubleJump,
};

/** 单个通道的疲劳记录。只存在于「尚未完全恢复」的通道，恢复到 0 即从状态中移除 */
USTRUCT(BlueprintType)
struct FYanFatigueEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Yan|Mover|Fatigue")
	EYanFatigueChannel Channel = EYanFatigueChannel::Jump;

	/** 疲劳度，0 为精力充沛、1 为完全疲劳。网络传输按 uint8 量化 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Yan|Mover|Fatigue")
	float Fatigue = 0.f;

	/** 每秒恢复的疲劳度。由最近一次消耗该通道的效果写入，仿真据此逐步衰减 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Yan|Mover|Fatigue")
	float RecoveryPerSecond = 0.f;

	/** 为 true 时暂停恢复，直到角色处于地面模式；一旦开始恢复即清除，此后离地不再暂停 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Yan|Mover|Fatigue")
	bool bRecoverAfterLanding = false;
};

/**
 * 角色各动作的疲劳度，随 SyncStateCollection 参与 rollback/resim 与复制。
 *
 * 疲劳度决定起跳速度、滑铲前冲等未来帧的仿真结果，故必须随 SyncState 回滚：
 * 若存于 Transition 的成员或黑板，客户端重模拟时会把同一次消耗再扣一遍，与服务器永久分歧。
 *
 * 数据流：
 *  - 消耗：Transition 的 Trigger 拿不到输出状态，由 FYanFatigueVelocityEffect 在
 *    ApplyMovementEffect_Async 中调用 Accumulate 写入，同时带入该通道的恢复速率。
 *  - 恢复：UYanChaosMoverSimulation::OnPostSimulationTick 每个物理步调用 Recover，
 *    恢复到 0 的通道被移除；所有通道都恢复后整个结构体从集合中移除，不再占用带宽。
 *    标记了 bRecoverAfterLanding 的通道在进入地面模式前不恢复，用于「落地才开始计时」的冷却。
 *  - 读取：Transition 在 Trigger 里用 ScaleByFatigue 把配置值按疲劳度插值；
 *    冷却类通道在 Evaluate 里以 GetFatigue > 0 判定仍在冷却。
 */
USTRUCT(BlueprintType)
struct FYanFatigueState : public FMoverDataStructBase
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Yan|Mover|Fatigue")
	TArray<FYanFatigueEntry> Entries;

	/** 该通道的疲劳记录；状态或通道不存在（已完全恢复）时返回 nullptr */
	static UE_API const FYanFatigueEntry* FindEntry(const FMoverSyncState& SyncState, EYanFatigueChannel Channel);

	/** 该通道的疲劳度（0~1）；状态或通道不存在视为 0 */
	static UE_API float GetFatigue(const FMoverSyncState& SyncState, EYanFatigueChannel Channel);

	/** 按疲劳度在精力充沛值与完全疲劳值之间线性插值，供各 Transition 换算速度、冲量等 */
	static UE_API float ScaleByFatigue(const FMoverSyncState& SyncState, EYanFatigueChannel Channel, float RestedValue, float ExhaustedValue);

	/** 叠加一次消耗并记下该通道的恢复速率与恢复时机，疲劳度钳在 1 */
	UE_API void Accumulate(EYanFatigueChannel Channel, float Cost, float RecoveryPerSecond, bool bRecoverAfterLanding = false);

	/** 按各通道速率恢复一步，恢复到 0 的通道移除；bOnGround 为本步结束时是否处于地面模式 */
	UE_API void Recover(float DeltaSeconds, bool bOnGround);

	bool IsEmpty() const { return Entries.IsEmpty(); }

	//~Begin FMoverDataStructBase Interface
	UE_API virtual FMoverDataStructBase* Clone() const override;
	UE_API virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	UE_API virtual UScriptStruct* GetScriptStruct() const override;
	UE_API virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	UE_API virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
	UE_API virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	//~End FMoverDataStructBase Interface

private:
	const FYanFatigueEntry* FindEntry(EYanFatigueChannel Channel) const;
	FYanFatigueEntry*       FindEntry(EYanFatigueChannel Channel);
};

template <>
struct TStructOpsTypeTraits<FYanFatigueState> : public TStructOpsTypeTraitsBase2<FYanFatigueState>
{
	enum
	{
		WithNetSerializer = true
	};
};

#undef UE_API
