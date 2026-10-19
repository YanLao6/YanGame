#pragma once

#include "Abilities/YanGameplayAbility.h"

#include "Ability_ColorCharge.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class AColorChargeOrb;
class UAnimMontage;

/**
 * 右键蓄力技能的公共流程：按下开始蓄力，手上出现蓄力球；松开后按蓄力时长交给子类发射。
 *
 * 身上有外来颜料时不能激活。蓄力时长以服务器上 WaitInputRelease 的计时为准，
 * 超过 MaxChargeTime 按满蓄力处理。蓄力球只在服务器生成，挂在施法者首个骨骼网格的 OrbSocket 上。
 */
UCLASS(MinimalAPI, Abstract)
class UAbility_ColorCharge : public UYanGameplayAbility
{
	GENERATED_BODY()

public:
	UE_API explicit UAbility_ColorCharge(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UGameplayAbility Interface
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	UE_API virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End UGameplayAbility Interface

protected:
	//~Begin UGameplayAbility Interface
	UE_API virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	//~End UGameplayAbility Interface

	/**
	 * 松手时在各端调用；子类负责在合适的时机 EndAbility。
	 * @param ChargeSeconds 已截断到 [0, MaxChargeTime]
	 */
	UE_API virtual void OnChargeReleased(float ChargeSeconds);

	UE_API float GetChargeAlpha(float ChargeSeconds) const;

	/** 蓄力档位下标，0..ChargeTierCount-1，按蓄力时长均分 */
	UE_API int32 GetChargeTier(float ChargeSeconds) const;

	/** 本端从激活起算的蓄力时长，已截断到 [0, MaxChargeTime] */
	UE_API float GetHeldSeconds() const;

	/** 蓄力期间播放的动画，技能结束时停止；需循环时由 Montage 段落自连实现。可空 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorCharge|Animation")
	TObjectPtr<UAnimMontage> ChargeMontage;

	/** 松手发射的动画，可空 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorCharge|Animation")
	TObjectPtr<UAnimMontage> ReleaseMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorCharge", Meta = (ClampMin = "0.1", ForceUnits = "s"))
	float MaxChargeTime = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorCharge", Meta = (ClampMin = "1"))
	int32 ChargeTierCount = 6;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorCharge")
	TSubclassOf<AColorChargeOrb> OrbClass;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorCharge")
	FName OrbSocket;

	/** 仅服务器，蓄力期间与发射前有效 */
	UPROPERTY(Transient)
	TObjectPtr<AColorChargeOrb> Orb;

private:
	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

	// 服务器：刷新蓄力球的进度与档位
	void UpdateOrbCharge();

	void PlayMontage(UAnimMontage* Montage, bool bStopWhenAbilityEnds);

	double ChargeStartTime = 0.0;

	FTimerHandle ChargeTimer;
};

#undef UE_API
