// Copyright Chronicler.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "CameraAssistInterface.generated.h"

/** 相机穿透（Penetration）辅助接口：由 Controller / Actor 实现以配合 ThirdPerson / Fixed 等模式。 */
UINTERFACE(BlueprintType)
class UCameraAssistInterface : public UInterface
{
	GENERATED_BODY()
};

class ICameraAssistInterface
{
	GENERATED_BODY()

public:
	/**
	 * 返回允许相机 Trace 忽略的 Actor 列表（例如 ViewTarget 集合、本 Pawn、载具等），
	 * 用于 ThirdPerson 跟随时减少误挡。
	 */
	virtual void GetIgnoredActorsForCameraPenetration(TArray<const AActor*>& OutActorsAllowPenetration) const { }

	/**
	 * 作为防穿透（PreventPenetration）几何参考的 Actor。
	 * 未实现时通常视为 ViewTarget；若 ViewTarget 与需保持在画面内的根 Actor 不一致则需覆盖。
	 */
	virtual TOptional<AActor*> GetCameraPreventPenetrationTarget() const
	{
		return TOptional<AActor*>();
	}

	/** 当相机与 focal target 发生穿透时可调用（例如隐藏被挡目标）。 */
	virtual void OnCameraPenetratingTarget() { }
};
