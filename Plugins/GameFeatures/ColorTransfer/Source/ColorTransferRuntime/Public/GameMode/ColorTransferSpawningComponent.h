#pragma once

#include "ActorComponent/ModularPlayerSpawningComponent.h"
#include "ColorTransferTypes.h"

#include "ColorTransferSpawningComponent.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class APlayerState;

/**
 * 颜色传递的出生规则：每支队伍固定出生在 PlayerStartTag 匹配的出生点，出生时按队伍重置颜料。
 *
 * 队伍编号须与建队组件的 TeamsToCreate 一致（大厅约定 1 = 红方、2 = 蓝方）。
 * 找不到匹配出生点时退回基类的随机选点。
 */
UCLASS(MinimalAPI, Blueprintable, Meta = (BlueprintSpawnableComponent))
class UColorTransferSpawningComponent : public UModularPlayerSpawningComponent
{
	GENERATED_BODY()

public:
	UE_API explicit UColorTransferSpawningComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 为玩家记录已激活的复活点。 */
	UE_API void SetPlayerCheckpoint(AController* Player, AActor* Checkpoint);

	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	//~End UActorComponent Interface

protected:
	//~Begin UModularPlayerSpawningComponent Interface
	UE_API virtual AActor* OnChoosePlayerStart(AController* Player, TArray<AModularPlayerStart*>& PlayerStarts) override;
	UE_API virtual void OnFinishRestartPlayer(AController* Player, const FRotator& StartRotation) override;
	//~End UModularPlayerSpawningComponent Interface

	/** 队伍编号 -> 出生点的 PlayerStartTag */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	TMap<int32, FName> TeamStartTags;

	/** 队伍编号 -> 自身颜色 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	TMap<int32, EColorPigment> TeamPigments;

private:
	int32 FindTeamId(const AController* Player) const;
	TMap<TWeakObjectPtr<APlayerState>, TWeakObjectPtr<AActor>> PlayerCheckpoints;
};

#undef UE_API
