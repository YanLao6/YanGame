// Fill out your copyright notice in the Description page of Project Settings.


#include "YanMoverComponent.h"

#include "YanChaosMoverBackend.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanMoverComponent)

UYanMoverComponent::UYanMoverComponent()
{
	// 后端决定仿真类，替换后端是接入项目仿真的唯一入口
	BackendClass = UYanChaosMoverBackendComponent::StaticClass();
}
