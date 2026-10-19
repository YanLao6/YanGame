#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "MoverGameplayLibrary.generated.h"

#define UE_API YANGAMEMOVER_API

class AYanHookProjectile;
class UAbilitySystemComponent;
class UGameplayEffect;
class UMoverComponent;

/** Mover 玩法对象创建与 GAS 效果的连接接口。 */
UCLASS(MinimalAPI)
class UMoverGameplayLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 在指定世界位置和朝向 Spawn AYanHookProjectile，由 MoverComp 提供世界上下文。
	 * 失败时（MoverComp 无效、ProjectileClass 为空、World 无效）返回 nullptr。
	 * 使用 AlwaysSpawn 策略，Owner 设为 MoverComp 的拥有者。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Grappling")
	static UE_API AYanHookProjectile* SpawnHookProjectile(UMoverComponent* MoverComp, TSubclassOf<AYanHookProjectile> ProjectileClass, FVector SpawnLocation, FRotator SpawnRotation);

	/**
	 * 解除被抓状态：移除目标身上所有授予 Status.Grabbed 的效果，恢复其余技能的激活权。
	 *
	 * 只解 GAS 那一层。移动上的钉制由 FYanLayeredMove_Clinch 承载，须由挣脱者
	 * 在自身输入包写入 bWantsToBreakFree 当帧解除——两层的解除路径本就不同，
	 * 前者要服务器权威，后者要本地即时响应。
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Clinch")
	static UE_API void ReleaseGrabbedState(AActor* TargetActor);

	/**
	 * 通过 GAS GameplayEffect 对目标施加击飞效果，由发起者 ASC 向目标 ASC 应用。
	 * 内部使用 SetByCaller 将速度分量编码进 GE Spec，由 ExecCalc 解码后落到目标 Mover。
	 * 持续时长、移动模式等施加方式配在 UMoverLaunchGameplayEffect 上，不经此处传入。
	 *
	 * @param InstigatorASC     发起者 AbilitySystemComponent（客户端调用；GAS 自动转服务器执行）
	 * @param TargetASC         目标 AbilitySystemComponent
	 * @param LaunchEffectClass 击飞效果类，直接用 UMoverLaunchGameplayEffect 或其蓝图子类；留空即不施加
	 * @param LaunchVelocity    世界空间击飞速度（cm/s）
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|Launch")
	static UE_API void ApplyLaunchEffectToTarget(UAbilitySystemComponent* InstigatorASC, UAbilitySystemComponent* TargetASC, TSubclassOf<UGameplayEffect> LaunchEffectClass, FVector LaunchVelocity);
};

#undef UE_API
