// Copyright Chronicler.

#pragma once

#include "DrawDebugHelpers.h"
#include "../ModalCameraMode.h"
#include "Curves/CurveFloat.h"

#include "CameraMode_Fixed.generated.h"

class UCurveVector;

/**
 * UCameraMode_Fixed
 *
 * 基础 Fixed CameraMode：使用固定 Location / Rotation，可选 Penetration 修正。
 */
UCLASS(Abstract, Blueprintable)
class UCameraMode_Fixed : public UModalCameraMode
{
	GENERATED_BODY()

public:

	/** 构造 Fixed 模式默认值。 */
	UCameraMode_Fixed();

protected:

	virtual void UpdateView(float DeltaTime) override;

	// 更新 Penetration：SafeLocation、Sweep、Assist 回调
	void UpdatePreventPenetration(float DeltaTime);
	// SafeLoc 与 CameraLoc 之间做 Sphere Sweep，输出 DistBlockedPct 并可能拉回 CameraLoc
	void PreventCameraPenetration(class AActor const& ViewTarget, FVector const& SafeLoc, FVector& CameraLoc, float const& DeltaTime, float& DistBlockedPct, bool bSingleRayOnly);

	virtual void DrawDebug(UCanvas* Canvas) const override;

	virtual FVector GetPivotLocation() const override;

	virtual FRotator GetPivotRotation() const override;

	virtual void OnActivation() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Fixed")
	FVector FixedLocation;

	UPROPERTY(EditDefaultsOnly, Category = "Fixed")
	FRotator FixedRotation;

	/** 为 true 时对 World 做碰撞检测，避免相机穿入几何。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision")
	bool bPreventPenetration = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	float CollisionPushOutDistance = 2.f;

	/** 当因穿透把相机距离压到理想距离的该比例以下时，触发 Report（与 Assist 回调相关）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	float ReportPenetrationPercent = 0.f;

	UPROPERTY(Transient)
	float AimLineToDesiredPosBlockedPct;

	UPROPERTY(Transient)
	TArray<TObjectPtr<const AActor>> DebugActorsHitDuringCameraPenetration;

#if ENABLE_DRAW_DEBUG
	mutable float LastDrawDebugTime = -MAX_FLT;
#endif

};
