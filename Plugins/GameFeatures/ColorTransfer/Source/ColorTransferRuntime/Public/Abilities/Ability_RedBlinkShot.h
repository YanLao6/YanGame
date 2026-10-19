#pragma once

#include "Abilities/Ability_ColorCharge.h"

#include "Ability_RedBlinkShot.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class AColorAttractOrb;
struct FGameplayAbilityTargetDataHandle;

/**
 * 红方右键：蓄力后朝屏幕中心瞄准的位置射出红球。
 *
 * 瞄准沿用 RangedWeapon 的做法：本地控制端沿相机中心射线检测，把命中结果作为 TargetData 交给服务器，
 * 服务器按自己计时的蓄力档位截断射程后结算。红球只是表现，飞到命中点才生效：
 * 命中 Pawn 则注入对应档位的红色，红球挂到对方 HitAttachSocket 上，对方沿射线方向获得
 * 对应档位的瞬时速度并短时间内无法落地，红球随后消失。
 * 射线先碰到蓝方吸引球（任何阶段）时，红球飞抵后与之一同在碰撞点爆炸：半径内的 Pawn 按档位注入红与蓝（跳过其自身颜色）。
 */
UCLASS(MinimalAPI)
class UAbility_RedBlinkShot : public UAbility_ColorCharge
{
	GENERATED_BODY()

public:
	UE_API explicit UAbility_RedBlinkShot(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UGameplayAbility Interface
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	UE_API virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End UGameplayAbility Interface

protected:
	//~Begin UAbility_ColorCharge Interface
	UE_API virtual void OnChargeReleased(float ChargeSeconds) override;
	//~End UAbility_ColorCharge Interface

	/** 各档位携带的红色 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|RedBlink")
	TArray<float> PigmentByTier = {5.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f};

	/** 最低档与最高档的射程（cm） */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|RedBlink")
	FVector2D RangeByCharge = FVector2D(300.0f, 3000.0f);

	/** 命中后红球附着的骨骼 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|RedBlink")
	FName HitAttachSocket = TEXT("spine_05");

	/** 最低档与最高档施加给被命中者的瞬时速度（cm/s），覆盖其当前速度 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|RedBlink")
	FVector2D LaunchSpeedByCharge = FVector2D(1500.0f, 4000.0f);

	/** 在射线方向速度之外额外附加的竖直向上速度（cm/s），不随档位变化 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|RedBlink")
	float LaunchUpSpeed = 600.0f;

	/** 被命中后禁止落地的时长，避免水平速度被地面摩擦立即吃掉 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|RedBlink", Meta = (ClampMin = "0.0", ForceUnits = "s"))
	float NoLandingDuration = 0.3f;

	/** 命中后红球在对方身上停留的时长，覆盖 sim action 的调度延迟 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|RedBlink", Meta = (ClampMin = "0.0", ForceUnits = "s"))
	float OrbLingerTime = 0.3f;

	/** 引爆吸引球时的波及半径（cm），注入量沿用 PigmentByTier */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|RedBlink", Meta = (ClampMin = "0.0"))
	float DetonateRadius = 400.0f;

private:
	// 服务器：收到本地控制端上传的瞄准结果
	void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);

	// 服务器：按服务器蓄力档位截断射程，让红球飞向命中点
	void AuthFire(const FHitResult& AimHit);

	// 服务器：红球飞抵命中点
	void AuthHandleOrbArrived(TWeakObjectPtr<APawn> WeakTarget, FVector Direction);

	// 服务器：红球飞抵吸引球，两球一同爆炸
	void AuthHandleAttractOrbHit(TWeakObjectPtr<AColorAttractOrb> WeakAttractOrb);

	// 本地控制端：射线穿过的最近吸引球比 InOutHit 更近时，把命中结果替换为吸引球
	void TraceAttractOrbs(FHitResult& InOutHit) const;

	float GetRange(float ChargeSeconds) const;

	// 服务器：发射档位对应的注入量
	float GetFiredPigment() const;

	// 服务器：发射档位对应的击飞速度
	float GetFiredLaunchSpeed() const;

	FDelegateHandle TargetDataHandle;

	// 服务器：发射时冻结的蓄力档位，红球飞行期间蓄力计时仍在走
	int32 FiredTier = 0;
};

#undef UE_API
