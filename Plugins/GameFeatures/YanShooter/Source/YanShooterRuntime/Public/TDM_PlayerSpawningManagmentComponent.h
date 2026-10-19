// 队伍死斗出生点管理组件声明。

#pragma once

#include "CoreMinimal.h"
#include "ActorComponent/ModularPlayerSpawningComponent.h"
#include "TDM_PlayerSpawningManagmentComponent.generated.h"

class AActor;
class AController;
class AModularPlayerStart;

/**
 * 队伍死斗出生点管理组件。
 *
 * 选取离敌方队伍现有角色最远的空闲出生点，以降低出生即遭遇的概率。
 * 全部空闲点都不可用时，退而选取距离最远的已占用点。
 */
UCLASS(MinimalAPI)
class UTDM_PlayerSpawningManagmentComponent : public UModularPlayerSpawningComponent
{
	GENERATED_BODY()

public:
	UTDM_PlayerSpawningManagmentComponent(const FObjectInitializer& ObjectInitializer);

	//~Begin UModularPlayerSpawningComponent Interface
	virtual AActor* OnChoosePlayerStart(AController* Player, TArray<AModularPlayerStart*>& PlayerStarts) override;
	virtual void OnFinishRestartPlayer(AController* Player, const FRotator& StartRotation) override;
	//~End UModularPlayerSpawningComponent Interface
};
