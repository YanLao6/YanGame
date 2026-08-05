#pragma once

#include "CoreMinimal.h"
#include "LayeredMove.h"
#include "LayeredMoveBase.h"

#include "YanNoLandingMove.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 短暂禁止落地：让角色在一段时间内滞留于 Falling，不被地面摩擦接管。
 *
 * 击飞若在 Walking 下结算，水平速度当帧就被地面摩擦吃掉，冲量形同虚设。
 * 与其新做一个「不落地的 Falling」运动模式，不如利用引擎既有的契约——
 * UChaosCharacterLandingCheck 在 Mover.DisableLanding 存在时直接跳过落地判定，
 * 而 UChaosMoverSimulation::HasGameplayTag 会遍历 SyncState 中的 active layered move。
 * 因此本结构只需携带该标签存活一段时间，无须产生任何移动。
 *
 * 职责按 tick 分两段：
 *  - 首帧：以 PreferredMode 把角色从地面模式踢进 Falling。引擎只在 move 的首个 tick
 *    采纳 PreferredMode，故「进入」只能在这一帧完成。
 *  - 此后：不参与移动混合（GenerateMove_Async 返回 false），仅以标签维持滞空。
 *    DurationMs 到期后本 move 自动移出 SyncState，标签随之消失，落地判定恢复。
 *
 * 计时与存续均由 SyncState 承载，重模拟与回滚天然一致，不依赖黑板。
 */
USTRUCT(BlueprintType)
struct FYanLayeredMove_NoLanding : public FLayeredMoveBase
{
	GENERATED_BODY()

	UE_API FYanLayeredMove_NoLanding();

	virtual ~FYanLayeredMove_NoLanding() = default;

	/** 首帧强制切入的运动模式；留空则维持当前模式，仅禁止落地 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FName ForceMovementMode = FName("Falling");

	//~Begin FLayeredMoveBase Interface
	virtual bool SupportsAsync() const override { return true; }

	UE_API virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const override;
	UE_API virtual void GetGameplayTags(FGameplayTagContainer& InOutTags) const override;

	UE_API virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	UE_API virtual bool GenerateMove_Async(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;

	UE_API virtual FLayeredMoveBase* Clone() const override;
	UE_API virtual void NetSerialize(FArchive& Ar) override;
	UE_API virtual UScriptStruct* GetScriptStruct() const override;
	UE_API virtual FString ToSimpleString() const override;
	UE_API virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
	//~End FLayeredMoveBase Interface
};

template <>
struct TStructOpsTypeTraits<FYanLayeredMove_NoLanding> : public TStructOpsTypeTraitsBase2<FYanLayeredMove_NoLanding>
{
	enum
	{
		WithCopy = true
	};
};

#undef UE_API
