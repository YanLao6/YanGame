// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModalCamera/Public/CameraModes/CameraModeTransition.h"
#include "CoreTypes.h"
#include "UObject/ObjectPtr.h"

#include "CameraMode.generated.h"

class UCameraNode;

/**
 * CameraMode 资源。
 *
 * 通过一棵 CameraNode 层级驱动相机行为与过渡（Transition）。
 */
UCLASS(MinimalAPI)
class UCameraMode : public UObject
{
	GENERATED_BODY()

public:

	/** 根 CameraNode。 */
	UPROPERTY(EditAnywhere, Instanced, Category=Common)
	TObjectPtr<UCameraNode> RootNode;

	/** 进入本 CameraMode 时使用的 Transition 列表。 */
	UPROPERTY(EditAnywhere, Category=Blending)
	TArray<FCameraModeTransition> EnterTransitions;

	/** 离开本 CameraMode 时使用的 Transition 列表。 */
	UPROPERTY(EditAnywhere, Category=Blending)
	TArray<FCameraModeTransition> ExitTransitions;
};
