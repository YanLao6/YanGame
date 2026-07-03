// Copyright Chronicler.

#pragma once
#include "InputModifiers.h"


#include "ModularInputModifiers.generated.h"

/**
 * InputModifier：按 ULocalPlayerSaveGame（共享槽位）中 double 属性对输入值做轴向缩放，再经 Min/Max Clamp。
 */
UCLASS(NotBlueprintable, MinimalAPI, meta = (DisplayName = "Setting Based Scalar"))
class UModularSettingBasedScalar : public UInputModifier
{
	GENERATED_BODY()

public:

	/** X 轴缩放系数对应的 UProperty 名称（在 SaveGame 类上查找）。 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings)
	FName XAxisScalarSettingName = NAME_None;

	/** Y 轴缩放系数对应的 UProperty 名称。 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings)
	FName YAxisScalarSettingName = NAME_None;

	/** Z 轴缩放系数对应的 UProperty 名称。 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings)
	FName ZAxisScalarSettingName = NAME_None;

	/** 各轴缩放系数上限（Clamp Max）。 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings)
	FVector MaxValueClamp = FVector(10.0, 10.0, 10.0);

	/** 各轴缩放系数下限（Clamp Min）。 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings)
	FVector MinValueClamp = FVector::ZeroVector;

protected:
	virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime) override;

	/** 解析后的 FProperty 指针缓存，避免每帧 FindPropertyByName。 */
	TArray<const FProperty*> PropertyCache;
};
