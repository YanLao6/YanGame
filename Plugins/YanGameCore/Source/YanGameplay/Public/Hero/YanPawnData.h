// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataAsset/ModularPawnData.h"
#include "YanPawnData.generated.h"

#define UE_API YANGAMEPLAY_API

/**
 * 项目层 PawnData。
 *
 * 技能、输入、组件与界面等能力一律通过基类的 Fragments 配置，本类不再新增字段；
 * 保留该类型是为了让项目侧英雄资产拥有稳定的资产类型入口。
 */
UCLASS(MinimalAPI)
class UYanPawnData : public UModularPawnData
{
	GENERATED_BODY()

public:
	UE_API explicit UYanPawnData(const FObjectInitializer& ObjectInitializer);
};

#undef UE_API
