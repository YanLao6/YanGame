// Copyright Chronicler.

#pragma once

#include "../CameraPenetrationAvoidanceFeeler.h"
#include "DrawDebugHelpers.h"
#include "..\ModalCameraMode.h"
#include "Curves/CurveFloat.h"
#include "CameraMode_ThirdPerson.generated.h"

class UCurveVector;

/**
 * UCameraMode_ThirdPerson
 *
 * 基础 ThirdPerson CameraMode：Pivot + Pitch 驱动 TargetOffset，可选 Penetration 与 Crouch 偏移 Blend。
 */
UCLASS(Abstract, Blueprintable)
class UCameraMode_ThirdPerson : public UModalCameraMode
{
	GENERATED_BODY()

public:

	/** 构造 ThirdPerson 模式并初始化默认 PenetrationAvoidanceFeelers。 */
	UCameraMode_ThirdPerson();

protected:

	virtual void UpdateView(float DeltaTime) override;

	// 根据 Target（如 Character Crouch）更新目标 Crouch 偏移
	void UpdateForTarget(float DeltaTime);
	void UpdatePreventPenetration(float DeltaTime);
	void PreventCameraPenetration(class AActor const& ViewTarget, FVector const& SafeLoc, FVector& CameraLoc, float const& DeltaTime, float& DistBlockedPct, bool bSingleRayOnly);

	virtual void DrawDebug(UCanvas* Canvas) const override;

protected:

	// 以 View Pitch 采样 UCurveVector，得到 Target 局部空间偏移（与 RuntimeFloatCurves 二选一）
	UPROPERTY(EditDefaultsOnly, Category = "Third Person", Meta = (EditCondition = "!bUseRuntimeFloatCurves"))
	TObjectPtr<const UCurveVector> TargetOffsetCurve;

	// UE-103986：PIE 下 RuntimeFloatCurves 实时编辑与资源曲线行为不一致；修复后可能改为默认方案
	UPROPERTY(EditDefaultsOnly, Category = "Third Person")
	bool bUseRuntimeFloatCurves;

	UPROPERTY(EditDefaultsOnly, Category = "Third Person", Meta = (EditCondition = "bUseRuntimeFloatCurves"))
	FRuntimeFloatCurve TargetOffsetX;

	UPROPERTY(EditDefaultsOnly, Category = "Third Person", Meta = (EditCondition = "bUseRuntimeFloatCurves"))
	FRuntimeFloatCurve TargetOffsetY;

	UPROPERTY(EditDefaultsOnly, Category = "Third Person", Meta = (EditCondition = "bUseRuntimeFloatCurves"))
	FRuntimeFloatCurve TargetOffsetZ;

	// Crouch 目标高度偏移的 Blend 速度倍率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Third Person")
	float CrouchOffsetBlendMultiplier = 5.0f;

	// Penetration 相关（部分属性需对蓝图可见）
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision")
	float PenetrationBlendInTime = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision")
	float PenetrationBlendOutTime = 0.15f;

	/** 为 true 时做碰撞，防止相机穿入 World。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision")
	bool bPreventPenetration = true;

	/** 为 true 时用额外 Feeler 预测转角后的挡墙，减轻相机弹出（Popping）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision")
	bool bDoPredictiveAvoidance = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	float CollisionPushOutDistance = 2.f;

	/** 穿透导致相机距离低于理想距离该比例时，用于 Report / Assist。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	float ReportPenetrationPercent = 0.f;

	/**
	 * Penetration 探针数组（Feeler）。
	 * Index 0：主射线，负责基础防穿。
	 * Index 1+：bDoPredictiveAvoidance 时用于扫描潜在遮挡，提前收相机。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Collision")
	TArray<FCameraPenetrationAvoidanceFeeler> PenetrationAvoidanceFeelers;

	UPROPERTY(Transient)
	float AimLineToDesiredPosBlockedPct;

	UPROPERTY(Transient)
	TArray<TObjectPtr<const AActor>> DebugActorsHitDuringCameraPenetration;

#if ENABLE_DRAW_DEBUG
	mutable float LastDrawDebugTime = -MAX_FLT;
#endif

protected:
	
	// 开始新一帧 Crouch 偏移 Blend
	void SetTargetCrouchOffset(const FVector& NewTargetOffset);
	void UpdateCrouchOffset(float DeltaTime);

	FVector InitialCrouchOffset = FVector::ZeroVector;
	FVector TargetCrouchOffset = FVector::ZeroVector;
	float CrouchOffsetBlendPct = 1.0f;
	FVector CurrentCrouchOffset = FVector::ZeroVector;
	
};
