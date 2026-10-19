// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/CheatManager.h"

#include "ModularCosmeticCheats.generated.h"

#define UE_API MODULARGAMEPLAYDATA_API

class UModularControllerComponent_CharacterParts;

/**
 * 外观部件相关的控制台作弊指令。
 * 在 CheatManager 创建时自动挂载，仅在带 CheatManager 的构建中生效。
 */
UCLASS(MinimalAPI, NotBlueprintable)
class UModularCosmeticCheats final : public UCheatManagerExtension
{
	GENERATED_BODY()

public:
	UE_API UModularCosmeticCheats();

	/** 追加一个部件；bSuppressNaturalParts 为真时同时屏蔽角色原有部件。 */
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	UE_API void AddCharacterPart(const FString& AssetName, bool bSuppressNaturalParts = true);

	/** 清掉此前的作弊部件后再追加一个。 */
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	UE_API void ReplaceCharacterPart(const FString& AssetName, bool bSuppressNaturalParts = true);

	/** 清除全部作弊部件，恢复角色原有外观。 */
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	UE_API void ClearCharacterPartOverrides();

private:
	UModularControllerComponent_CharacterParts* GetCosmeticComponent() const;
};

#undef UE_API
