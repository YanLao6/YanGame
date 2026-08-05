#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilities/ModularGameplayAbility.h"
#include "ChaosMoverLaunchAbility.generated.h"

#define UE_API YANGAMEMOVER_API

class AYanHookProjectile;
class APawn;
class UGameplayEffect;
class USoundBase;
class UAnimMontage;
class UAbilityTask_WaitInputRelease;

/**
 * Chaos 版发射冲击弹丸技能（服务器权威发射 + LocalPredicted 表现）
 */
UCLASS(MinimalAPI)
class UChaosMoverLaunchAbility : public UModularGameplayAbility
{
	GENERATED_BODY()

public:
	UE_API UChaosMoverLaunchAbility(const FObjectInitializer& Initializer);

	//~Begin UGameplayAbility Interface
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	UE_API virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End UGameplayAbility Interface

protected:
	/** 投射物类（蓝图子类中指定，复用 AYanHookProjectile 子类即可） */
	UPROPERTY(EditDefaultsOnly, Category = "Launch")
	TSubclassOf<AYanHookProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Launch", meta = (ClampMin = "100"))
	float ProjectileSpeed = 3000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Launch", meta = (ClampMin = "0"))
	float LaunchSpeed = 800.f;

	UPROPERTY(EditDefaultsOnly, Category = "Launch", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UpwardRatio = 0.4f;

	/* 命中后对目标施加的击飞 GameplayEffect（Instant）*/
	UPROPERTY(EditDefaultsOnly, Category = "Launch")
	TSubclassOf<UGameplayEffect> LaunchGameplayEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Launch|Feedback")
	TObjectPtr<USoundBase> PressSound;

	UPROPERTY(EditDefaultsOnly, Category = "Launch|Feedback")
	TObjectPtr<UAnimMontage> PressMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Launch|Feedback")
	TObjectPtr<USoundBase> ReleaseSound;

	UPROPERTY(EditDefaultsOnly, Category = "Launch|Feedback")
	TObjectPtr<UAnimMontage> ReleaseMontage;

private:
	UFUNCTION()
	UE_API void OnInputReleased(float TimeHeld);

	UFUNCTION()
	UE_API void HandleProjectileHit(AYanHookProjectile* Projectile, const FHitResult& Hit);

	UFUNCTION()
	UE_API void HandleProjectileMissed(AYanHookProjectile* Projectile);

	// 由投射物前向与 Up 按 UpwardRatio 插值归一化后乘 LaunchSpeed，得到世界空间击飞速度
	UE_API FVector ComputeLaunchVelocity(const AYanHookProjectile* Projectile) const;

	UE_API void SpawnAndLaunchProjectile();

	UE_API void PlayLocalFeedback(USoundBase* Sound, UAnimMontage* Montage) const;

private:
	UPROPERTY()
	TObjectPtr<AYanHookProjectile> PendingProjectile;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputRelease> WaitReleaseTask;
};

#undef UE_API
