#pragma once

#include "GameFramework/Actor.h"
#include "ColorTransferTypes.h"

#include "ColorWindZone.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class UAbilitySystemComponent;
class UBoxComponent;
class UNiagaraComponent;

/**
 * 风场：风吹时区域内的角色不能发射颜色射线、外来颜料持续衰减，从区域外射来的射线被风截断。
 *
 * 风从 Actor 的 -Y 吹向 +Y；WindVolume 只阻挡颜色射线的通道，角色可以正常穿过。
 * AllowedPigment 不为 None 时只有该队伍颜色的角色能通过，其余角色进入即死亡。
 * PulsePeriod 大于 0 时风每个周期停 PulseOffDuration 秒，各端按服务器时间自行计算相位。
 * 判定只在服务器上进行；AuthDeactivate 直接销毁风场。
 */
UCLASS(MinimalAPI)
class AColorWindZone : public AActor
{
	GENERATED_BODY()

public:
	UE_API AColorWindZone();

	/** 销毁风场，仅在服务器上调用。 */
	UE_API void AuthDeactivate();

	UFUNCTION(BlueprintPure, Category = "Yan|ColorTransfer")
	UE_API bool IsWindBlowing() const;

	//~Begin AActor Interface
	UE_API virtual void Tick(float DeltaSeconds) override;
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

protected:
	//~Begin AActor Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End AActor Interface

	/** 风的作用范围，同时截断颜色射线 */
	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UBoxComponent> WindVolume;

	/** 风的表现，需带 User 参数 WindColor / BoxSize / WindVelocity */
	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UNiagaraComponent> WindEffect;

	/** 区域内外来颜料每秒衰减量 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.0"))
	float DecayPerSecond = 2.0f;

	/** 可以通过的队伍颜色；None 表示不限 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer")
	EColorPigment AllowedPigment = EColorPigment::None;

	/** 停风周期（秒），0 表示一直吹 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.0", ForceUnits = "s"))
	float PulsePeriod = 0.0f;

	/** 每个周期中停风的时长 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.0", ForceUnits = "s", EditCondition = "PulsePeriod > 0"))
	float PulseOffDuration = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer")
	FLinearColor WindColor = FLinearColor(0.8f, 0.9f, 1.0f, 0.5f);

private:
	// 服务器定时结算区域内的角色
	void AuthUpdatePawnsInVolume();

	// 按新的区域内名单增删禁射，传空集合即全部撤销
	void AuthSetBlockedAbilitySystems(TSet<TWeakObjectPtr<UAbilitySystemComponent>>&& NewBlocked);

	// 按风的开关切换射线阻挡与特效
	void ApplyWindState(bool bBlowing);

	// 停风周期的相位起点，服务器世界时间
	UPROPERTY(Replicated)
	double PulseStartTime = 0.0;

	// 已被本风场施加禁射的角色
	TSet<TWeakObjectPtr<UAbilitySystemComponent>> BlockedAbilitySystems;

	bool bAppliedBlowing = true;

	FTimerHandle UpdateTimer;
};

#undef UE_API
