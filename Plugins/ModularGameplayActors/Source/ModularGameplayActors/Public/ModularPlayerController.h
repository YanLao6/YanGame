// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/PlayerController.h"

#include "ModularPlayerController.generated.h"

#define UE_API MODULARGAMEPLAYACTORS_API

class UObject;

/**
 * 可被 GameFeature 插件扩展的最小 PlayerController 基类。
 */
UCLASS(MinimalAPI, Blueprintable)
class AModularPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	//~ Begin AActor interface
	/** 组件预初始化阶段，触发扩展系统初始化。 */
	UE_API virtual void PreInitializeComponents() override;
	/** 结束运行时触发扩展系统清理。 */
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor interface

	//~ Begin APlayerController interface
	/** 客户端玩家就绪回调，可用于扩展初始化。 */
	UE_API virtual void ReceivedPlayer() override;
	/** 每帧玩家控制器更新。 */
	UE_API virtual void PlayerTick(float DeltaTime) override;
	//~ End APlayerController interface
};

#undef UE_API
