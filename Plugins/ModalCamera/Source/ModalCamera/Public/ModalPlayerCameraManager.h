// Copyright Chronicler Software Corporation.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"

#include "ModalPlayerCameraManager.generated.h"

/**
 * 可扩展的玩家相机管理器。
 *
 * 负责与 ModalCamera/ModalCameraComponent 协同工作，
 * 通常在 `PlayerController`（C++ 或 Blueprint）中配置为默认 CameraManager。
 */
UCLASS(NotPlaceable)
class MODALCAMERA_API AModalPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	/** 构造玩家相机管理器。 */
	AModalPlayerCameraManager(const FObjectInitializer& ObjectInitializer);

protected:
	/** 输出相机相关调试信息。 */
	virtual void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;

};
