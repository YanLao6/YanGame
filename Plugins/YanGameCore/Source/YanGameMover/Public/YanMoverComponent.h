// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChaosMover/Character/ChaosCharacterMoverComponent.h"
#include "YanMoverComponent.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 项目角色 Mover 组件。
 *
 * 继承 UChaosCharacterMoverComponent，模拟运行在 Chaos 物理线程（异步）。
 *
 * 运动模式、转换、共享设置等 Mover 配置只允许在组件蓝图的 Class Defaults 上编辑；
 * 挂到 Pawn 上的组件模板与关卡实例一律只读。内联子对象若散落在各 Pawn 蓝图的组件模板上，
 * 子蓝图的继承覆写会与父模板共享同一批私有对象，保存时报 Illegal reference to private object。
 */
UCLASS(MinimalAPI, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UYanMoverComponent : public UChaosCharacterMoverComponent
{
	GENERATED_BODY()

public:
	UE_API UYanMoverComponent();
};

#undef UE_API
