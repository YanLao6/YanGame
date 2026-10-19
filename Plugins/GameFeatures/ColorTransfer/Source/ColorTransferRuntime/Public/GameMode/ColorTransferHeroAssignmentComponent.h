#pragma once

#include "HeroAssignmentComponent.h"

#include "ColorTransferHeroAssignmentComponent.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class UModularPawnData;

/**
 * 颜色传递的角色分配：每支队伍对应一个固定角色，队伍由房主开房时的大厅名单决定。
 *
 * 建队组件与本组件的回调先后不受 Experience 注入顺序保证，
 * 分配时队伍尚未落定的玩家会等到其队伍变化后再补登角色并补写 PawnData。
 * 队伍已定但未配置角色时，退回基类的默认分配。
 */
UCLASS(MinimalAPI, Blueprintable)
class UColorTransferHeroAssignmentComponent : public UHeroAssignmentComponent
{
	GENERATED_BODY()

public:
	//~Begin UHeroAssignmentComponent Interface
	UE_API virtual bool IsManagingHeroAssignment() const override;
	//~End UHeroAssignmentComponent Interface

protected:
	//~Begin UHeroAssignmentComponent Interface
	UE_API virtual const UModularPawnData* ChooseHeroForController(const AController* Controller) override;
	//~End UHeroAssignmentComponent Interface

	/** 队伍编号 -> 该队角色；编号须与建队组件的 TeamsToCreate 一致（1 = 红方、2 = 蓝方）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Hero")
	TMap<int32, TObjectPtr<UModularPawnData>> TeamHeroes;

private:
	UFUNCTION()
	void OnPlayerTeamChanged(UObject* ObjectChangingTeam, int32 OldTeamID, int32 NewTeamID);
};

#undef UE_API
