// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Character.h"

#include "ModularCharacter.generated.h"

#define UE_API MODULARGAMEPLAYACTORS_API

class UObject;

/**
 * 可被 GameFeature 插件扩展的最小 Character 基类。
 */
UCLASS(MinimalAPI, Blueprintable)
class AModularCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	//~ Begin AActor Interface
	/** 组件预初始化阶段，触发扩展系统初始化。 */
	UE_API virtual void PreInitializeComponents() override;
	/** 角色开始运行时触发扩展系统进入 Active 状态。 */
	UE_API virtual void BeginPlay() override;
	/** 角色结束运行时触发扩展系统清理。 */
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface
};

#undef UE_API
