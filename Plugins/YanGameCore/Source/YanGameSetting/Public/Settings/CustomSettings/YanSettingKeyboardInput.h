// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnhancedActionKeyMapping.h"
#include "GameSettingValue.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#include "YanSettingKeyboardInput.generated.h"

#define UE_API YANGAMESETTING_API

/**
 * 单条按键绑定。键位数据本身由增强输入的 UEnhancedInputUserSettings 持有，
 * 本类只是设置界面与之交互的外壳，不额外存储映射。
 */
UCLASS(MinimalAPI)
class UYanSettingKeyboardInput : public UGameSettingValue
{
	GENERATED_BODY()

public:
	UE_API UYanSettingKeyboardInput();

	/** 从键位配置中的一行映射初始化本条目，QueryOptions 决定筛选哪一类按键 */
	UE_API void InitializeInputData(const UEnhancedPlayerMappableKeyProfile* KeyProfile, const FKeyMappingRow& MappingData, const FPlayerMappableKeyQueryOptions& QueryOptions);

	UE_API FText GetKeyTextFromSlot(const EPlayerMappableKeySlot InSlot) const;

	//~Begin UGameSettingValue Interface
	UE_API virtual void StoreInitial() override;
	UE_API virtual void ResetToDefault() override;
	UE_API virtual void RestoreToInitial() override;
	//~End UGameSettingValue Interface

	/** 改绑指定槽位；手柄按键不在本条目处理范围内，返回 false */
	UE_API bool ChangeBinding(int32 InKeyBindSlot, FKey NewKey);

	/** 取出该按键当前已绑定的所有动作，用于冲突提示 */
	UE_API void GetAllMappedActionsFromKey(int32 InKeyBindSlot, FKey Key, TArray<FName>& OutActionNames) const;

	/** 该条目是否已被玩家改过 */
	UE_API bool IsMappingCustomized() const;

	UE_API FText GetSettingDisplayName() const;
	UE_API FText GetSettingDisplayCategory() const;

	UE_API const FKeyMappingRow* FindKeyMappingRow() const;
	UE_API UEnhancedPlayerMappableKeyProfile* FindMappableKeyProfile() const;
	UE_API UEnhancedInputUserSettings* GetUserSettings() const;

protected:
	//~Begin UGameSettingValue Interface
	UE_API virtual void OnInitialized() override;
	//~End UGameSettingValue Interface

	/** 该条目对应的动作映射名 */
	FName ActionMappingName;

	/** 筛选条件，决定本条目只展示哪一类按键 */
	FPlayerMappableKeyQueryOptions QueryOptions;

	/** 所属键位配置的标识 */
	FString ProfileIdentifier;

	/** 各槽位的初始按键，用于取消修改时还原 */
	TMap<EPlayerMappableKeySlot, FKey> InitialKeyMappings;
};

#undef UE_API
