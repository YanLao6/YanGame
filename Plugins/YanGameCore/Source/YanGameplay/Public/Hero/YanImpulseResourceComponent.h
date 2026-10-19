#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"

#include "YanImpulseResourceComponent.generated.h"

#define UE_API YANGAMEPLAY_API

class UAbilitySystemComponent;
class UMoverComponent;
struct FOnAttributeChangeData;

/**
 * 机动资源的时间演化：落地回本、贴墙慢慢攒、脱战掉负荷。
 *
 * 与 GameplayEffect 的分工是按「一次性」与「连续」划的：命中 +1 格、位移 −1 格、受击 −1 格
 * 这类瞬时增减由 GE 承载，随时间连续变化的部分归本组件。周期性 GE 也能做连续变化，
 * 但「双脚踩实地面才回能量、离地即停」这个条件在 GE 的标签体系里表达不出来——
 * 它取决于 Mover 的当前移动模式，只能逐帧去读。
 *
 * 只在服务器上推进。两项资源都是对抗结算的输入：能量决定位移放不放行，负荷决定击退推多远，
 * 客户端各算各的会在预测失配时给出不同的可用格数。
 *
 * 挂在 Pawn 而非 PlayerState，尽管属性本身在 PlayerState 的 ASC 上——恢复条件来自 Mover，
 * 而 Mover 在 Pawn 上。随 Pawn 一同重建也正合适：重生本就该从干净的计时开始。
 */
UCLASS(MinimalAPI, ClassGroup = (Yan), Meta = (BlueprintSpawnableComponent))
class UYanImpulseResourceComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	UE_API explicit UYanImpulseResourceComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UE_API virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~End UActorComponent Interface

protected:
	/** 双脚踩实地面时充满整条能量所需时长（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Impulse|Charge", Meta = (ClampMin = "0.01", ForceUnits = "s"))
	float GroundRefillSeconds = 0.4f;

	/** 贴墙攀爬时每恢复一格所需时长（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Impulse|Charge", Meta = (ClampMin = "0.01", ForceUnits = "s"))
	float WallClimbSecondsPerCharge = 0.6f;

	/**
	 * 攀爬所能恢复到的格数上限。
	 * 低于能量上限是刻意的：墙面给的是「撑住不死」的余量，补满仍须落地。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Impulse|Charge", Meta = (ClampMin = "0"))
	float WallClimbChargeCap = 2.f;

	/** 爬墙移动模式名，须与 UChaosWallClimbEnterCheck::TransitionToMode 保持一致 */
	UPROPERTY(EditDefaultsOnly, Category = "Impulse|Charge")
	FName WallClimbModeName = FName("WallClimb");

	/** 负荷停止增长后多久开始自然衰减（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Impulse|Load", Meta = (ClampMin = "0", ForceUnits = "s"))
	float LoadDecayDelaySeconds = 2.f;

	/** 负荷自然衰减速率（每秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Impulse|Load", Meta = (ClampMin = "0"))
	float LoadDecayPerSecond = 1.5f;

private:
	// 解析 PlayerState 上的 ASC，并在首次拿到时挂上负荷变化监听
	UE_API UAbilitySystemComponent* ResolveAndBindAbilitySystem();

	// 按当前移动模式推进能量恢复；只有攒满整格才写入属性，零头留在 PendingCharge
	UE_API void TickChargeRegen(UAbilitySystemComponent& ASC, const UMoverComponent& MoverComp, float DeltaSeconds);

	// 脱战计时到点后按速率削减负荷
	UE_API void TickLoadDecay(UAbilitySystemComponent& ASC, float DeltaSeconds);

	// 负荷上升时重置脱战计时；下降（衰减或出界清零）不重置，否则衰减会自我延期
	UE_API void HandleLoadChanged(const FOnAttributeChangeData& ChangeData);

	// 解绑监听，Pawn 销毁与组件卸载共用
	UE_API void UnbindAbilitySystem();

	/**
	 * 不足一格的恢复进度。离开恢复状态即清零：允许零头跨状态累积，
	 * 就等于「反复轻点墙面」也能攒出一格，那是本设计要避免的取巧。
	 */
	float PendingCharge = 0.f;

	// 距上次负荷上升的时长（秒）
	float TimeSinceLoadGain = 0.f;

	FDelegateHandle LoadChangedHandle;

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;
};

#undef UE_API
