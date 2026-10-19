#pragma once

#include "ChaosMover/ChaosMoverSimulation.h"

#include "YanChaosMoverSimulation.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 项目角色 Mover 仿真。
 *
 * 在基类的接触修改阶段追加一条角色间规则：两名角色相撞时，只有主动接近的一方参与
 * 冲量交换，被动一方按无限质量求解，因而不会被撞开。碰撞约束本身保留，阻挡、站位
 * 与角色对场景物件的交互均不受影响。
 *
 * 规则只读取双方的位置与速度，故两端仿真遍历同一接触对时结论一致，可安全参与重模拟。
 *
 * 另在每个物理步收尾时恢复 FYanFatigueState 各通道的疲劳度。恢复与当前运动模式无关，
 * 故放在仿真层统一处理，而非散落到各模式的 SimulationTick。
 */
UCLASS(MinimalAPI, BlueprintType)
class UYanChaosMoverSimulation : public UChaosMoverSimulation
{
	GENERATED_BODY()

protected:
	//~Begin UChaosMoverSimulation Interface
	UE_API virtual void OnPostSimulationTick(const FMoverTimeStep& TimeStep, UE::ChaosMover::FSimulationOutputData& OutputData) override;
	UE_API virtual void OnModifyContacts(const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationInputData& InputData, const UE::ChaosMover::FSimulationOutputData& OutputData, Chaos::FCollisionContactModifier& Modifier) override;
	//~End UChaosMoverSimulation Interface
};

#undef UE_API
