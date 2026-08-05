#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilities/ModularGameplayAbility.h"
#include "YanKinesisAbility.generated.h"

#define UE_API YANGAMEMOVER_API

class APawn;
class APlayerController;
class UGameplayEffect;
class USoundBase;
class UAnimMontage;
class UYanKinesisTargetComponent;

/**
 * 一次推拉意图，全部字段由激活事件解码而来。
 *
 * bFromSession 区分两条来源：由会话激活时目标必须由会话锁定，锁不到即放弃本次施力；
 * 直接按键激活时无会话可锁，回落为视野锥内的群体推拉，保留单独调试的手段。
 */
struct FYanKinesisIntent
{
	/** 屏幕空间手势方向，零向量表示玩家未划动 */
	FVector2D AimScreenDirection = FVector2D::ZeroVector;

	/** 径向系数：推为 +1、拉为 -1、无前后意图为 0 */
	float RadialSign = 0.f;

	/** 会话提名的被控目标，须经服务器校验 */
	const AActor* NominatedTarget = nullptr;

	/** 目的地：非空时被控目标一律朝它飞去，手势与前后修饰不再参与 */
	const AActor* Destination = nullptr;

	/** 是否来自控制会话 */
	bool bFromSession = false;
};

/**
 * 念力推拉技能：对念力可控目标施加一次瞬时速度。
 *
 * 推与拉是同一套逻辑的两个方向，由 bIsPush 区分，通常做成两个蓝图子类分别绑定左右键。
 *
 * 本类是控制会话的执行层：推拉方向由两路意图合成，均由会话层注入，本类不自行采集。
 *  - 屏幕平面：玩家的鼠标手势，编码为事件的手势角
 *  - 前后：左右键的修饰标签，推为远离施法者、拉为靠近，两键并存或均无则不含前后分量
 * 直接激活（无会话）时无手势，按 bIsPush 走纯径向推拉。
 *
 * 目标一律取自 UYanKinesisRegistrySubsystem 的登记表，即挂有 UYanKinesisTargetComponent
 * 的 Actor。目标不必是 Pawn，也不必拥有碰撞体。
 *
 * 网络模型：
 *  目标玩家的 Mover 输入包只有其本地端能写，跨 Pawn 施力无法走输入包路径。
 *  故目标筛选与施力一律在服务器执行，经击飞 GE 落到目标 ASC，
 *  再由 UMoverLaunchExecutionCalculation 以 Authority sim action 下发到目标 Mover。
 *  本地端只播放预测的施法表现。
 *
 *  手势以屏幕空间二维方向传入，世界映射由服务器依据输入包中的 ControlRotation 完成：
 *  客户端只提供手势本身，无法左右施力的世界基准。
 *
 *  选取与校验两端口径不同，这是刻意的：客户端按屏幕像素选（与玩家所见的指示器一致），
 *  服务器没有客户端的视口尺寸与 FOV，无从复现同一次投影，只能以视野锥做宽松的下限校验。
 *  精确选取归客户端，服务器只拦截明显越界的提名。
 */
UCLASS(MinimalAPI)
class UYanKinesisAbility : public UModularGameplayAbility
{
	GENERATED_BODY()

public:
	UE_API UYanKinesisAbility(const FObjectInitializer& Initializer);

	//~Begin UGameplayAbility Interface
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	//~End UGameplayAbility Interface

	/**
	 * 将屏幕空间手势编码为可经 GameplayEvent::EventMagnitude 传递的角度，供会话层调用。
	 *
	 * 编码为自屏幕正右起逆时针的角度（度），取值 [0, 360)；
	 * 手势长度低于 MinMagnitude 时返回 -1，表示无手势。
	 * 解码由本类内部完成，两端须成对修改。
	 *
	 * @param AimScreenDirection 屏幕空间手势向量，X 为向右、Y 为向上，无需预先归一化
	 * @param MinMagnitude       判定为有效手势的最小长度，与传入向量同量纲
	 */
	UFUNCTION(BlueprintPure, Category = "Kinesis")
	static UE_API float EncodeAimScreenDirection(FVector2D AimScreenDirection, float MinMagnitude);

	/**
	 * 选取屏幕上最贴近准心的念力可控目标，供会话层在本地锁定被控目标。
	 *
	 * 判定在屏幕空间进行，与玩家看到的指示器出自同一次投影：准心压住哪个指示器就选中谁，
	 * 不因目标远近而改变手感。世界空间的视野锥做不到这点——同样的夹角在远处张成的屏幕范围要大得多。
	 *
	 * 瞄准取自本地实时视角而非输入包：准心是玩家所见，服务器侧的视角滞后一个 RTT。
	 * 选中结果仅为提名，最终仍须通过服务器的宽松校验。
	 *
	 * @param OwnerPawn          施法者，自身不参与选取
	 * @param MaxDistance        生效距离上限（cm），自视点起算
	 * @param ScreenRadiusRatio  可选中的屏幕半径，以视口高度为基准的比例（0.1 即视口高度的十分之一）
	 * @param IgnoredActor       排除在外的 Actor，选取目的地时用于跳过已被控制的目标
	 */
	UFUNCTION(BlueprintCallable, Category = "Kinesis")
	static UE_API AActor* FindKinesisTarget(APawn* OwnerPawn, float MaxDistance, float ScreenRadiusRatio, AActor* IgnoredActor = nullptr);

	/**
	 * 向目标转发控制起止，供会话层在锁定与松手时调用。
	 * 目标未挂 UYanKinesisTargetComponent 时静默忽略，调用方无须先行判定。
	 *
	 * @param bBegin true 为控制开始，false 为结束
	 */
	UFUNCTION(BlueprintCallable, Category = "Kinesis")
	static UE_API void NotifyKinesisControlState(AActor* Target, AActor* InInstigator, bool bBegin);

protected:
	/**
	 * 无会话直接激活时的推拉方向：true 推离施法者，false 拉向施法者。
	 * 由会话激活时前后方向一律取自事件修饰标签，本项不参与。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis")
	bool bIsPush = true;

	/** 生效距离上限（cm），自视点起算 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxDistance = 1000.f;

	/**
	 * 视野锥半角（度）：目标与视线夹角不超过此值才生效，总张角为其两倍。
	 * 用于服务器校验与无会话时的群体推拉；会话的本地选取走屏幕空间，不受本项约束。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", meta = (ClampMin = "0", ClampMax = "180"))
	float ConeHalfAngleDegrees = 70.f;

	/**
	 * 服务器校验提名目标时额外放宽的角度（度）。
	 * 两端选取口径本就不同，且服务器视角滞后一个 RTT，无余量会驳回玩家明明对准了的目标。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", meta = (ClampMin = "0", ClampMax = "90"))
	float ServerConeSlackDegrees = 15.f;

	/** 施加给目标的速度大小（cm/s） */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float Speed = 1200.f;

	/**
	 * 鼠标手势对推拉方向的偏转权重。
	 * 0 为纯径向；越大越偏向玩家划动鼠标的方向。无手势时该项自动为零。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", meta = (ClampMin = "0"))
	float AimIntentWeight = 0.5f;

	/** 单次生效的目标数上限，避免密集人群下的网络尖峰 */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis", meta = (ClampMin = "1"))
	int32 MaxTargets = 8;

	/** 施加到目标的 GameplayEffect（Instant，须使用 UMoverLaunchExecutionCalculation） */
	UPROPERTY(EditDefaultsOnly, Category = "Kinesis")
	TSubclassOf<UGameplayEffect> KinesisGameplayEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Kinesis|Feedback")
	TObjectPtr<USoundBase> CastSound;

	UPROPERTY(EditDefaultsOnly, Category = "Kinesis|Feedback")
	TObjectPtr<UAnimMontage> CastMontage;

private:
	// 目标是否落在视野锥内；OutAlignment 为与视线的贴合度，越大越接近准心中心
	static UE_API bool EvaluateConeFit(const FVector& ViewLocation, const FVector& ViewForward, const FVector& TargetLocation, float MaxDistance, float CosHalfAngle, float& OutAlignment);

	// 收集视野锥内的登记目标，按 MaxTargets 截断
	UE_API void GatherTargetsInCone(APawn* OwnerPawn, const FVector& ViewLocation, const FVector& ViewForward, TArray<AActor*>& OutTargets) const;

	// 校验会话提名的目标：通过则独占本次施力，未通过则回落群体锥形
	UE_API bool AcceptNominatedTarget(const AActor* NominatedTarget, APawn* OwnerPawn, const FVector& ViewLocation, const FVector& ViewForward, TArray<AActor*>& OutTargets) const;

	// 从激活事件解出本次推拉的全部意图
	UE_API FYanKinesisIntent DecodeIntent(const FGameplayEventData* TriggerEventData) const;

	// EncodeAimScreenDirection 的逆向：无触发事件或角度为负时返回零向量
	static UE_API FVector2D DecodeAimScreenDirection(const FGameplayEventData* TriggerEventData);

	// 从事件修饰标签解出径向系数：推为 +1、拉为 -1，两者并存或均无则为 0
	UE_API float DecodeRadialSign(const FGameplayEventData* TriggerEventData) const;

	// 屏幕空间手势映射到世界：横向取视线右向量、纵向取视线上向量，零向量原样返回
	static UE_API FVector ResolveAimIntentDirection(const FRotator& AimRotation, const FVector2D& AimScreenDirection);

	// 径向分量（按 RadialSign 定向）与鼠标手势按权重混合后乘以 Speed
	UE_API FVector ComputeVelocityForTarget(const FVector& TargetLocation, const FVector& ViewLocation, const FVector& AimIntentDirection, float RadialSign) const;

	// 朝目的地的速度，方向按服务器侧的实时位置求得，不采信客户端算好的方向
	UE_API FVector ResolveVelocityTowardDestination(const FVector& TargetLocation, const AActor* Destination) const;

	// 服务器权威的施力入口：筛选目标并逐个施加 GE
	UE_API void ApplyKinesisToTargets(const FYanKinesisIntent& Intent);

	UE_API void PlayLocalFeedback() const;
};

#undef UE_API
