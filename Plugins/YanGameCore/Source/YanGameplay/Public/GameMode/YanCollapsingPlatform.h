#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "YanCollapsingPlatform.generated.h"

#define UE_API YANGAMEPLAY_API

class UStaticMeshComponent;
class UYanDuelRoundComponent;

/**
 * 收缩环的一块平台：回合开始后按环序号先后塌陷，由外向内。
 *
 * 塌陷的目的是强制交战——场地越收越小，龟缩不再是一种可行打法；末段只剩中心一小块时
 * 钩锁失去可用锚点，长程机动流被迫近身。所以序号越靠外的越早塌，中心那块永不塌陷。
 *
 * 计时基准取回合而非关卡开始：回合可能因出界提前结束，按关卡时间自行计时会与实际回合
 * 越错越远。赛制组件在每个回合起始处广播，本类据此复位并重排自己的塌陷时刻。
 *
 * 只在服务器推进，塌陷与预警两个状态复制到客户端；表现层（下沉、变色、碎裂）交给蓝图
 * 子类的三个事件，本类只管状态与碰撞。
 */
UCLASS(MinimalAPI, Blueprintable)
class AYanCollapsingPlatform : public AActor
{
	GENERATED_BODY()

public:
	UE_API explicit AYanCollapsingPlatform(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin AActor Interface
	UE_API virtual void PostInitializeComponents() override;
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UE_API virtual void Tick(float DeltaSeconds) override;
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

	/** 本块是否已塌陷。 */
	UFUNCTION(BlueprintPure, Category = "Platform")
	bool IsCollapsed() const { return bCollapsed; }

	/** 本块是否处于塌陷前的预警期。 */
	UFUNCTION(BlueprintPure, Category = "Platform")
	bool IsWarning() const { return bWarning; }

protected:
	/** 平台本体，作为根组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Platform")
	TObjectPtr<UStaticMeshComponent> PlatformMesh;

	/**
	 * 环序号：0 为最外圈，数值越大越靠内。塌陷时刻为 (RingIndex + 1) × CollapseIntervalSeconds。
	 * 取负值表示永不塌陷，中心那块用它。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Platform")
	int32 RingIndex = 0;

	/** 相邻两圈塌陷的间隔（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Platform", meta = (ClampMin = "1", ForceUnits = "s"))
	float CollapseIntervalSeconds = 30.f;

	/** 塌陷前的预警时长（秒）。没有预警，站在外圈的玩家就只能靠背时间表活命 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Platform", meta = (ClampMin = "0", ForceUnits = "s"))
	float WarningSeconds = 3.f;

	/** 塌陷时下坠的距离（cm）。坠完即隐藏，故只需坠到看不见即可 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Platform|Visual", meta = (ClampMin = "0", ForceUnits = "cm"))
	float CollapseFallDistance = 900.f;

	/** 下坠动画时长（秒）。碰撞在塌陷当帧就没了，这段时间纯粹是给玩家看的 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Platform|Visual", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float CollapseFallSeconds = 0.8f;

	/** 预警闪烁频率（Hz） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Platform|Visual", meta = (ClampMin = "0.1"))
	float WarnPulseHz = 3.f;

	/** 材质中预警混合量的标量参数名，取值 0 为常态色、1 为警告色 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Platform|Visual")
	FName WarnAmountParameterName = FName("WarnAmount");

	/** 蓝图实现：进入预警期的表现，如变色或轻微抖动 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Platform")
	UE_API void OnCollapseWarning();

	/** 蓝图实现：塌陷瞬间的表现 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Platform")
	UE_API void OnCollapsed();

	/** 蓝图实现：回合复位、平台重新出现时的表现 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Platform")
	UE_API void OnRestored();

private:
	/**
	 * 尝试绑定赛制组件；未就绪时按 BindRetryInterval 重试。
	 *
	 * 组件由 Experience 激活时经 GameFeatureAction 注入 GameState，晚于关卡内 Actor 的
	 * BeginPlay，故这里无法一次绑定成功。绑上的那一刻按一次回合开始处理——首个回合此时
	 * 通常刚开始不久，几百毫秒的偏差对 30 秒的间隔无关紧要。
	 */
	UE_API void TryBindRoundComponent();

	UFUNCTION()
	UE_API void HandleRoundStarted();

	// 按环序号排定本块的预警与塌陷时刻
	UE_API void ScheduleCollapse();

	UE_API void EnterWarning();
	UE_API void Collapse();
	UE_API void Restore();

	/**
	 * 把复制来的状态落到碰撞、可见性与表现事件上，服务器与客户端共用。
	 *
	 * @param bImmediate 跳过下坠动画直接落到终态。中途加入的客户端首次收到状态时用，
	 *                   否则一进场就会看到早已塌掉的平台重演一遍坠落
	 */
	UE_API void ApplyCollapsedState(bool bImmediate = false);

	// 仅在预警或下坠期间开启 Tick，静止的平台不该每帧唤醒
	UE_API void UpdateTickEnabled();

	// 把预警混合量写进材质；材质无此参数时静默忽略
	UE_API void PushWarnAmount(float Amount) const;

	UFUNCTION()
	UE_API void OnRep_Collapsed();

	UFUNCTION()
	UE_API void OnRep_Warning();

	UPROPERTY(ReplicatedUsing = OnRep_Collapsed)
	bool bCollapsed = false;

	UPROPERTY(ReplicatedUsing = OnRep_Warning)
	bool bWarning = false;

	TWeakObjectPtr<UYanDuelRoundComponent> BoundRoundComponent;

	// 原位，BeginPlay 时记下；下坠改的是 Actor 位置，复位时据此还原
	FVector RestLocation = FVector::ZeroVector;

	// 下坠动画已进行的时长（秒），达到 CollapseFallSeconds 即隐藏并停止
	float FallElapsedSeconds = 0.f;

	bool bFalling = false;

	// 首次落状态一律瞬时完成，之后才播动画
	bool bVisualsInitialized = false;

	FTimerHandle BindRetryTimer;
	FTimerHandle WarningTimer;
	FTimerHandle CollapseTimer;
};

#undef UE_API
