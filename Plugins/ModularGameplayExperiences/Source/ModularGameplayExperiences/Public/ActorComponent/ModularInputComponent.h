// Copyright Chronicler.


#pragma once

#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Components/GameFrameworkInitStateInterface.h"
#include "Components/PawnComponent.h"
#include "GameFeature/GameFeatureAction_AddInputContextMapping.h"
#include "ModularInputComponent.generated.h"

/**
 * 模块化输入组件：将默认 InputMappingContext 与 PawnData 中的 InputConfig 绑定到本机 Enhanced Input。
 *
 * 设计参考 Lyra HeroComponent，但裁剪为 Experience 通用子集。
 *
 * @todo 评估是否将全部 InputTag 收敛到 DefaultGameplayTags.ini
 *
 * @see LyraGame/Character/LyraHeroComponent
 */
UCLASS(Blueprintable, Meta=(BlueprintSpawnableComponent))
class MODULARGAMEPLAYEXPERIENCES_API UModularInputComponent : public UPawnComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()

public:
	/** 构造输入组件。 */
	explicit UModularInputComponent(const FObjectInitializer& ObjectInitializer);

	/** 在 Actor 上查找 UModularInputComponent。 */
	UFUNCTION(BlueprintPure, Category="Experience")
	static UModularInputComponent* FindExperienceInputComponent(const AActor* Actor)
	{
		return (Actor ? Actor->FindComponentByClass<UModularInputComponent>() : nullptr);
	}

protected:
	/** 将映射上下文与 Native Action 绑定到 PlayerInput。 */
	virtual void InitializePlayerInput(UInputComponent* PlayerInputComponent);

	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_LookMouse(const FInputActionValue& InputActionValue);
	void Input_LookStick(const FInputActionValue& InputActionValue);
	void Input_Crouch(const FInputActionValue& InputActionValue);

	void InputActionMapping(UInputComponent* PlayerInputComponent, const APawn* Pawn, const APlayerController* PlayerController);

	/** 默认注入的 InputMappingContext 及优先级。 */
	UPROPERTY(EditAnywhere)
	TArray<FInputMappingContextAndPriority> DefaultInputMappings;

	/** 已完成输入绑定（非玩家永远不会为 true）。 */
	bool bReadyToBindInputs;

public:
	//~Begin IGameFrameworkInitStateInterface
	static const FName NAME_ActorFeatureName;
	virtual FName      GetFeatureName() const override { return NAME_ActorFeatureName; }
	virtual bool       CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const override;
	virtual void       CheckDefaultInitialization() override;
	virtual void       HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) override;
	virtual void       OnActorInitStateChanged(const FActorInitStateChangedParams& Params) override;
	//~End IGameFrameworkInitStateInterface

protected:
	//~Begin UActorComponent interface.
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent interface.
};
