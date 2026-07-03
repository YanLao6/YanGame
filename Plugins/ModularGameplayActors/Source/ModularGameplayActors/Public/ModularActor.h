// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ModularActor.generated.h"

UCLASS()
class MODULARGAMEPLAYACTORS_API AModularActor : public AActor
{
	GENERATED_BODY()

public:
	/** 构造可被 GameFeature 扩展的基础 Actor。 */
	AModularActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	/** 组件预初始化阶段，通知扩展系统进入初始化流程。 */
	virtual void PreInitializeComponents() override;
	/** Actor 开始运行时通知扩展系统。 */
	virtual void BeginPlay() override;
	/** Actor 结束运行时执行扩展系统清理。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	/** 每帧 Tick（保留给扩展逻辑）。 */
	virtual void Tick(float DeltaTime) override;
};
