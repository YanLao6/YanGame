#pragma once

#include "Abilities/YanGameplayAbility.h"

#include "Ability_ColorPurple.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class AColorPurpleProjectile;
class UAnimMontage;
struct FGameplayAbilityTargetDataHandle;

/**
 * 茈：紫爆倒计时期间（持有 Status_PurpleCharging）按住右键蓄力，松开后朝屏幕中心瞄准的位置射出茈，命中其他玩家即令其死亡。
 *
 * 按下即中断自爆，蓄力不限时；蓄力期间茈挂在 ProjectileSocket 上。瞄准同 RedBlinkShot：
 * 本地控制端沿相机中心射线检测，把命中结果作为 TargetData 交给服务器，服务器让茈从插槽飞向该点。
 * 与右键蓄力技能绑在同一输入上；紫爆时身上必有外来颜料，蓄力技能此时本就不能激活，两者不会同时触发。
 */
UCLASS(MinimalAPI)
class UAbility_ColorPurple : public UYanGameplayAbility
{
	GENERATED_BODY()

public:
	UE_API explicit UAbility_ColorPurple(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UGameplayAbility Interface
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	UE_API virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End UGameplayAbility Interface

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Purple")
	TSubclassOf<AColorPurpleProjectile> ProjectileClass;

	/** 蓄力期间茈附着的骨骼 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Purple")
	FName ProjectileSocket = TEXT("finger_r");

	/** 瞄准射线长度（cm），未命中时朝射线终点发射 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Purple", Meta = (ClampMin = "1.0"))
	float AimRange = 5000.0f;

	/** 蓄力期间循环播放的动画，可空 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Purple|Animation")
	TObjectPtr<UAnimMontage> ChargeMontage;

	/** 松手发射的动画，可空 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Purple|Animation")
	TObjectPtr<UAnimMontage> FireMontage;

private:
	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

	// 服务器：收到本地控制端上传的瞄准结果
	void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);

	// 服务器：让茈脱手飞向瞄准点
	void AuthFire(const FHitResult& AimHit);

	// 服务器：中断紫爆并在插槽上生成茈；倒计时已结束（已判死）时返回 false
	bool AuthBeginCharge();

	void PlayMontage(UAnimMontage* Montage);

	/** 仅服务器，蓄力期间有效，发射后交出 */
	UPROPERTY(Transient)
	TObjectPtr<AColorPurpleProjectile> Projectile;

	FDelegateHandle TargetDataHandle;
};

#undef UE_API
