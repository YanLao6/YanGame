// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cosmetics/ModularCharacterPartTypes.h"
#include "Engine/DeveloperSettingsBackedByCVars.h"

#include "ModularCosmeticDeveloperSettings.generated.h"

#define UE_API MODULARGAMEPLAYDATA_API

struct FPropertyChangedEvent;

/** 编辑器外观作弊与既有部件的关系。 */
UENUM()
enum class EModularCosmeticCheatMode
{
	// 屏蔽角色原有部件，只显示作弊部件
	ReplaceParts,

	// 在原有部件之上叠加
	AddParts
};

/**
 * 外观调试设置，仅存于本机编辑器配置，用于在 PIE 中快速试装。
 * 改动后会立即重新下发给 PIE 中的所有玩家控制器。
 */
UCLASS(MinimalAPI, config = EditorPerProjectUserSettings)
class UModularCosmeticDeveloperSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()

public:
	UE_API UModularCosmeticDeveloperSettings();

	//~Begin UDeveloperSettings Interface
	UE_API virtual FName GetCategoryName() const override;
	//~End UDeveloperSettings Interface

	UPROPERTY(Transient, EditAnywhere)
	TArray<FModularCharacterPart> CheatCosmeticCharacterParts;

	UPROPERTY(Transient, EditAnywhere)
	EModularCosmeticCheatMode CheatMode = EModularCosmeticCheatMode::ReplaceParts;

#if WITH_EDITOR
	/** 由编辑器在 PIE 启动时调用，提示当前存在外观覆盖。 */
	UE_API void OnPlayInEditorStarted() const;

	//~Begin UObject Interface
	UE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	UE_API virtual void PostReloadConfig(FProperty* PropertyThatWasLoaded) override;
	UE_API virtual void PostInitProperties() override;
	//~End UObject Interface

private:
	void ApplySettings();
	void ReapplyLoadoutIfInPIE();
#endif
};

#undef UE_API
