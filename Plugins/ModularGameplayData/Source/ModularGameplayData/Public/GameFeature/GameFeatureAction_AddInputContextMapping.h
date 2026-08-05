// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFeatureAction_WorldActionBase.h"
#include "UObject/SoftObjectPtr.h"
#include "GameFeatureAction_AddInputContextMapping.generated.h"

#define UE_API MODULARGAMEPLAYDATA_API

class AActor;
class UInputMappingContext;
class UPlayer;
class APlayerController;
struct FComponentRequestHandle;

USTRUCT()
struct FInputMappingContextAndPriority
{
	GENERATED_BODY()

	/**
	 * InputMappingContext 的 Soft 引用；运行时需经 UModularAssetManager::GetAsset 解析后方可安全 AddMappingContext。
	 * @todo 研究是否在 Data 层自动触发加载，减少忘记 GetAsset 的风险。
	 */
	UPROPERTY(EditAnywhere, Category="Input", meta=(AssetBundles="Client,Server"))
	TSoftObjectPtr<UInputMappingContext> InputMapping;

	/** Priority 越大，同一 LocalPlayer 上越优先匹配（见 UEnhancedInputLocalPlayerSubsystem::AddMappingContext）。 */
	UPROPERTY(EditAnywhere, Category="Input")
	int32 Priority = 0;
	
	/** 为 true 时：GameFeature 注册阶段把该 IMC 登记到 UEnhancedInputUserSettings。 */
	UPROPERTY(EditAnywhere, Category="Input")
	bool bRegisterWithSettings = true;
};

/**
 * GameFeature 输入映射动作。
 *
 * 将配置的 `InputMappingContext` 注入到本地玩家的 EnhancedInput 系统。
 * 要求目标项目已经启用并正确配置 EnhancedInput。
 */
UCLASS(MinimalAPI, meta = (DisplayName = "Add Input Mapping"))
class UGameFeatureAction_AddInputContextMapping final : public UGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()

public:
	//~ UGameFeatureAction 接口
	/** 注册 GameFeature 时注册输入映射上下文。 */
	UE_API virtual void OnGameFeatureRegistering() override;
	/** 激活 GameFeature 时向目标 World 注入输入映射。 */
	UE_API virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	/** 反激活 GameFeature 时回收输入映射。 */
	UE_API virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	/** 注销 GameFeature 时解绑注册状态。 */
	UE_API virtual void OnGameFeatureUnregistering() override;
	//~ UGameFeatureAction 接口结束

	//~ UObject 接口
#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~ UObject 接口结束

	UPROPERTY(EditAnywhere, Category="Input")
	TArray<FInputMappingContextAndPriority> InputMappings;

private:
	/** 每个 FGameFeatureStateChangeContext 一组：扩展句柄与曾注入过 IMC 的 PlayerController。 */
	struct FPerContextData
	{
		TArray<TSharedPtr<FComponentRequestHandle>> ExtensionRequestHandles;
		TArray<TWeakObjectPtr<APlayerController>> ControllersAddedTo;
	};

	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;
	
	/** OnStartGameInstance 委托句柄：新 GameInstance 启动时登记 IMC。 */
	FDelegateHandle RegisterInputContextMappingsForGameInstanceHandle;

	/** 将全部配置的 IMC 登记到 UserSettings，并监听 GameInstance/LocalPlayer 增删。 */
	UE_API void RegisterInputMappingContexts();
	
	/** 为指定 GameInstance 绑定 LocalPlayer 事件并在已存在 LocalPlayer 上登记 IMC。 */
	UE_API void RegisterInputContextMappingsForGameInstance(UGameInstance* GameInstance);

	/** 为指定 LocalPlayer 把需登记 Settings 的 IMC Register 到 UEnhancedInputUserSettings。 */
	UE_API void RegisterInputMappingContextsForLocalPlayer(ULocalPlayer* LocalPlayer);

	/** 解除 RegisterInputMappingContexts 绑定的全局与逐实例监听。 */
	UE_API void UnregisterInputMappingContexts();

	/** 对指定 GameInstance 解绑本 Action 的 LocalPlayer 回调。 */
	UE_API void UnregisterInputContextMappingsForGameInstance(UGameInstance* GameInstance);

	/** LocalPlayer 移除时从 UserSettings 反注册对应 IMC。 */
	UE_API void UnregisterInputMappingContextsForLocalPlayer(ULocalPlayer* LocalPlayer);

	//~ UGameFeatureAction_WorldActionBase 接口
	UE_API virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	//~ UGameFeatureAction_WorldActionBase 接口结束

	// 清空 Extension 请求句柄并从各 PlayerController 移除 IMC。
	UE_API void Reset(FPerContextData& ActiveData);
	// UGameFrameworkComponentManager 扩展回调：PlayerController 增删或 BindInputsNow 时增删 Mapping。
	UE_API void HandleControllerExtension(AActor* Actor, FName EventName, FGameFeatureStateChangeContext ChangeContext);
	// 向 LocalPlayer 的 UEnhancedInputLocalPlayerSubsystem 逐个 AddMappingContext。
	UE_API void AddInputMappingForPlayer(UPlayer* Player, FPerContextData& ActiveData);
	// 从指定 PlayerController 的 LocalPlayer 子系统 RemoveMappingContext。
	UE_API void RemoveInputMapping(APlayerController* PlayerController, FPerContextData& ActiveData);
};

#undef UE_API
