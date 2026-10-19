#pragma once

#include "ChaosMover/Backends/ChaosMoverBackend.h"

#include "YanChaosMoverBackend.generated.h"

#define UE_API YANGAMEMOVER_API

/**
 * 项目 Mover 后端。
 *
 * 仿真类由后端在构造期决定且不对外暴露，故替换仿真实现只能经由后端子类。
 */
UCLASS(MinimalAPI, Within = MoverComponent)
class UYanChaosMoverBackendComponent : public UChaosMoverBackendComponent
{
	GENERATED_BODY()

public:
	UE_API UYanChaosMoverBackendComponent();
};

#undef UE_API
