// Copyright Chronicler.

#pragma once

#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "ModalCamera/Public/CameraModes/CameraMode.h"

#include "ModalCameraMode.generated.h"

#define CAMERA_DEFAULT_FOV			(80.0f)
#define CAMERA_DEFAULT_PITCH_MIN	(-89.0f)
#define CAMERA_DEFAULT_PITCH_MAX	(89.0f)

class UModalCameraComponent;
/**
 * CameraMode 之间 Blend 时使用的曲线类型（BlendFunction）。
 */
UENUM(BlueprintType)
enum class ECameraModeBlendFunction : uint8
{
	// Linear 线性插值
	Linear,

	// EaseIn：快速加速、末端平滑减速；形状由 BlendExponent 控制
	EaseIn,

	// EaseOut：平滑加速、接近目标时不减速；形状由 BlendExponent 控制
	EaseOut,

	// EaseInOut：先加速后减速；形状由 BlendExponent 控制
	EaseInOut,

	COUNT	UMETA(Hidden)
};


/**
 * CameraMode 输出的视图数据（View）。
 *
 * 用于多个 CameraMode 之间的 Blend 计算。
 */
struct FCameraModeView
{
public:
	/** 构造默认视图数据。 */
	FCameraModeView();

	/** 将 Other 按权重 OtherWeight Blend 到当前视图。 */
	void Blend(const FCameraModeView& Other, float OtherWeight);

public:

	FVector Location;
	FRotator Rotation;
	FRotator ControlRotation;
	float FieldOfView;
};


/**
 * ModalCamera 模式基类。
 *
 * 封装单个 CameraMode 的视图更新、Transition Blend 与 Debug 绘制。
 */
UCLASS(Abstract, NotBlueprintable)
class MODALCAMERA_API UModalCameraMode : public UCameraMode
{
	GENERATED_BODY()

public:
	/** 构造相机模式并初始化默认参数。 */
	UModalCameraMode();

	/** 返回拥有该模式的 ModalCameraComponent。 */
	UModalCameraComponent* GetCameraComponent() const;

	/** 返回当前模式所在的 World（CDO 返回 nullptr）。 */
	virtual UWorld* GetWorld() const override;

	/** 返回当前模式所 follow 的 TargetActor。 */
	AActor* GetTargetActor() const;

	/** 返回当前模式产出的 FCameraModeView（只读）。 */
	const FCameraModeView& GetCameraModeView() const { return View; }

	/** 压入 CameraModeStack 并激活时调用；可在子类中响应。 */
	virtual void OnActivation() {};

	/** 从 CameraModeStack 移除并停用时调用；可在子类中响应。 */
	virtual void OnDeactivation() {};

	/** 更新当前 CameraMode（View + Blend 权重）。 */
	void UpdateCameraMode(float DeltaTime);

	/** 返回 Blend 时长（秒）。 */
	float GetBlendTime() const { return BlendTime; }
	/** 返回当前 Blend 权重 [0,1]。 */
	float GetBlendWeight() const { return BlendWeight; }
	/** 直接设置 Blend 权重并反推 BlendAlpha（与 BlendFunction 一致）。 */
	void SetBlendWeight(float Weight);

	/** 供 Gameplay 查询的 CameraType（GameplayTag），无需依赖具体子类。 */
	FGameplayTag GetCameraTypeTag() const
	{
		return CameraTypeTag;
	}

	/** 在 Canvas 上绘制本模式的 Debug 信息。 */
	virtual void DrawDebug(UCanvas* Canvas) const;

protected:

	/** 计算 pivot 位置（随 Target 类型可能含 Character 高度修正）。 */
	virtual FVector GetPivotLocation() const;
	/** 计算 pivot 旋转。 */
	virtual FRotator GetPivotRotation() const;

	/** 更新本模式输出的 View（Location/Rotation/FOV 等）。 */
	virtual void UpdateView(float DeltaTime);
	/** 根据 DeltaTime 推进 BlendAlpha 并更新 BlendWeight。 */
	virtual void UpdateBlending(float DeltaTime);

protected:
	/** Gameplay 可通过 Tag 判断某类 CameraMode 是否激活，而无需硬编码类名。 */
	UPROPERTY(EditDefaultsOnly, Category = "Blending")
	FGameplayTag CameraTypeTag;

	/** 本模式计算得到的 View 输出。 */
	FCameraModeView View;

	/** 水平 FOV（度）。 */
	UPROPERTY(EditDefaultsOnly, Category = "View", Meta = (UIMin = "5.0", UIMax = "170", ClampMin = "5.0", ClampMax = "170.0"))
	float FieldOfView;

	/** View Pitch 下限（度）。 */
	UPROPERTY(EditDefaultsOnly, Category = "View", Meta = (UIMin = "-89.9", UIMax = "89.9", ClampMin = "-89.9", ClampMax = "89.9"))
	float ViewPitchMin;

	/** View Pitch 上限（度）。 */
	UPROPERTY(EditDefaultsOnly, Category = "View", Meta = (UIMin = "-89.9", UIMax = "89.9", ClampMin = "-89.9", ClampMax = "89.9"))
	float ViewPitchMax;

	/** Blend 进入本模式所需时间（秒）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Blending")
	float BlendTime;

	/** Blend 曲线类型。 */
	UPROPERTY(EditDefaultsOnly, Category = "Blending")
	ECameraModeBlendFunction BlendFunction;

	/** BlendFunction 使用的指数，控制曲线形状。 */
	UPROPERTY(EditDefaultsOnly, Category = "Blending")
	float BlendExponent;

	/** 线性 Blend alpha，用于推导 BlendWeight。 */
	float BlendAlpha;

	/** 由 BlendAlpha 与 BlendFunction 计算得到的权重。 */
	float BlendWeight;

protected:
	/** 若为 true，本帧跳过插值并将相机置于理想位置；下一帧自动清零。 */
	UPROPERTY(transient)
	uint32 bResetInterpolation:1;
};


/**
 * CameraMode 混合栈（Stack）。
 *
 * 维护激活顺序并输出最终 Blend 后的相机 View。
 */
UCLASS()
class UCameraModeStack : public UObject
{
	GENERATED_BODY()

public:
	/** 构造模式栈。 */
	UCameraModeStack();

	/** 激活栈：允许 EvaluateStack 更新与混合。 */
	void ActivateStack();
	/** 停用栈：停止更新并通知各模式 OnDeactivation。 */
	void DeactivateStack();

	/** 栈当前是否处于激活状态。 */
	bool IsStackActivate() const { return bIsActive; }

	/** 将指定 CameraMode 压栈顶并参与 Blend。 */
	void PushCameraMode(TSubclassOf<UModalCameraMode> CameraModeClass);

	/** 评估整帧栈并写入 OutCameraModeView。 */
	bool EvaluateStack(float DeltaTime, FCameraModeView& OutCameraModeView);

	/** 绘制栈内各模式的 Debug。 */
	void DrawDebug(UCanvas* Canvas) const;

	/** 取得栈底（最终层）条目的 Blend 权重与 CameraTypeTag。 */
	void GetBlendInfo(float& OutWeightOfTopLayer, FGameplayTag& OutTagOfTopLayer) const;

protected:

	// 按 Class 复用或新建 UModalCameraMode 实例
	UModalCameraMode* GetCameraModeInstance(TSubclassOf<UModalCameraMode> CameraModeClass);

	// 逐层 UpdateCameraMode，并移除已被完全覆盖的下层
	void UpdateStack(float DeltaTime);
	// 自底向上 Blend 各层 View
	void BlendStack(FCameraModeView& OutCameraModeView) const;

protected:

	bool bIsActive;

	UPROPERTY()
	TArray<TObjectPtr<UModalCameraMode>> CameraModeInstances;

	UPROPERTY()
	TArray<TObjectPtr<UModalCameraMode>> CameraModeStack;
};
