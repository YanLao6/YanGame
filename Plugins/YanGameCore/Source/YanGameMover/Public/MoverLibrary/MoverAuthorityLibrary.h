#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MoverMode/Attraction/YanAttractionState.h"

#include "MoverAuthorityLibrary.generated.h"

#define UE_API YANGAMEMOVER_API

class UMoverComponent;

/** 游戏线程中由服务器向目标 Mover 下发 Authority sim action，不写目标的输入包。 */
UCLASS(MinimalAPI)
class UMoverAuthorityLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 以服务器权威方式向目标 Mover 施加瞬时速度，用于跨 Pawn 施力（击飞、念力推拉等）。
	 *
	 * 经 NetworkPhysics sim action 以 Authority 风格下发：服务器单方面决定，目标客户端不做预测。
	 * 目标玩家的输入包只有其本地端能写，跨 Pawn 施力无法走输入包路径，只能由此下发。
	 * 仅在目标 Owner 具备 authority 时生效，且要求目标挂有 UNetworkPhysicsComponent（ChaosMover 后端）。
	 *
	 * @param bOverrideVelocity true 覆盖目标当前速度（强度稳定可预测），false 叠加到当前速度
	 * @param bScheduleForSync  true 按 EventSchedulingMinDelaySeconds 延迟调度，各端在同一物理帧生效、
	 *                          目标端无回滚修正，代价是同等长度的生效延迟；false 立即下发，无延迟但目标端会被修正
	 * @return 成功入队返回 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Launch")
	static UE_API bool ApplyChaosVelocity(UMoverComponent* MoverComp, FVector WorldSpaceVelocity, bool bOverrideVelocity = true, bool bScheduleForSync = true);

	/**
	 * 以服务器权威方式把目标 Pawn 传送到指定位姿，用于传送门一类的世界 Actor。
	 *
	 * 经 NetworkPhysics sim action 以 Authority 风格下发，走 ChaosMover 的 AttemptTeleport：
	 * 目标点被占据等情况会被 CanTeleport 拒绝，此时 Pawn 原地不动、出口速度也不施加。
	 * 目标 Owner 须具备 authority 且挂有 UNetworkPhysicsComponent（ChaosMover 后端）。
	 *
	 * @param TargetLocation        传送目标位置（世界空间）
	 * @param TargetRotation        传送后的朝向（世界空间）
	 * @param bOverrideVelocity     true 则传送成功后把速度覆盖为 VelocityAfterTeleport，false 保留按旋转差转向后的入口速度
	 * @param VelocityAfterTeleport 出口速度（世界空间，cm/s），仅 bOverrideVelocity 为 true 时生效
	 * @param bScheduleForSync      同 ApplyAuthoritativeVelocityToTarget
	 * @return 成功入队返回 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Teleport")
	static UE_API bool ApplyTeleport(UMoverComponent* MoverComp, FVector TargetLocation, FRotator TargetRotation, bool bOverrideVelocity = false, FVector VelocityAfterTeleport = FVector::ZeroVector, bool bScheduleForSync = true);

	/**
	 * 以服务器权威方式让目标 Mover 在 DurationMs 内快速位移到 TargetLocation（FYanLayeredMove_BlinkTo），视觉上连续。
	 *
	 * 与 ApplyRadialAttractionToTarget 同为 Authority sim action：位移由别人发起，目标端的输入包不归本端写。
	 * 起点取服务器当前位置；按 EventSchedulingMinDelaySeconds 延迟调度后的实际起点略有出入，
	 * BlinkTo 按位置差反推速度，首帧即收敛。终点须由调用方预先扫描收敛（见 SweepBlinkTarget）。
	 *
	 * @param ForceMovementMode 位移期间切换的移动模式，默认 Falling 以避开地面摩擦与台阶处理
	 * @return 成功入队返回 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Blink")
	static UE_API bool ApplyBlink(UMoverComponent* MoverComp, FVector TargetLocation, float DurationMs, float EaseExponent = 0.5f, FName ForceMovementMode = FName("Falling"));

	/**
	 * 以服务器权威方式向目标 Mover 施加一段径向牵引，用于苍这类持续拉扯的效果。
	 *
	 * 与 ApplyAuthoritativeVelocityToTarget 同为 Authority sim action，区别在于下发的是
	 * 一个持续若干毫秒的 LayeredMove 而非单帧速度：牵引须在目标接近或远离的过程中持续起作用。
	 *
	 * 引力中心以坐标传入而非 Actor：LayeredMove 在物理线程求值，那里访问 AActor 不安全。
	 * 中心会移动时由调用方按刷新间隔反复下发，DurationMs 取略大于间隔即可无缝衔接。
	 *
	 * @param AttractionCenter 引力中心（世界空间）
	 * @param Radius           作用半径（cm），超出完全无牵引
	 * @param Magnitude        中心处的牵引速度（cm/s）
	 * @param FalloffExponent  衰减指数，作用于 (1 - 距离/半径)；1 为线性，越大越集中在近处
	 * @param DurationMs       持续时间（毫秒）
	 * @param bIsPush          true 推开、false 拉近
	 * @return 成功入队返回 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Launch")
	static UE_API bool ApplyRadialAttraction(UMoverComponent* MoverComp, FVector AttractionCenter, float Radius, float Magnitude, float FalloffExponent = 1.f, float DurationMs = 200.f, bool bIsPush = false);

	/**
	 * Authority 刷新同一来源的飞行牵引输入，保留模拟侧捕获状态；目标须在蓝图配置 UChaosAttractionMode 与进入判定。
	 * Settings.SourceId 必须非零、UpdateSequence 必须递增；Radius <= 0 取消该来源，DurationMs 是刷新后的存续时长。
	 * 中心快照的模拟时间由本接口填写。仅适用于具备 NetworkPhysicsComponent 的目标。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Attraction")
	static UE_API bool RefreshRadialAttraction(UMoverComponent* MoverComp, const FYanAttractionSettings& Settings, bool bScheduleForSync = true);

	/**
	 * 以服务器权威方式把被钩住的 Pawn 朝汇合点牵引，与发起者侧的 UChaosGrapplingMode 构成对称的两端。
	 *
	 * 与 ApplyRadialAttractionToTarget 同为 Authority sim action，区别在于下发的牵引复用
	 * 钩锁那一套积分（减半的钩锁重力 + 主动拉力 + 合速度限速），且为覆盖式，
	 * 被钩者自身的下落与地面摩擦不再把牵引吃掉。
	 *
	 * 汇合点每帧都在动，由调用方按刷新间隔反复下发，DurationMs 取略大于间隔即可无缝衔接。
	 *
	 * @param MeetPoint        牵引汇合点（世界空间），取自发起方输入包的 GrappleAnchorPoint
	 * @param PullAcceleration 朝汇合点的主动拉力加速度（cm/s^2）
	 * @param MaxPullSpeed     合速度上限（cm/s）
	 * @param StopDistance     距汇合点在此以内即不再施力（cm）
	 * @param DurationMs       持续时间（毫秒）
	 * @return 成功入队返回 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Grappling")
	static UE_API bool ApplyGrapplePull(UMoverComponent* MoverComp, FVector MeetPoint, float PullAcceleration = 3000.f, float MaxPullSpeed = 2500.f, float StopDistance = 80.f, float DurationMs = 200.f);

	/**
	 * 以服务器权威方式让目标在一段时间内无法落地，用于击飞这类不该被地面摩擦接管的位移。
	 *
	 * 下发一个只携带 Mover.DisableLanding 标签的 LayeredMove：首帧把目标踢进 Falling，
	 * 此后仅以标签压住落地判定，到期自动失效。与击飞速度分开下发，两者时长可各自调。
	 *
	 * @param DurationSeconds 禁止落地的持续时间（秒）
	 * @return 成功入队返回 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Launch")
	static UE_API bool ApplyNoLanding(UMoverComponent* MoverComp, float DurationSeconds = 0.3f);

	/**
	 * 以服务器权威方式钉住被抓方并扳正其朝向，与发起方共同构成贴身抓取。
	 *
	 * 与发起方那一份效果的差别只在参数：被抓方无须位移（落位点即其自身当前位置），
	 * 但朝向被接管。下落时刻 FallStartSimTimeMs 必须与发起方取自同一来源——
	 * 本效果经 sim action 下发，生效时刻本就晚于发起方一个往返，
	 * 各自计时会让高度差随时间线性拉大，双方须在同一绝对时刻一同起落。
	 *
	 * 挣脱不走本通道：被抓方凭自身输入包中的 bWantsToBreakFree 当帧解除，无需等待往返。
	 *
	 * @param AnchorLocation     被抓方的落位点（世界空间），取其发起帧位置
	 * @param FallStartSimTimeMs 双方共同的下落起始时刻（绝对仿真时间，毫秒）
	 * @param TargetYaw          被抓方被扳向的朝向（度，世界空间）
	 * @param MaxDurationSeconds 存续上限（秒），到期强制解除以免异常状态下被永久钉住
	 * @return 成功入队返回 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Clinch")
	static UE_API bool ApplyClinch(UMoverComponent* MoverComp, FVector AnchorLocation, double FallStartSimTimeMs, float TargetYaw, float MaxHoldSpeed = 600.f, float TurnRateDegrees = 720.f, float MaxDurationSeconds = 8.f);
};

#undef UE_API
