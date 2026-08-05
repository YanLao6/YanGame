#pragma once
#include "ModularAbilitySystemComponent.h"
#include "UObject/ObjectKey.h"

#include "ModularAbilityExtensionComponent.generated.h"

#define UE_API MODULARGAMEPLAYABILITIES_API

class UModularInputConfig;

/**
 * Pawn 侧 Ability 扩展组件。
 *
 * 负责把 Pawn 与 UModularAbilitySystemComponent 对接，并处理 Input 绑定与初始化状态流转。
 */
UCLASS(MinimalAPI, ClassGroup=AbilitySystem, hidecategories=(Object,LOD,Lighting,Transform,Sockets,TextureStreaming), editinlinenew, meta=(BlueprintSpawnableComponent))
class UModularAbilityExtensionComponent : public UPawnComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()

public:
	/** 构造 Ability 扩展组件。 */
	UE_API UModularAbilityExtensionComponent(const FObjectInitializer& ObjectInitializer);

	/** 绑定并初始化目标 AbilitySystem。 */
	UE_API virtual void InitializeAbilitySystem(UModularAbilitySystemComponent* InASC, AActor* InOwnerActor);

	/** 解除当前 AbilitySystem 绑定并清理状态。 */
	UE_API void UninitializeAbilitySystem();

	/**
	 * 获取当前缓存的 UModularAbilitySystemComponent。
	 *
	 * ASC 可能由其他 Actor 持有（例如 PlayerState），此处仅返回已绑定的指针。
	 */
	UFUNCTION(BlueprintPure, Category = "AbilitySystem|Pawn")
	UModularAbilitySystemComponent* GetModularAbilitySystemComponent() const { return AbilitySystemComponent; }

	/** 控制器变化时刷新 Input / Ability 初始化流程。 */
	UE_API void HandleControllerChanged();
	/** 初始化玩家 Input 绑定。 */
	UE_API void InitializePlayerInput(UInputComponent* PlayerInputComponent);
	/** 按 InputConfig 将 Enhanced Input Action 映射到 AbilityInputTag。 */
	UE_API void InputActionMapping(UInputComponent* PlayerInputComponent, const APawn* Pawn);
	/** Ability 输入按下：转发到 ASC。 */
	UE_API void Input_AbilityInputTagPressed(FGameplayTag InputTag);
	/** Ability 输入抬起：转发到 ASC。 */
	UE_API void Input_AbilityInputTagReleased(FGameplayTag InputTag);

	/**
	 * 在 PawnData 自带的 InputConfig 之外追加一份 Ability 输入绑定。
	 *
	 * 供 GameFeature / Experience 在运行期注入；同一 InputConfig 重复调用不会重复绑定。
	 * 需要 Pawn 的 InputComponent 为 UModularInputConfigComponent 子类。
	 */
	UE_API void AddAdditionalInputConfig(const UModularInputConfig* InputConfig);

	/** 移除由 AddAdditionalInputConfig 建立的绑定；未绑定过的 InputConfig 会被忽略。 */
	UE_API void RemoveAdditionalInputConfig(const UModularInputConfig* InputConfig);

	/** 基础 Ability 输入是否已绑定完成；非本地控制的 Pawn 恒为 false。 */
	UFUNCTION(BlueprintPure, Category = "AbilitySystem|Pawn")
	bool IsReadyToBindInputs() const { return bReadyToBindInputs; }

	//~IGameFrameworkInitStateInterface
	virtual FName GetFeatureName() const override { return NAME_ActorFeatureName; }
	UE_API virtual bool  CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const override;
	UE_API virtual void  HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) override;
	UE_API virtual void  OnActorInitStateChanged(const FActorInitStateChangedParams& Params) override;
	UE_API virtual void  CheckDefaultInitialization() override;
	//~IGameFrameworkInitStateInterface 结束

	static UE_API const FName NAME_ActorFeatureName;

protected:
	UE_API virtual void OnRegister() override;
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 缓存的 AbilitySystemComponent 指针（便于 Blueprint / 代码访问）。
	 *
	 * @todo 评估用更清晰的 service / 初始化管线替代 friend 式启动依赖。
	 */
	UPROPERTY()
	TObjectPtr<UModularAbilitySystemComponent> AbilitySystemComponent;

	/** Pawn 成为 ASC 的 Avatar 并完成初始化后广播。 */
	FSimpleMulticastDelegate OnAbilitySystemInitialized;

	/** Pawn 从 ASC 的 Avatar 解绑后广播。 */
	FSimpleMulticastDelegate OnAbilitySystemUninitialized;

	/** 基础 Ability 输入绑定是否完成，见 IsReadyToBindInputs。 */
	bool bReadyToBindInputs = false;

	/** 运行期追加的 InputConfig 与其绑定句柄，用于停用时对称解绑。 */
	TMap<TObjectKey<UModularInputConfig>, TArray<uint32>> AdditionalInputBindHandles;
};

#undef UE_API
