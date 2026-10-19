#pragma once

#include "Components/PlayerStateComponent.h"
#include "NativeGameplayTags.h"

#include "ColorPigmentRulesComponent.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class UAbilitySystemComponent;
class UColorPigmentSet;
class UNiagaraSystem;

/** 紫爆倒计时期间持有的 Loose Tag（复制到各端），茈以此为激活条件。 */
COLORTRANSFERRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_PurpleCharging);

/**
 * 颜料规则：由身上颜料的比例裁决疾跑锁定、紫爆与倒地。
 *
 * 挂在 PlayerState 上，与颜料属性集同寿命；只在服务器上裁决，结果经 GameplayEvent.Death、
 * 复制的 Loose Tag 与紫爆开始时间传给各端。
 * 一次死亡只裁决一次，重生时由出生规则调用 AuthResetRules 解除。
 */
UCLASS(MinimalAPI, ClassGroup = "ColorTransfer", Meta = (BlueprintSpawnableComponent))
class UColorPigmentRulesComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	UE_API explicit UColorPigmentRulesComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End UActorComponent Interface

	/** 重生后清除上一条命的裁决状态，并按重置后的颜料重新裁决。 */
	UE_API void AuthResetRules();

	/** 由外部机关直接判死（如颜色不符的风场）；与颜料裁决共用同一次死亡。 */
	UE_API void AuthRequestDeath();

	/**
	 * 中断紫爆倒计时并排空外来颜料，发射茈时调用；排空后红蓝不再相等，不会立刻再次进入紫爆。
	 * @return 是否确有倒计时被中断
	 */
	UE_API bool AuthCancelPurple();

	/** 紫爆进度 0..1，未进入紫爆为 0；各端按服务器时间计算，供表现使用。 */
	UE_API float GetPurpleAlpha() const;

protected:
	/** 自身颜料低于此量时锁定疾跑。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	float SprintLockThreshold = 50.0f;

	/** 自身颜料低于此量时倒地死亡。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	float CollapseThreshold = 20.0f;

	/** 红蓝差值不超过此量即视为相等。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	float PurpleTolerance = 0.5f;

	/** 红蓝相等后变紫并自爆所需时长。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.1", ForceUnits = "s"))
	float PurpleDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	TSoftObjectPtr<UNiagaraSystem> PurpleExplosionEffect;

private:
	void EvaluateRules();
	void ExplodePurple();

	// 同步紫爆开始时间、倒计时与 Status_PurpleCharging
	void SetPurpleCharging(bool bCharging);
	void RequestDeath();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPurpleExplosion(FVector_NetQuantize Location);

	UAbilitySystemComponent* GetAbilitySystemComponent() const;

	// 服务器世界时间，小于 0 表示未进入紫爆
	UPROPERTY(Replicated)
	double PurpleStartTime = -1.0;

	bool bDeathRequested = false;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> LoadedExplosionEffect;

	// 颜料集随技能集授予，晚于本组件 BeginPlay，首次重生时才绑定
	TWeakObjectPtr<UColorPigmentSet> BoundPigmentSet;

	FTimerHandle PurpleTimer;
};

#undef UE_API
