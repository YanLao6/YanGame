// Copyright Chronicler.

#pragma once

#include "AbilitySystemComponent.h"
#include "AngelscriptAbilitySystemComponent.h"
#include "NativeGameplayTags.h"
#include "GameplayAbilities/ModularAbilityTagRelationshipMapping.h"
#include "GameplayAbilities/ModularGameplayAbility.h"

#include "ModularAbilitySystemComponent.generated.h"

#define UE_API MODULARGAMEPLAYABILITIES_API

MODULARGAMEPLAYABILITIES_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Gameplay_AbilityInputBlocked);

/**
 * Modular AbilitySystem 组件。
 *
 * 在 GAS 基础上扩展 ActivationGroup、InputTag 驱动激活、TagRelationship 约束等。
 */
UCLASS(MinimalAPI, ClassGroup=AbilitySystem, hidecategories=(Object,LOD,Lighting,Transform,Sockets,TextureStreaming), editinlinenew, meta=(BlueprintSpawnableComponent))
class UModularAbilitySystemComponent : public UAngelscriptAbilitySystemComponent
{
	GENERATED_BODY()

public:
	/** 构造 AbilitySystem 组件。 */
	UE_API UModularAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UActorComponent 接口
	/** 结束运行时：从 GlobalAbilitySystem 注销并清理。 */
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~UActorComponent 接口结束

	/** 初始化 Owner / Avatar 对应的 AbilityActorInfo。 */
	UE_API virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	using TShouldCancelAbilityFunc = TFunctionRef<bool(const UModularGameplayAbility* ModularAbility, FGameplayAbilitySpecHandle Handle)>;
	/** 按自定义条件批量取消 Ability。 */
	UE_API void CancelAbilitiesByFunc(TShouldCancelAbilityFunc ShouldCancelFunc, bool bReplicateCancelAbility);

	/** 取消所有由 Input 驱动的 Ability（OnInputTriggered / WhileInputActive）。 */
	UE_API void CancelInputActivatedAbilities(bool bReplicateCancelAbility);

	/** 将指定 InputTag 标记为按下，并记入本帧待处理 Spec。 */
	UE_API void AbilityInputTagPressed(const FGameplayTag& InputTag);
	/** 将指定 InputTag 标记为抬起，并记入本帧待处理 Spec。 */
	UE_API void AbilityInputTagReleased(const FGameplayTag& InputTag);
	/** 查询 InputTag 当前是否在 ActiveInputTags 中。 */
	UE_API bool IsInputTagActive(const FGameplayTag& InputTag) const;
	/** 直接设置某 InputTag 的激活状态。 */
	UE_API void SetInputTagActive(const FGameplayTag& InputTag, bool bActive);


	/** 处理本帧累积的 Input，并驱动 TryActivate / InputPressed 事件。 */
	UE_API void ProcessAbilityInput(float DeltaTime, bool bGamePaused);
	/** 清空 Input 缓存队列。 */
	UE_API void ClearAbilityInput();

	/** 指定 ActivationGroup 是否因 Exclusive_Blocking 被阻塞。 */
	UE_API bool IsActivationGroupBlocked(EModularAbilityActivationGroup Group) const;
	/** 将运行中 Ability 计入 ActivationGroup，并触发组内互斥取消逻辑。 */
	UE_API void AddAbilityToActivationGroup(EModularAbilityActivationGroup Group, UModularGameplayAbility* ModularAbility);
	/** 从 ActivationGroup 计数中移除 Ability。 */
	UE_API void RemoveAbilityFromActivationGroup(EModularAbilityActivationGroup Group, UModularGameplayAbility* ModularAbility);
	/** 取消某组内（可排除指定实例）的所有 Ability。 */
	UE_API void CancelActivationGroupAbilities(EModularAbilityActivationGroup Group, UModularGameplayAbility* IgnoreModularAbility, bool bReplicateCancelAbility);

	/** 通过专用 GameplayEffect 为 ASC 动态授予 GameplayTag。 */
	UE_API void AddDynamicTagGameplayEffect(const FGameplayTag& Tag);

	/** 移除用于动态授予 Tag 的 GameplayEffect 实例。 */
	UE_API void RemoveDynamicTagGameplayEffect(const FGameplayTag& Tag);

	/** 根据 AbilityHandle 与 ActivationInfo 取出已复制的 TargetData。 */
	UE_API void GetAbilityTargetData(const FGameplayAbilitySpecHandle AbilityHandle, FGameplayAbilityActivationInfo ActivationInfo, FGameplayAbilityTargetDataHandle& OutTargetDataHandle);

	/** 设置 TagRelationshipMapping；传入 nullptr 表示清空。 */
	UE_API void SetTagRelationshipMapping(UModularAbilityTagRelationshipMapping* NewMapping);

	/** 依据 AbilityTags 扩展额外的 ActivationRequired / ActivationBlocked。 */
	UE_API void GetAdditionalActivationTagRequirements(const FGameplayTagContainer& AbilityTags, FGameplayTagContainer& OutActivationRequired, FGameplayTagContainer& OutActivationBlocked) const;

protected:
	// Avatar 就绪后尝试激活 OnSpawn 策略的 Ability。
	UE_API void TryActivateAbilitiesOnSpawn();

	UE_API virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	UE_API virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;

	UE_API virtual void NotifyAbilityActivated(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability) override;
	UE_API virtual void NotifyAbilityFailed(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason) override;
	UE_API virtual void NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled) override;
	UE_API virtual void ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility, bool bEnableBlockTags, const FGameplayTagContainer& BlockTags, bool bExecuteCancelTags, const FGameplayTagContainer& CancelTags) override;
	UE_API virtual void HandleChangeAbilityCanBeCanceled(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility, bool bCanBeCanceled) override;

	/** Client：同步通知某次 Ability 激活失败（非本地控制且 Ability 支持网络时）。 */
	UFUNCTION(Client, Unreliable)
	UE_API void ClientNotifyAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason);

	// 本地处理失败：转发到 ModularGameplayAbility 的失败回调。
	UE_API void HandleAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason);

protected:
	// Tag 关系表：用于展开 Block / Cancel / 额外激活条件。
	UPROPERTY()
	TObjectPtr<UModularAbilityTagRelationshipMapping> TagRelationshipMapping;

	// 本帧发生 InputPressed 的 AbilitySpecHandle。
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;

	// 本帧发生 InputReleased 的 AbilitySpecHandle。
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;

	// 当前仍按住 Input 的 AbilitySpecHandle。
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;

	// 各 ActivationGroup 正在运行的 Ability 数量。
	int32 ActivationGroupCounts[static_cast<uint8>(EModularAbilityActivationGroup::MAX)];

private:
	FGameplayTagContainer ActiveInputTags;
};

#undef UE_API
