#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilities/ModularGameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "MoverLaunchAbility.generated.h"

class AYanHookProjectile;
class APawn;

/**
 * 发射冲击弹丸技能（LocalPredicted 实现）
 *
 * 网络职责划分：
 *   本地受控端从 Pawn 视点 Spawn cosmetic 投射物并做命中探测（位置即玩家看到的预测位置，无滞后）；
 *   命中 Pawn 后通过 GAS ServerSetReplicatedTargetData 把命中结果（FYanLaunchTargetData）回传服务器；
 *   服务器经 AbilityTargetDataSetDelegate 收取，校验后对目标 MoverComponent QueueLaunchMove，
 *   击飞效果经 FMoverSyncState 复制同步到所有客户端。
 *
 * 投射物为本地 cosmetic，服务器不 Spawn、不以其碰撞作为命中真相；命中真相由回传的 TargetData 承载。
 */
UCLASS()
class YANGAMEMOVER_API UMoverLaunchAbility : public UModularGameplayAbility
{
	GENERATED_BODY()

public:
	UMoverLaunchAbility(const FObjectInitializer& Initializer);

	//~Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End UGameplayAbility Interface

protected:
	/** 投射物类（蓝图子类中指定，复用 AYanHookProjectile 子类即可） */
	UPROPERTY(EditDefaultsOnly, Category = "Launch")
	TSubclassOf<AYanHookProjectile> ProjectileClass;

	/** 投射物飞行速度（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "Launch", meta = (ClampMin = "100"))
	float ProjectileSpeed = 3000.f;

	/** 击飞速度大小（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "Launch", meta = (ClampMin = "0"))
	float LaunchSpeed = 150.f;

	/**
	 * 击飞方向中向上分量的插值比例（0 = 纯水平，1 = 纯垂直）。
	 * 混合后再归一化，控制弹飞抛物线的仰角感。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Launch", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UpwardRatio = 0.4f;

	/** FLayeredMove_Launch 持续时间（毫秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Launch", meta = (ClampMin = "0"))
	float LaunchDurationMs = 150.f;

private:
	/** 投射物命中回调（本地受控端）：命中 Pawn 时回传 TargetData，随后结束技能 */
	UFUNCTION()
	void HandleProjectileHit(AYanHookProjectile* Projectile, const FHitResult& Hit);

	/** 投射物超出射程回调（本地受控端）：结束技能 */
	UFUNCTION()
	void HandleProjectileMissed(AYanHookProjectile* Projectile);

	// 服务器收到客户端回传命中数据后施加击飞并结束技能
	void OnLaunchTargetDataReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ActivationTag);

	// 从回传数据中取出命中 Pawn，对其 MoverComponent 施加击飞 LayeredMove（服务器权威）
	void ApplyLaunchFromTargetData(const FGameplayAbilityTargetDataHandle& Data) const;

	// 销毁 cosmetic 投射物并结束技能
	void CleanupAndEnd();

	UPROPERTY()
	TObjectPtr<AYanHookProjectile> PendingProjectile;

	// 服务器侧 TargetData 委托句柄，EndAbility 时反注册
	FDelegateHandle TargetDataDelegateHandle;
};
