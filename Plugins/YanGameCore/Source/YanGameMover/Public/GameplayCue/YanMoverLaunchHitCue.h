#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "YanMoverLaunchHitCue.generated.h"

#define UE_API YANGAMEMOVER_API

class USoundBase;
class UParticleSystem;

/**
 * 击飞命中 GameplayCue（播放逻辑驻留 C++）。
 *
 * 由击飞 GameplayEffect 的 GameplayCues 触发：服务器施加击飞 GE 时经 ASC 复制到各端（含模拟端），
 * 在命中目标处播放命中音效/特效。
 *
 * 发现机制说明：GameplayCueManager 仅扫描内容路径中的 Blueprint 资产建立 Tag→Class 映射，
 * 纯原生类不会被自动发现。需为本类创建一个 data-only 蓝图子类，设置其 GameplayCueTag 与音效/特效
 * 资产并置于 GameplayCue 内容路径；本类只承载播放逻辑，资产与 Tag 由蓝图子类绑定。
 */
UCLASS(MinimalAPI)
class UYanMoverLaunchHitCue : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	//~Begin UGameplayCueNotify_Static Interface
	UE_API virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	//~End UGameplayCueNotify_Static Interface

protected:
	/** 命中击飞音效 */
	UPROPERTY(EditDefaultsOnly, Category = "LaunchHit")
	TObjectPtr<USoundBase> HitSound;

	/** 命中击飞特效（可选） */
	UPROPERTY(EditDefaultsOnly, Category = "LaunchHit")
	TObjectPtr<UParticleSystem> HitEffect;
};

#undef UE_API
