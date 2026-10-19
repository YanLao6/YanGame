// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameSetting.h"

#include "GameSettingValue.generated.h"

#define UE_API GAMESETTINGS_API

class UObject;

//--------------------------------------
// UGameSettingValue
//--------------------------------------

/**
 * 所有"值类"设置的基类：这类设置可被修改，因而也可重置为默认值或还原为初始值。
 */
UCLASS(MinimalAPI, Abstract)
class UGameSettingValue : public UGameSetting
{
	GENERATED_BODY()

public:
	UE_API UGameSettingValue();

	/** 记录该设置的初始值。初始化时调用；apply 设置时同样应当调用。 */
	virtual void StoreInitial() PURE_VIRTUAL(, );

	/** 将属性重置为默认值。 */
	virtual void ResetToDefault() PURE_VIRTUAL(, );

	/** 将设置还原为初始值，即打开设置界面、尚未做任何改动时的值。 */
	virtual void RestoreToInitial() PURE_VIRTUAL(, );

protected:
	UE_API virtual void OnInitialized() override;
};

#undef UE_API
