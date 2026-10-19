#pragma once

#include "Abilities/SlashAbilityTargetData.h"
#include "Containers/Ticker.h"
#include "Weapon/Abilities/GameplayAbility_RangedWeapon.h"

#include "YanGameplayAbility_Slash.generated.h"

#define UE_API SHINJUKUSHOWDOWNRUNTIME_API

class UGameplayEffect;

/**
 * 第一人称蓄力斩击能力。
 *
 * 按下开始蓄力，松开时才结算：以松手前一小段时间内的视角位移决定挥砍朝向，
 * 从相机出发在梯形扇区内发射多条射线，命中 Pawn 时穿透继续，命中 Destructible 时终止。
 *
 * 瞄准在本地控制端解算，伤害与 Chaos 破坏场都只在权威端执行；挥砍朝向随 TargetData 复制过去。
 * 客户端看到的破坏由 GeometryCollection 自身的复制驱动，因此参与 gameplay 的碎块各端一致。
 */
UCLASS(MinimalAPI)
class UYanGameplayAbility_Slash : public UGameplayAbility_RangedWeapon
{
	GENERATED_BODY()

public:
	UE_API UYanGameplayAbility_Slash(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UGameplayAbility Interface
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	UE_API virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	UE_API virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	//~End UGameplayAbility Interface

	/** 立即结束蓄力并执行斩击结算；重复调用会被忽略。 */
	UFUNCTION(BlueprintCallable, Category = "Slash")
	UE_API void ReleaseSlash();

	/** 返回本次蓄力已持续的时长，单位为秒。 */
	UFUNCTION(BlueprintPure, Category = "Slash")
	float GetChargeElapsedTime() const
	{
		return ChargeElapsedTime;
	}

	/** 返回本次蓄力的归一化进度，1 表示已蓄满。 */
	UFUNCTION(BlueprintPure, Category = "Slash")
	UE_API float GetChargeAlpha() const;

protected:
	//~Begin UGameplayAbility_RangedWeapon Interface
	UE_API virtual void PerformLocalTargeting(OUT TArray<FHitResult>& OutHits) override;
	UE_API virtual void OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag) override;
	//~End UGameplayAbility_RangedWeapon Interface

protected:
	/** 斩击的最远作用距离，超出后射线终止。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slash|Range", meta = (ForceUnits = cm, ClampMin = "1.0"))
	float MaxSlashRange = 5000.0f;

	/** 扇区近端宽度，即紧贴相机处的挥砍宽度。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slash|Range", meta = (ForceUnits = cm, ClampMin = "0.0"))
	float NearSlashWidth = 300.0f;

	/** 扇区远端宽度，即 MaxSlashRange 处的挥砍宽度；与近端线性插值构成梯形扇区。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slash|Range", meta = (ForceUnits = cm, ClampMin = "0.0"))
	float FarSlashWidth = 10000.0f;

	/** 扇区内均匀铺开的射线数量；数值越高越不容易漏检细小目标，代价是每次斩击的查询开销。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slash|Range", meta = (ClampMin = "1"))
	int32 FanTraceCount = 9;

	/** 单条射线允许穿透的 Pawn 上限，用于防止异常碰撞设置导致死循环。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slash|Range", meta = (ClampMin = "0"))
	int32 MaxPawnPenetrations = 8;

	/** 未蓄力时的斩击深度，即破坏场盒子沿视线方向的长度。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slash|Charge", meta = (ForceUnits = cm, ClampMin = "0.0"))
	float MinSlashDepth = 500.0f;

	/** 蓄满时的斩击深度。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slash|Charge", meta = (ForceUnits = cm, ClampMin = "0.0"))
	float MaxSlashDepth = 1000.0f;

	/** 达到最大斩击深度所需的按住时长。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slash|Charge", meta = (ForceUnits = s, ClampMin = "0.01"))
	float MaxChargeTime = 1.5f;

	/** 解算挥砍朝向时回看的时间窗口；只有落在窗口内的视角位移会参与计算。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slash|Direction", meta = (ForceUnits = s, ClampMin = "0.01"))
	float DirectionSampleWindow = 0.15f;

	/** 窗口内的视角位移低于该值时视为玩家没有主动挥动，斩击退化为水平横斩。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slash|Direction", meta = (ForceUnits = deg, ClampMin = "0.0"))
	float MinSlashInputMagnitude = 1.0f;

	/** 命中 Destructible 时生成的 Chaos 破坏场 Actor；通常指向 Field System 蓝图。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slash|Field")
	TSubclassOf<AActor> SlashFieldActorClass;

	/** 破坏场 Actor 生成后的存活时长，0 表示交由 Field 蓝图自行销毁。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slash|Field", meta = (ForceUnits = s, ClampMin = "0.0"))
	float SlashFieldLifeSpan = 0.3f;

	/**
	 * 破坏场 Actor 在缩放为 1 时的盒子尺寸，用于把期望的深度与宽度换算成 Scale。
	 * Z 轴为斩击厚度，不参与缩放，因此更换 Field 蓝图时必须同步此处的基准尺寸。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slash|Field")
	FVector FieldBoxBaseSize = FVector(50.0f, 50.0f, 25.0f);

	/** 对命中的 Pawn 施加的 GameplayEffect；留空则不结算伤害，交由蓝图处理 TargetData。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slash|Damage")
	TSubclassOf<UGameplayEffect> SlashDamageEffect;

private:
	// 一帧的视角位移采样，X 为水平（向右为正），Y 为垂直（向上为正），单位为度
	struct FSlashRotationSample
	{
		float TimeSeconds = 0.0f;
		FVector2D Delta = FVector2D::ZeroVector;
	};

	// 解算瞄准并把几何参数与命中一并提交；父类入口只能产出命中项，故不复用
	void StartSlashTargeting();

	// 每帧记录视角位移；返回 true 保持 ticker 存活
	bool TickSampleRotation(float DeltaTime);

	// 开始与停止视角采样，成对调用
	void StartRotationSampling();
	void StopRotationSampling();

	// 按时间加权聚合采样窗口内的位移，得到屏幕空间的单位挥砍方向；无有效输入时返回零向量
	FVector2D ComputeSampledSlashDirection() const;

	// 在梯形扇区内铺开射线并逐条检测
	void TraceSlashFan(const FVector& Origin, const FVector& AimDir, const FVector& SlashDir, OUT TArray<FHitResult>& OutHits);

	// 检测单条射线，遇 Pawn 穿透、遇 Destructible 或其他阻挡物终止
	void TraceSingleSlashRay(const FVector& RayStart, const FVector& RayEnd, OUT TArray<FHitResult>& OutHits);

	// 对命中的 Pawn 施加伤害效果，同一 Pawn 只结算一次
	void ApplyDamageToPawnHits(const TArray<FHitResult>& Hits);

	// 为命中的每个 Destructible 生成一个破坏场，同一 Actor 只生成一次
	void SpawnSlashFields(const TArray<FHitResult>& Hits);

	// 在指定命中点生成破坏场，宽度按命中距离插值
	void SpawnSlashFieldAt(const FVector& ImpactPoint, float SlashDepth);

private:
	// 视角采样窗口，只保留 DirectionSampleWindow 之内的记录
	TArray<FSlashRotationSample> RotationSamples;

	// 上一帧的控制旋转，用于求增量
	FRotator LastControlRotation = FRotator::ZeroRotator;

	FTSTicker::FDelegateHandle RotationSampleTickerHandle;

	// 蓄力已持续的时长，由采样 ticker 累加，不依赖 World 时间
	float ChargeElapsedTime = 0.0f;

	// 本次斩击的几何基准：本地端由视角采样得出，权威端由复制的 TargetData 还原
	FSlashAbilityTargetData_Geometry SlashGeometry;

	// 本次激活是否已经结算过斩击，防止输入抖动导致重复结算
	bool bSlashReleased = false;
};

#undef UE_API
