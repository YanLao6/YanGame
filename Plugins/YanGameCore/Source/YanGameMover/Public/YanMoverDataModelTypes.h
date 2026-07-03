#pragma once

#include "CoreMinimal.h"
#include "MoverTypes.h"
#include "YanMoverDataModelTypes.generated.h"

/**
 * 项目自定义 Mover 输入包，是 Ability 与 Mover 之间唯一的通信接口。
 *
 * 继承 FMoverDataStructBase，随每帧输入命令包（FMoverInputCmdContext）一起
 * 打包发往服务器，服务器重模拟时使用完全相同的数据求值 Transition，
 * 保证客户端与服务器在相同模拟步切换模式，消除预测回滚抖动。
 *
 * 接口契约（必须共同维护，否则破坏网络确定性）：
 *  - 写入方：仅本地受控端的 InputProducer（各 UMoverInputAbility 子类）在
 *    ProduceMoverInput 中写入。ProduceInput 只在 IsLocallyControlled 端调用，
 *    服务器对远程 Pawn 用复制来的本结构重放。
 *  - 读取方：sim 内的 MovementMode（GenerateMove）与 Transition（Evaluate）。
 *    禁止在读取方旁路读取摄像机、game-thread 状态或 Actor 引用——只能读本结构。
 *  - 决策来源：模式切换一律由读取方依据本结构的持续标志裁决，
 *    不使用一次性 FCharacterDefaultInputs::SuggestedMovementMode。
 *
 * 新增移动意图时在此结构体加字段，并同步维护 NetSerialize/ShouldReconcile/
 * Merge/Interpolate；无需修改 FCharacterDefaultInputs。
 */
USTRUCT(BlueprintType)
struct YANGAMEMOVER_API FYanCharacterInputs : public FMoverDataStructBase
{
	GENERATED_USTRUCT_BODY()

	/** 玩家持续按住滑铲键 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	bool bWantsToSlide = false;

	/** 冲刺键刚按下（单帧脉冲，不持续） */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	bool bIsSprintJustPressed = false;

	/** 冲刺键持续按住 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	bool bIsSprintPressed = false;

	/**
	 * 钩锁激活的持续标志（命中后置 true，松手/超时/锚点入后半球后置 false）。
	 * 写入：MoverGrapplingAbility；读取：UChaosGrapplingEnterCheck（进入）与
	 * UChaosGrapplingExitCheck（退出）。进入与退出均依据本标志，形成对称闭环。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	bool bIsGrapplingActive = false;

	/**
	 * 钩锁锚点世界坐标（OnHookHit 快照）。钩锁期间每帧随输入上传，
	 * 服务器只信任本字段，不自行重算锚点。读取：UChaosGrapplingMode::GenerateMove。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	FVector GrappleAnchorPoint = FVector::ZeroVector;

	/** 预留：激活时角色到锚点的初始距离（cm，绳长约束用）。当前 GenerateMove 采用拉力模型，暂未读取。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	float GrappleRopeLength = 0.f;

	//~Begin FMoverDataStructBase Interface
	virtual FMoverDataStructBase* Clone() const override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
	virtual void Merge(const FMoverDataStructBase& From) override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	//~End FMoverDataStructBase Interface
};

template <>
struct TStructOpsTypeTraits<FYanCharacterInputs> : public TStructOpsTypeTraitsBase2<FYanCharacterInputs>
{
	enum
	{
		WithNetSerializer = true
	};
};
