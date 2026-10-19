#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "MoverTargetingLibrary.generated.h"

#define UE_API YANGAMEMOVER_API

class APawn;
class UMoverComponent;

/** 游戏线程中的 Mover 目标与路径扫描；结果不能在物理线程或回滚中重新查询。 */
UCLASS(MinimalAPI)
class UMoverTargetingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 从摄像机位置沿前向量做钩锁 LineTrace（WorldStatic 通道），返回是否命中及命中点。
	 * 自动忽略 Pawn 自身。MaxDistance 为本次探测最大距离（cm）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Grappling")
	static UE_API bool LineTraceForGrapple(UMoverComponent* MoverComp, float MaxDistance, FVector& OutImpactPoint);

	/**
	 * 沿给定方向扫描瞬移终点，以角色自身胶囊为形状做 sweep，被挡则停在障碍前。
	 *
	 * 终点必须由发起端一次算定并随输入上传：扫描依赖 game-thread 的碰撞场景，
	 * 客户端与服务器各自扫描不保证得到同一结果，两端各算一次会造成轨迹分歧。
	 * 不校验落点是否可站立——瞬移到空中由后续的下落模式接管，属预期行为。
	 *
	 * @param Direction         瞬移方向（世界空间，内部归一化）
	 * @param MaxDistance       最大瞬移距离（cm）
	 * @param OutTargetLocation 扫描所得终点（世界空间，胶囊中心）
	 * @return 方向或距离无效时返回 false，Out 参数不写入
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Blink")
	static UE_API bool SweepBlinkTarget(UMoverComponent* MoverComp, FVector Direction, float MaxDistance, FVector& OutTargetLocation);

	/**
	 * 扫描贴身抓取目标：取当前速度方向前方扇形内、最近的一个带 ASC 的 Pawn。
	 *
	 * 方向基准取速度而非视线——抓取由冲刺撞上触发，判定的是「撞到」而非「瞄准」，
	 * 擦身而过不应命中。判定在发起端做一次即定，落位点随输入包上传，服务器不重算：
	 * 目标位置在发起端是插值外推的结果，两端各自扫描不保证得到同一落点。
	 *
	 * 落位点与目标等高，且水平取双方胶囊半径之和加间隙——抓取双方的垂直运动靠同一
	 * 运动方程保持同步，一方先落地即破坏守恒，故不允许有高度差；留出间隙则是为了
	 * 两个物理体从不产生穿透，否则接触约束会把贴住的双方持续推开。
	 *
	 * @param SearchRadius      搜索半径（cm）
	 * @param HalfAngleDegrees  与速度方向的夹角在此以内才算命中
	 * @param SurfaceClearance  两胶囊表面之间额外留出的间隙（cm）
	 * @param OutAnchorPoint    发起方应当停靠的落位点（世界空间）
	 * @return 未找到目标时返回 nullptr，Out 参数不写入
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Clinch")
	static UE_API APawn* FindClinchTarget(UMoverComponent* MoverComp, float SearchRadius, float HalfAngleDegrees, float SurfaceClearance, FVector& OutAnchorPoint);

	/**
	 * 预扫描贴墙推进的路径折线：自被推方位置探到墙面，再沿墙面逐步采样出一条贴合曲面的折线。
	 *
	 * 整条路径一次算定并随输入上传，此后不再重扫。逐帧沿墙重扫看似更贴合，但扫描结果两端
	 * 必然分歧，且被推方的效果由服务器下发、拿不到发起方的输入包，无从复现一条只有发起方
	 * 在扫的曲线。代价是折线不随墙体移动而更新。
	 *
	 * 折线取的是**发起方**的行进位置（已含离墙间隙与双方身位），被推方的偏移由服务器在
	 * 下发其效果时按被推方的权威位置定出，不在此处计算。
	 *
	 * 采样在下列任一情形提前收尾，此时折线短于理论推进距离，推进随之提前结束：
	 * 前方被阻挡（内凹角、柱子）、侧向探不到墙（墙到头）、法线退化（非竖直面）。
	 *
	 * @param TargetLocation    被推方位置，探墙自此出发
	 * @param WallProbeDistance 探墙距离（cm），超出即认为附近无墙
	 * @param PairSeparation    折线与墙面的距离（cm），须容得下被推方加间隙
	 * @param StepDistance      沿墙采样步长（cm），越小越贴合曲面，代价是点数上限更快耗尽
	 * @param Deceleration      摩擦减速度（cm/s²），与初速共同决定折线需要多长
	 * @param MaxPoints         折线点数上限，内部再按输入包的上限收紧
	 * @param OutPath           采样所得折线（世界空间）
	 * @param OutEntrySpeed     速度在墙面内的投影长度（cm/s）
	 * @return 未找到墙面或折线不足两点时返回 false，Out 参数不保证有效
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|WallGrind")
	static UE_API bool TraceWallGrindPath(UMoverComponent* MoverComp,
	                                      FVector          TargetLocation,
	                                      float            WallProbeDistance,
	                                      float            PairSeparation,
	                                      float            StepDistance,
	                                      float            Deceleration,
	                                      int32            MaxPoints,
	                                      TArray<FVector>& OutPath,
	                                      float&           OutEntrySpeed);
};

#undef UE_API
