// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InputModifiers.h"

#include "YanInputModifiers.generated.h"

#define UE_API YANGAMESETTING_API

struct FInputActionValue;

class FProperty;
class UEnhancedPlayerInput;
class UYanAimSensitivityData;

/**
 * 按共享设置中的 double 属性缩放输入值。
 * 属性名在 InputAction 的 Modifier 实例上指定，运行时经反射从 UYanSettingsShared 读取，
 * 因而灵敏度改动无需任何通知即可在下一帧生效。
 */
UCLASS(NotBlueprintable, MinimalAPI, meta = (DisplayName = "Setting Based Scalar"))
class UYanSettingBasedScalar : public UInputModifier
{
	GENERATED_BODY()

public:
	/** 用于缩放 X 轴的设置属性名 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = Settings)
	FName XAxisScalarSettingName = NAME_None;

	/** 用于缩放 Y 轴的设置属性名 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = Settings)
	FName YAxisScalarSettingName = NAME_None;

	/** 用于缩放 Z 轴的设置属性名 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = Settings)
	FName ZAxisScalarSettingName = NAME_None;

	/** 各轴缩放值上限 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = Settings)
	FVector MaxValueClamp = FVector(10.0, 10.0, 10.0);

	/** 各轴缩放值下限 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = Settings)
	FVector MinValueClamp = FVector::ZeroVector;

protected:
	UE_API virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime) override;

	// 缓存查到的 FProperty，避免逐帧按名查找
	TArray<const FProperty*> PropertyCache;
};

/** 依据共享设置中的反转轴开关翻转视角输入 */
UCLASS(NotBlueprintable, MinimalAPI, meta = (DisplayName = "Aim Inversion Setting"))
class UYanInputModifierAimInversion : public UInputModifier
{
	GENERATED_BODY()

protected:
	UE_API virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime) override;
};

/** 死区裁剪方式。 */
UENUM()
enum class EYanDeadZoneType : uint8
{
	// 各轴独立裁剪，摇杆斜推时会被裁成十字形
	Axial,

	// 按向量长度裁剪，保持圆形手感
	Radial,
};

/** 死区取自哪只摇杆的设置。 */
UENUM()
enum class EYanDeadzoneStick : uint8
{
	MoveStick,
	LookStick,
};

/**
 * 依据共享设置中的摇杆死区裁剪输入。
 * 与引擎自带死区 Modifier 的区别在于阈值来自玩家设置而非资产上的固定值。
 */
UCLASS(NotBlueprintable, MinimalAPI, meta = (DisplayName = "Dead Zone Setting"))
class UYanInputModifierDeadZone : public UInputModifier
{
	GENERATED_BODY()

public:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = Settings, Config)
	EYanDeadZoneType Type = EYanDeadZoneType::Radial;

	/** 超过该值的输入直接钳为 1 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = Settings, Config)
	float UpperThreshold = 1.0f;

	/** 决定读取哪一项死区设置 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = Settings, Config)
	EYanDeadzoneStick DeadzoneStick = EYanDeadzoneStick::MoveStick;

protected:
	UE_API virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime) override;
};

/** 该 Modifier 作用于常规视角还是瞄准状态。 */
UENUM()
enum class EYanTargetingType : uint8
{
	Normal,
	Targeting,
};

/** 依据共享设置中的手柄灵敏度档位缩放视角输入 */
UCLASS(NotBlueprintable, MinimalAPI, meta = (DisplayName = "Gamepad Sensitivity Setting"))
class UYanInputModifierGamepadSensitivity : public UInputModifier
{
	GENERATED_BODY()

public:
	/** 读取常规还是瞄准灵敏度档位 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = Settings, Config)
	EYanTargetingType TargetingType = EYanTargetingType::Normal;

	/** 档位到倍率的映射表；未指定时该 Modifier 不生效 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Client,Server"))
	TObjectPtr<const UYanAimSensitivityData> SensitivityLevelTable;

protected:
	UE_API virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime) override;
};

#undef UE_API
