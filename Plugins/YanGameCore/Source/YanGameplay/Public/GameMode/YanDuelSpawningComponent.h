#pragma once

#include "ActorComponent/ModularPlayerSpawningComponent.h"

#include "YanDuelSpawningComponent.generated.h"

#define UE_API YANGAMEPLAY_API

class AController;
class AModularPlayerStart;

/**
 * 对抗出生点选择：始终挑离对手最远的出生点。
 *
 * 出界即重生的规则下，随机出生点会把刚复活的一方直接送到对手面前，
 * 让追杀方无成本地续上压制。取最远点使复活方至少拿回一次接近战场的选择权。
 *
 * 仅按对手当前位置排序，不考虑地形连通性；场地为悬空平台，直线距离已足够近似。
 */
UCLASS(MinimalAPI)
class UYanDuelSpawningComponent : public UModularPlayerSpawningComponent
{
	GENERATED_BODY()

public:
	UE_API explicit UYanDuelSpawningComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~Begin UModularPlayerSpawningComponent Interface
	UE_API virtual AActor* OnChoosePlayerStart(AController* Player, TArray<AModularPlayerStart*>& PlayerStarts) override;
	//~End UModularPlayerSpawningComponent Interface

private:
	// 收集除 Player 外所有已具备 Pawn 的控制器位置；无对手时返回空
	UE_API TArray<FVector> GatherOpponentLocations(const AController* Player) const;
};

#undef UE_API
