#pragma once

#include "Components/ControllerComponent.h"

#include "HeroSwitchComponent.generated.h"

#define UE_API HEROASSIGNMENTSYSTEM_API

/**
 * 对局内换人组件。
 *
 * 由对战 Experience 注入到 PlayerController，向拥有者客户端暴露换人入口。
 * 请求经 Server RPC 上报，合法性校验与实际换装统一由 GameState 上的
 * UHeroAssignmentComponent 完成，使玩家主动换人与逻辑驱动换人走同一条路径。
 */
UCLASS(MinimalAPI, Blueprintable, Meta = (BlueprintSpawnableComponent))
class UHeroSwitchComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	UE_API explicit UHeroSwitchComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 请求更换角色，由拥有者客户端调用；服务器校验后生效。 */
	UFUNCTION(BlueprintCallable, Category = "Hero")
	UE_API void RequestChangeHero(FPrimaryAssetId HeroId);

private:
	UFUNCTION(Server, Reliable)
	UE_API void ServerChangeHero(FPrimaryAssetId HeroId);
};

#undef UE_API
