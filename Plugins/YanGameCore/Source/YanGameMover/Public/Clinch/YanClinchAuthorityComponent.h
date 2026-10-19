#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NativeGameplayTags.h"

#include "YanClinchAuthorityComponent.generated.h"

#define UE_API YANGAMEMOVER_API

class APawn;
class UGameplayEffect;
class UMoverComponent;
struct FYanCharacterInputs;

/**
 * 被抓状态标签，由 GrabbedEffect 授予。
 *
 * 与 Mover.IsClinched 分属两层且不可互换：后者随 LayeredMove 存在于仿真状态，管的是「能否移动」；
 * 本标签存在于 ASC，管的是「能否放技能」。挣脱须同时解除两者。
 */
YANGAMEMOVER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Grabbed);

/**
 * 贴身抓取的服务器裁决器：读本 Pawn 上传的抓取意图，复检后对被抓方下发钉制效果。
 *
 * 之所以由 game thread 的组件承担，而非在 sim 侧就地完成：跨 Pawn 施力只能经
 * UNetworkPhysicsComponent 的 EnqueueScheduledAction_External 下发，那是 game-thread 接口，
 * 而 sim 侧的 Transition 运行于物理线程，在那里访问其他 Actor 并不安全。
 *
 * 裁决只在意图的上升沿做一次。抓取意图是持续标志（脉冲会被 game tick 漏采样），
 * 故须自行记录已裁决状态，否则每帧都会向被抓方下发一次效果。
 *
 * 本组件不负责解除：被抓方凭自身输入包中的挣脱位当帧解除，发起方松手亦同理，
 * 两者都无须经过服务器往返。
 */
UCLASS(MinimalAPI, Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UYanClinchAuthorityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UE_API UYanClinchAuthorityComponent();

	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~End UActorComponent Interface

protected:
	/**
	 * 复检搜索半径（cm）。取值应略大于发起端的搜索半径：发起端看到的目标位置是插值外推的结果，
	 * 与服务器上的真值有出入，口径完全一致会让边界处的合法抓取被误判为无效。
	 */
	UPROPERTY(EditAnywhere, Category = "Clinch", meta = (ClampMin = "0", ForceUnits = "cm"))
	float VerifyRadius = 500.f;

	/** 复检时与速度方向的夹角上限（度），同样比发起端放宽 */
	UPROPERTY(EditAnywhere, Category = "Clinch", meta = (ClampMin = "1", ClampMax = "180", ForceUnits = "degrees"))
	float VerifyHalfAngle = 90.f;

	/** 被抓方水平回正的速度上限（cm/s） */
	UPROPERTY(EditAnywhere, Category = "Clinch", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxHoldSpeed = 600.f;

	/** 被抓方被扳向发起方的转向速率上限（度/秒） */
	UPROPERTY(EditAnywhere, Category = "Clinch", meta = (ClampMin = "1"))
	float TurnRateDegrees = 720.f;

	/** 被抓方钉制效果的存续上限（秒），到期强制解除 */
	UPROPERTY(EditAnywhere, Category = "Clinch", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float MaxDurationSeconds = 8.f;

	/**
	 * 眩晕效果：短时长，封锁被抓方的全部技能，连挣脱技能一并压住。
	 * 它同时也是一道时序保障——钉制效果经 sim action 下发有生效延迟，
	 * 若被抓方能在此期间抢先按下挣脱，效果会一诞生就被终结；技能被封锁则无从写入挣脱位。
	 * 留空则跳过。
	 */
	UPROPERTY(EditAnywhere, Category = "Clinch|Effects")
	TSubclassOf<UGameplayEffect> StunEffect;

	/**
	 * 被抓效果：与钉制同寿，仅放行挣脱技能。
	 * 放行由 UModularAbilityTagRelationshipMapping 集中配置，无须逐个技能填写激活条件。
	 * 移除由挣脱技能负责，故其时长应长于预期的挣脱耗时。留空则跳过。
	 */
	UPROPERTY(EditAnywhere, Category = "Clinch|Effects")
	TSubclassOf<UGameplayEffect> GrabbedEffect;

private:
	// 扫描目标、复检并下发钉制效果，失败时只记日志
	UE_API void ResolveClinch(const FYanCharacterInputs& YanInputs);

	// 由发起方 ASC 向被抓方施加眩晕与被抓效果，两者留空则各自跳过
	UE_API void ApplyClinchEffects(APawn* Target) const;

	// 本次抓取是否已裁决：意图为持续标志，须只在上升沿下发一次
	bool bClinchResolved = false;

	UPROPERTY(Transient)
	TObjectPtr<UMoverComponent> CachedMoverComp;
};

#undef UE_API
