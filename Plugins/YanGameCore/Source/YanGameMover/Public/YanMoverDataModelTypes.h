#pragma once

#include "CoreMinimal.h"
#include "MoverTypes.h"
#include "YanMoverDataModelTypes.generated.h"

#define UE_API YANGAMEMOVER_API

namespace YanMoverInputs
{
	/** 贴墙折线的点数上限，兼作反制恶意客户端上传超长数组的闸门 */
	inline constexpr int32 MaxWallGrindPathPoints = 16;
}

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
 *    禁止在读取方旁路读取摄像机、game-thread 状态或 Actor 引用；方向与位置可读仿真同步状态。
 *  - 决策来源：模式切换由读取方依据输入意图与仿真状态裁决，
 *    不使用一次性 FCharacterDefaultInputs::SuggestedMovementMode。
 *
 * 新增移动意图时在此结构体加字段，并同步维护 NetSerialize/ShouldReconcile/
 * Merge/Interpolate；无需修改 FCharacterDefaultInputs。
 */
USTRUCT(BlueprintType)
struct FYanCharacterInputs : public FMoverDataStructBase
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
	 * 「本帧发起者应被钩锁牵引」的持续标志。写入：MoverGrapplingAbility；
	 * 读取：UChaosGrapplingEnterCheck（进入）与 UChaosGrapplingExitCheck（退出）。
	 * 进入与退出均依据本标志及视角水平前向的有效方向范围，形成对称闭环。
	 *
	 * 钩住墙壁时等价于「钩锁激活」；钩住 Pawn 时还取决于双方的离地状态——
	 * 发起者站在地上而对方悬空时本标志为 false，发起者因而留在原有移动模式，
	 * 只有对方被拉过来。离地状态中途变化时本标志随之翻转，模式跟着切换。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	bool bIsGrapplingActive = false;

	/**
	 * 钩锁牵引的汇合点（世界坐标）。钩住墙壁时即命中点；钩住 Pawn 时是按双方离地状态
	 * 算出的会合位置——双方都该动取实时中点，只有一方该动则取不动那一方的位置。
	 * 双方朝同一点收敛，故发起者与被钩者共用本字段。
	 *
	 * 钩锁期间每帧随输入上传，服务器只信任本字段，不自行重算。
	 * 读取：UChaosGrapplingMode::GenerateMove 与 UYanGrappleAuthorityComponent。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	FVector GrappleAnchorPoint = FVector::ZeroVector;

	/** 钩子位置快照（世界坐标），用于方向退出判定；钩住 Pawn 时跟随其位置，与牵引汇合点独立。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	FVector GrappleHookLocation = FVector::ZeroVector;

	/**
	 * 「钩锁挂在某个 Pawn 上，且本帧该 Pawn 应被牵引」的持续标志。
	 * 写入：MoverGrapplingAbility；读取：服务器 game thread 的 UYanGrappleAuthorityComponent。
	 *
	 * 与 bWantsToClinch 同为持续标志而非脉冲：跨 Pawn 施力只能在 game thread 下发，
	 * 而 game tick 与仿真 tick 并非一一对应，脉冲会被采样漏掉。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	bool bGrapplePullsTarget = false;

	/**
	 * 被钩 Pawn 的位置快照，仅供服务器就近认定「钩到了谁」。
	 *
	 * 只作认定用的坐标，不作牵引终点——终点是 GrappleAnchorPoint。
	 * 目标位置在发起端是插值外推的结果，与服务器上的真值本就不同，故认定须留出容差。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	FVector GrappleTargetLocation = FVector::ZeroVector;

	/** 预留：激活时角色到锚点的初始距离（cm，绳长约束用）。当前 GenerateMove 采用拉力模型，暂未读取。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	float GrappleRopeLength = 0.f;

	/**
	 * 主动脱墙意图（下蹲键按住）。写入：MoverSlideAbility（与滑铲、蹲伏共用同一按键，
	 * 各意图由 sim 侧各自的 Transition 裁决）；读取：UChaosWallClimbExitCheck。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	bool bWantsToDetachFromWall = false;

	/**
	 * 空中冲刺意图（单帧脉冲）。写入：UAS_CharacterDash；读取：UChaosCharacterAirDashCheck。
	 * 冲刺方向与速度均不随本结构上传：方向由 FCharacterDefaultInputs 推导，
	 * 速度取自 Transition 的配置值，两端各自求得同一结果。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	bool bWantsToAirDash = false;

	/**
	 * 瞬移意图（单帧脉冲）。写入：UAS_CharacterBlink；读取：UChaosCharacterBlinkCheck。
	 * 与冲刺不同，方向不由 sim 侧推导——瞬移终点须经碰撞收敛，两端各自扫描不保证得到同一结果，
	 * 故终点随 BlinkTargetPoint 一并上传。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	bool bWantsToBlink = false;

	/**
	 * 瞬移终点世界坐标（发起帧扫描所得）。服务器只信任本字段，不自行重算终点，
	 * 与 GrappleAnchorPoint 同一契约。仅在 bWantsToBlink 为真的那一帧有意义。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	FVector BlinkTargetPoint = FVector::ZeroVector;

	/**
	 * 贴身抓取意图（持续标志，抓取期间一直为真）。写入：抓取 Ability；
	 * 读取：UChaosCharacterClinchCheck 与服务器侧的 UYanClinchAuthorityComponent。
	 *
	 * 之所以是持续标志而非脉冲：服务器须在 game thread 读到本意图才能对被抓者下发效果
	 * （物理线程访问其他 Actor 不安全），而 game tick 与仿真 tick 并非一一对应，
	 * 单帧脉冲会被采样漏掉。重复入队则由 sim 侧按 Mover.IsClinched 标签判重。
	 *
	 * 仅发起方（A）写入本组字段，被抓者（B）的同名效果由服务器经 sim action 下发。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	bool bWantsToClinch = false;

	/**
	 * 贴身落位点世界坐标（命中帧算定）。落位点依赖被抓者的位置，而该位置在发起端是
	 * 插值外推的结果，与服务器上的真值不同——两端各自重算必然分歧，故与 BlinkTargetPoint
	 * 同一契约：只信任发起端上传的坐标。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	FVector ClinchAnchorPoint = FVector::ZeroVector;

	/**
	 * 贴身双方开始下落的绝对仿真时间（毫秒）。抓取双方的垂直运动必须严格同步，
	 * 否则高度差按 g·Δt·t 随时间线性拉大；而双方的效果生效时刻本就相差一个往返
	 * （一方本地入队、另一方经 sim action 下发），故不能各自计时，须共用本时间基准。
	 * 由发起方按 UMoverComponent::GetLastTimeStep 取定——该时间处于服务器仿真时间轴，
	 * 客户端预测时亦在同一轴上，两端因而可比。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	double ClinchFallStartSimTimeMs = 0.0;

	/**
	 * 挣脱贴身的意图（单帧脉冲）。写入：挣脱 Ability；读取：FYanLayeredMove_Clinch。
	 * 被抓者的钉制效果虽由服务器下发，挣脱却走本地输入包——按键当帧即可解除，
	 * 无需等待一个往返；服务器重模拟时读到同一输入位，在同一模拟步得到相同结果。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	bool bWantsToBreakFree = false;

	/**
	 * 贴墙摩擦推进意图（持续标志，推进期间一直为真）。写入：贴墙推进 Ability；
	 * 读取：UChaosCharacterWallGrindCheck 与服务器侧的裁决组件。
	 *
	 * 与 bWantsToClinch 同为持续标志：服务器须在 game thread 读到本意图才能对被推方下发效果，
	 * 而 game tick 与仿真 tick 并非一一对应，单帧脉冲会被采样漏掉。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	bool bWantsToWallGrind = false;

	/**
	 * 贴墙推进的路径折线（世界坐标，点数不超过 YanMoverInputs::MaxWallGrindPathPoints）。
	 *
	 * 之所以整条路径一次算定并上传，而非每帧各自沿墙重扫：逐帧扫描结果两端必然分歧，
	 * 且被推方的效果由服务器下发、拿不到发起方的输入包，无从复现一条只有发起方在扫的曲线。
	 * 折线一经上传，推进双方便共用同一条曲线，各自按弧长求值，相对位置因而守恒。
	 *
	 * 服务器须在意图的上升沿采样到本数组才能构造被推方的效果，故不能只发一帧；
	 * 发起方在确信服务器已采样后清空本数组即可，序列化随之退化为一个字节的计数。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	TArray<FVector> WallGrindPath;

	/** 贴墙推进的沿墙初速（cm/s），由发起帧的速度在墙面上的投影取定 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	float WallGrindEntrySpeed = 0.f;

	/**
	 * 推进双方一同起滑的绝对仿真时间（毫秒）。与 ClinchFallStartSimTimeMs 同一契约：
	 * 双方的效果生效时刻本就相差一个往返（一方本地入队、另一方经 sim action 下发），
	 * 各自计时会让两人沿折线的进度错开且不再收敛，故须共用本时间基准，先就位的一方等齐。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	double WallGrindStartSimTimeMs = 0.0;

	/**
	 * 踩中世界发射板的意图（单帧脉冲）。写入：UYanWorldTriggerInputComponent；
	 * 读取：UChaosCharacterLaunchPadCheck。
	 *
	 * 发射板是世界 Actor，其重叠回调在 game thread；若由回调直接向 Mover 入队效果，
	 * 该效果不可回滚，客户端预测出的弹射会被服务器状态抹掉。故与冲刺同路：
	 * 本地受控端把意图写进输入包，两端各自在 sim 内以可回滚方式施加。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	bool bWantsWorldLaunch = false;

	/**
	 * 发射请求的自增序号（从 1 起，回绕时跳过 0）。仅在 bWantsWorldLaunch 为真的那一帧有意义。
	 *
	 * 输入包按 game 帧生产、按物理步消费：game 线程卡顿时同一份输入包会被连续多个物理步复用，
	 * 单看脉冲位每一步都会再触发一次。sim 侧把已消费的序号记进 FYanWorldTriggerState，相同序号不再触发。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	uint8 WorldLaunchRequestId = 0;

	/**
	 * 发射板给出的叠加速度（世界空间，cm/s）。随包上传而非 sim 侧重算：
	 * 速度取决于发射板参数与 Pawn 朝向，物理线程访问不到 Actor。仅在 bWantsWorldLaunch 为真的那一帧有意义。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	FVector WorldLaunchVelocity = FVector::ZeroVector;

	/**
	 * 踩中传送门的意图（单帧脉冲）。写入：UYanWorldTriggerInputComponent；
	 * 读取：UChaosCharacterTeleporterCheck。与 bWantsWorldLaunch 同一契约。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	bool bWantsWorldTeleport = false;

	/** 传送请求的自增序号，与 WorldLaunchRequestId 同一契约 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	uint8 WorldTeleportRequestId = 0;

	/** 传送目标位置（世界空间），仅在 bWantsWorldTeleport 为真的那一帧有意义 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	FVector WorldTeleportLocation = FVector::ZeroVector;

	/** 传送后的朝向（世界空间） */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	FRotator WorldTeleportRotation = FRotator::ZeroRotator;

	/** true 则传送成功后把速度覆盖为 WorldTeleportExitVelocity，false 保留按旋转差转向后的入口速度 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	bool bWorldTeleportOverridesVelocity = false;

	/** 传送后的出口速度（世界空间，cm/s），仅 bWorldTeleportOverridesVelocity 为真时生效 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Yan|Mover|Input")
	FVector WorldTeleportExitVelocity = FVector::ZeroVector;

	//~Begin FMoverDataStructBase Interface
	UE_API virtual FMoverDataStructBase* Clone() const override;
	UE_API virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	UE_API virtual UScriptStruct* GetScriptStruct() const override;
	UE_API virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	UE_API virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
	UE_API virtual void Merge(const FMoverDataStructBase& From) override;
	UE_API virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	UE_API virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
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

#undef UE_API
