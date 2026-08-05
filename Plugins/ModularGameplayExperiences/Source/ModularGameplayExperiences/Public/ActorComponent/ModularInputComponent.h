// Copyright Chronicler.


#pragma once

#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Components/GameFrameworkInitStateInterface.h"
#include "Components/PawnComponent.h"
#include "GameFeature/GameFeatureAction_AddInputContextMapping.h"
#include "UObject/ObjectKey.h"
#include "ModularInputComponent.generated.h"

#define UE_API MODULARGAMEPLAYEXPERIENCES_API

class UModularInputConfig;
class UModularInputConfigComponent;

/**
 * 模块化输入组件：将默认 InputMappingContext 与 PawnData 中的 InputConfig 绑定到本机 Enhanced Input。
 *
 * 设计参考 Lyra HeroComponent，但裁剪为 Experience 通用子集。
 *
 * @todo 评估是否将全部 InputTag 收敛到 DefaultGameplayTags.ini
 *
 * @see LyraGame/Character/LyraHeroComponent
 */
UCLASS(MinimalAPI, Blueprintable, Meta=(BlueprintSpawnableComponent))
class UModularInputComponent : public UPawnComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()

public:
	/** 构造输入组件。 */
	UE_API explicit UModularInputComponent(const FObjectInitializer& ObjectInitializer);

	/** 在 Actor 上查找 UModularInputComponent。 */
	UFUNCTION(BlueprintPure, Category="Experience")
	static UModularInputComponent* FindExperienceInputComponent(const AActor* Actor)
	{
		return (Actor ? Actor->FindComponentByClass<UModularInputComponent>() : nullptr);
	}

	/**
	 * 在 PawnData 自带的 InputConfig 之外追加一份 Native 输入绑定。
	 *
	 * 供 GameFeature / Experience 在运行期注入；同一 InputConfig 重复调用不会重复绑定。
	 * 仅绑定本组件已实现回调的 InputTag，其余 NativeInputActions 会被忽略。
	 * 需要 Pawn 的 InputComponent 为 UModularInputConfigComponent 子类。
	 */
	UE_API void AddAdditionalInputConfig(const UModularInputConfig* InputConfig);

	/** 移除由 AddAdditionalInputConfig 建立的绑定；未绑定过的 InputConfig 会被忽略。 */
	UE_API void RemoveAdditionalInputConfig(const UModularInputConfig* InputConfig);

	/** 基础 Native 输入是否已绑定完成；非本地玩家恒为 false。 */
	UFUNCTION(BlueprintPure, Category="Experience")
	bool IsReadyToBindInputs() const { return bReadyToBindInputs; }

protected:
	/** 将映射上下文与 Native Action 绑定到 PlayerInput。 */
	UE_API virtual void InitializePlayerInput(UInputComponent* PlayerInputComponent);

	UE_API void Input_Move(const FInputActionValue& InputActionValue);
	UE_API void Input_LookMouse(const FInputActionValue& InputActionValue);
	UE_API void Input_LookStick(const FInputActionValue& InputActionValue);
	UE_API void Input_Crouch(const FInputActionValue& InputActionValue);

	UE_API void InputActionMapping(UInputComponent* PlayerInputComponent, const APawn* Pawn, const APlayerController* PlayerController);

	// 将 InputConfig 中本组件支持的 NativeInputActions 绑定到对应回调；OutHandles 非空时记录句柄以供解绑。
	UE_API void BindNativeInputActions(UModularInputConfigComponent* InputConfigComponent, const UModularInputConfig* InputConfig, TArray<uint32>* OutHandles);

	/** 默认注入的 InputMappingContext 及优先级。 */
	UPROPERTY(EditAnywhere)
	TArray<FInputMappingContextAndPriority> DefaultInputMappings;

	/** 已完成输入绑定（非玩家永远不会为 true）。 */
	bool bReadyToBindInputs;

	/** 运行期追加的 InputConfig 与其绑定句柄，用于停用时对称解绑。 */
	TMap<TObjectKey<UModularInputConfig>, TArray<uint32>> AdditionalInputBindHandles;

public:
	//~Begin IGameFrameworkInitStateInterface
	static UE_API const FName NAME_ActorFeatureName;
	virtual FName      GetFeatureName() const override { return NAME_ActorFeatureName; }
	UE_API virtual bool       CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const override;
	UE_API virtual void       CheckDefaultInitialization() override;
	UE_API virtual void       HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) override;
	UE_API virtual void       OnActorInitStateChanged(const FActorInitStateChangedParams& Params) override;
	//~End IGameFrameworkInitStateInterface

protected:
	//~Begin UActorComponent interface.
	UE_API virtual void OnRegister() override;
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent interface.
};

#undef UE_API
