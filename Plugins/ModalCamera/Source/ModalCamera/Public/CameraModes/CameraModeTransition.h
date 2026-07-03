// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreTypes.h"
#include "UObject/ObjectPtr.h"

#include "CameraModeTransition.generated.h"

class UBlendCameraNode;
class UCameraAsset;
class UCameraMode;

/**
 * CameraMode 过渡（Transition）条件匹配参数。
 */
struct FCameraModeTransitionConditionMatchParams
{
	/** 前一个 CameraMode。 */
	const UCameraMode* FromCameraMode = nullptr;
	/** 前一个 CameraAsset。 */
	const UCameraAsset* FromCameraAsset = nullptr;

	/** 目标 CameraMode。 */
	const UCameraMode* ToCameraMode = nullptr;
	/** 目标 CameraAsset。 */
	const UCameraAsset* ToCameraAsset = nullptr;
};

/**
 * CameraMode Transition 条件基类。
 */
UCLASS(Abstract, DefaultToInstanced, MinimalAPI)
class UCameraModeTransitionCondition : public UObject
{
	GENERATED_BODY()

public:

	/** 判断本 Transition 是否应被采用。 */
	bool TransitionMatches(const FCameraModeTransitionConditionMatchParams& Params) const;

protected:

	/** 子类实现：是否匹配 Transition 条件。 */
	virtual bool OnTransitionMatches(const FCameraModeTransitionConditionMatchParams& Params) const { return false; }
};

/**
 * 单个 CameraMode Transition（条件 + Blend）。
 */
USTRUCT()
struct FCameraModeTransition
{
	GENERATED_BODY()

	/** 需全部通过才会使用该 Transition 的 Condition 列表。 */
	UPROPERTY(EditAnywhere, Instanced, Category=Common)
	TArray<TObjectPtr<UCameraModeTransitionCondition>> Conditions;

	/** 用于 Blend 进/出指定 CameraMode 的 BlendCameraNode。 */
	UPROPERTY(EditAnywhere, Instanced, Category=Common)
	TObjectPtr<UBlendCameraNode> Blend;
};
