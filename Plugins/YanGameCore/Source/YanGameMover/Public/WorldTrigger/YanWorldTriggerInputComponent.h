#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MoverSimulationTypes.h"

#include "YanWorldTriggerInputComponent.generated.h"

#define UE_API YANGAMEMOVER_API

class APawn;
class UMoverComponent;

/**
 * 把世界触发器（发射板、传送门）的请求写进 Mover 输入包，使其走本地预测。
 *
 * 触发器的重叠回调在 game thread，直接向 Mover 入队效果不可回滚，预测结果会被服务器抹掉。
 * 本组件作为输入源登记到 MoverComponent：请求只置位，在 ProduceInput 时写入 FYanCharacterInputs，
 * 由 sim 侧的 UChaosCharacterLaunchPadCheck / UChaosCharacterTeleporterCheck 以可回滚方式施加。
 *
 * 只对本地受控的 Pawn 有意义：输入包只有本地端能写。远端玩家踩板由其自己的输入包送达，
 * 服务器上不需要、也不应替它施力。
 *
 * 按需挂到 Pawn 上（FindOrAdd），无须改动 Pawn 类；同一帧多次请求只保留最后一次。
 *
 * 每次请求附带自增序号：输入包按 game 帧生产，game 线程卡顿时会被连续多个物理步复用，
 * sim 侧凭序号识别「同一份输入包再次被读」与「新请求」，脉冲因而只施加一次。
 */
UCLASS(MinimalAPI, ClassGroup = (Mover))
class UYanWorldTriggerInputComponent : public UActorComponent, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	/** 取 Pawn 上的本组件，没有则创建并登记到其 MoverComponent；Pawn 无 Mover 时返回 null */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|WorldTrigger")
	static UE_API UYanWorldTriggerInputComponent* FindOrAdd(APawn* Pawn);

	/** 请求在下一次输入采样时叠加一段速度（世界空间，cm/s） */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|WorldTrigger")
	UE_API void RequestLaunch(FVector AdditiveVelocity);

	/**
	 * 请求在下一次输入采样时传送到指定位姿。
	 * @param bOverrideVelocity true 则传送后把速度覆盖为 ExitVelocity，false 保留按旋转差转向后的入口速度
	 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Mover|WorldTrigger")
	UE_API void RequestTeleport(FVector Location, FRotator Rotation, bool bOverrideVelocity, FVector ExitVelocity);

	//~Begin UActorComponent Interface
	UE_API virtual void OnRegister() override;
	UE_API virtual void OnUnregister() override;
	//~End UActorComponent Interface

	//~Begin IMoverInputProducerInterface
	UE_API virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;
	//~End IMoverInputProducerInterface

private:
	// 取下一个请求序号；0 保留给「尚未有过请求」，回绕时跳过
	static uint8 NextRequestId(uint8& Serial);

	// 待投递的请求；投递后即清空，脉冲只出现在一帧输入包里
	bool     bPendingLaunch = false;
	uint8    LaunchRequestSerial = 0;
	FVector  PendingLaunchVelocity = FVector::ZeroVector;
	bool     bPendingTeleport = false;
	uint8    TeleportRequestSerial = 0;
	FVector  PendingTeleportLocation = FVector::ZeroVector;
	FRotator PendingTeleportRotation = FRotator::ZeroRotator;
	bool     bPendingTeleportOverridesVelocity = false;
	FVector  PendingTeleportExitVelocity = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UMoverComponent> CachedMoverComp;
};

#undef UE_API
