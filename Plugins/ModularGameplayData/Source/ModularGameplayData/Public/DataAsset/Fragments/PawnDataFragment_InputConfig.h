// Copyright Chronicler.

#pragma once

#include "DataAsset/PawnDataFragment.h"
#include "GameFeature/GameFeatureAction_AddInputContextMapping.h"

#include "PawnDataFragment_InputConfig.generated.h"

#define UE_API MODULARGAMEPLAYDATA_API

class UModularInputConfig;

/**
 * 输入配置 Fragment。
 *
 * 承载该 Pawn 的 InputTag 映射表与 InputMappingContext。本 Fragment 不自行注册：
 * 注册必须与输入组件的 ClearAllMappings 保持同一时序，且原生输入的回调宿主在输入组件内，
 * 故由 UModularInputComponent 与 UModularAbilityExtensionComponent 查询后统一执行。
 */
UCLASS(MinimalAPI, Const, DisplayName = "Input Config")
class UPawnDataFragment_InputConfig : public UPawnDataFragment
{
	GENERATED_BODY()

public:
	//~Begin UPawnDataFragment Interface
	virtual EPawnDataFragmentScope GetScope() const override { return EPawnDataFragmentScope::Pawn; }
	virtual bool                   RequiresAuthority() const override { return false; }
	/** 输入组件以单例语义查询本 Fragment，重复配置会使其中一份静默失效。 */
	virtual bool                   AllowsMultipleInstances() const override { return false; }
#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsFragmentValid(FDataValidationContext& Context) const override;
#endif
	//~End UPawnDataFragment Interface

	/** 将 UInputAction 映射到 InputTag，供 Ability 输入与原生输入共同查询。 */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UModularInputConfig> InputConfig;

	/** 仅追加 Ability 输入绑定的额外配置，不参与原生输入与键位映射。 */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TArray<TObjectPtr<UModularInputConfig>> AdditionalAbilityInputConfigs;

	/** 该 Pawn 生效期间追加到本地玩家的 InputMappingContext。 */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TArray<FInputMappingContextAndPriority> InputMappings;
};

#undef UE_API
