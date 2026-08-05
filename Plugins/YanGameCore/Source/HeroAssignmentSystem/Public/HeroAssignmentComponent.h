#pragma once

#include "Components/GameStateComponent.h"
#include "UObject/ObjectKey.h"

#include "HeroAssignmentComponent.generated.h"

#define UE_API HEROASSIGNMENTSYSTEM_API

class AController;
class AGameModeBase;
class UHeroCatalog;
class UModularExperienceDefinition;
class UModularPawnData;

/**
 * 对局内角色分配组件。
 *
 * 由 Experience 通过 AddComponents 注入到 GameState，持有本局候选角色并在服务器侧
 * 为每个玩家决议 PawnData。候选角色使用硬引用，随组件类一同加载，使决议无需同步加载资产。
 *
 * 分配在玩家初始化时一次性完成并登记，之后的决议只做查表，保证同一玩家在 Pawn 生成与
 * PlayerState 写入两条路径上得到一致结果。
 */
UCLASS(MinimalAPI, Blueprintable)
class UHeroAssignmentComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UE_API explicit UHeroAssignmentComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * 查询已登记给该 Controller 的角色数据。
	 *
	 * 返回 nullptr 表示本组件不接管该玩家，调用方应回退到 Experience 的 DefaultPawnData。
	 */
	UE_API const UModularPawnData* ResolveHeroForPlayer(const AController* Controller) const;

	/** 按 PrimaryAssetId 在本局候选列表中查找角色，不在列表内返回 nullptr。 */
	UE_API const UModularPawnData* FindSelectableHero(const FPrimaryAssetId& HeroId) const;

	/**
	 * Authority：为玩家更换角色并触发重生。
	 *
	 * 改写登记与 PlayerState 的 PawnData 后请求下一帧重生：PawnClass 随角色变化，
	 * 只有生成新 Pawn 时才会取到新的 PawnData，旧 Pawn 上的注入随其销毁回收。
	 *
	 * @return 是否成功换人。目标不在目录内、与当前角色一致或 PlayerState 缺失时为 false。
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Hero")
	UE_API bool ChangeHeroForController(AController* Controller, FPrimaryAssetId HeroId);

	/**
	 * 本组件是否接管本局的角色分配。
	 *
	 * 为 true 时，尚未登记的玩家应推迟 PawnData 决议而非回退 Experience 默认值：
	 * 玩家在 Experience 就绪后登录时，PlayerState 的首次查询早于身份建立，
	 * 此时写入默认值会锁死首写，使后续分配失效。
	 */
	UE_API bool IsManagingHeroAssignment() const;

	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	//~End UActorComponent Interface

protected:
	/** 本局使用的角色目录，须与大厅选人界面引用同一份资产。 */
	UPROPERTY(EditDefaultsOnly, Category = "Hero")
	TObjectPtr<UHeroCatalog> HeroCatalog;

	/**
	 * 换人后是否在原位重生，而非回到出生点。
	 *
	 * 开启时新 Pawn 沿用旧 Pawn 的位置与朝向；速度不继承，且新角色碰撞体更大时生成点会被就近微调。
	 * 注意这会让换人可被用作规避位移限制的手段，对局内开放换人时需自行约束使用时机。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Hero")
	bool bRestartInPlace = false;

	// Experience 就绪后分配已在场玩家，并开始监听后续登录
	UE_API void OnExperienceLoaded(const UModularExperienceDefinition* Experience);

	// 为单个玩家决议并登记角色，重复调用对同一 Controller 无副作用
	UE_API void AssignHeroForController(AController* Controller);

	// 选择缺失或非法时，挑选一个尚未被其他玩家占用的候选角色
	UE_API const UModularPawnData* ChooseUnclaimedHero(const AController* Controller) const;

private:
	UFUNCTION()
	UE_API void OnPlayerInitialized(AGameModeBase* GameMode, AController* NewPlayer);

	// 已登记的分配结果。PawnData 的生命周期由 SelectableHeroes 持有，此处不参与 GC 追踪
	TMap<FObjectKey, const UModularPawnData*> AssignedHeroes;
};

#undef UE_API
