// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFeature/GameFeatureAction_WorldActionBase.h"
#include "UObject/SoftObjectPtr.h"

#include "GameFeatureAction_AddInputBinding.generated.h"

#define UE_API MODULARGAMEPLAYABILITIES_API

class AActor;
class APawn;
class UModularInputConfig;
struct FComponentRequestHandle;

/**
 * GameFeature 输入绑定动作。
 *
 * 在 PawnData 自带的 InputConfig 之外，向本地玩家 Pawn 追加绑定一组 UModularInputConfig，
 * 停用时对称解绑。Ability 输入交由 UModularAbilityExtensionComponent 绑定，
 * Native 输入交由 UModularInputComponent 绑定。
 *
 * 前置条件：目标 Pawn 至少持有上述两个组件之一，
 * 且其 InputComponent 为 UModularInputConfigComponent 子类。
 */
UCLASS(MinimalAPI, meta = (DisplayName = "Add Input Binds"))
class UGameFeatureAction_AddInputBinding final : public UGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()

public:
	//~Begin UGameFeatureAction interface
	UE_API virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	UE_API virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	//~End UGameFeatureAction interface

	//~Begin UObject interface
#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~End UObject interface

	/** 要追加绑定的 InputConfig；AssetBundles 保证其在 Feature 激活期间保持加载。 */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (AssetBundles = "Client,Server"))
	TArray<TSoftObjectPtr<const UModularInputConfig>> InputConfigs;

private:
	/** 每个 FGameFeatureStateChangeContext 一组：扩展句柄与已注入过绑定的 Pawn。 */
	struct FPerContextData
	{
		TArray<TSharedPtr<FComponentRequestHandle>> ExtensionRequestHandles;
		TArray<TWeakObjectPtr<APawn>>               PawnsAddedTo;
	};

	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;

	//~Begin UGameFeatureAction_WorldActionBase interface
	UE_API virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	//~End UGameFeatureAction_WorldActionBase interface

	// 清空扩展句柄，并对全部已注入的 Pawn 解绑。
	UE_API void Reset(FPerContextData& ActiveData);
	// UGameFrameworkComponentManager 扩展回调：Pawn 增删或 BindInputsNow 时增删绑定。
	UE_API void HandlePawnExtension(AActor* Actor, FName EventName, FGameFeatureStateChangeContext ChangeContext);
	// 向本地玩家 Pawn 的 Ability / Input 组件追加绑定；同一 Pawn 幂等。
	UE_API void AddInputBindingForPlayer(APawn* Pawn, FPerContextData& ActiveData);
	// 移除先前对该 Pawn 追加的绑定。
	UE_API void RemoveInputBinding(APawn* Pawn, FPerContextData& ActiveData);
};

#undef UE_API
