#pragma once
#include "AngelscriptGASAbility.h"
#include "ModalCameraMode.h"
#include "ModularAbilityCost.h"
#include "ModularAbilitySourceInterface.h"
#include "ModularCharacter.h"
#include "ModularPlayerController.h"
#include "Abilities/GameplayAbility.h"
#include "ActorComponent/ModularPawnComponent.h"

#include "ModularGameplayAbility.generated.h"

/**
 * Ability 的激活策略（ActivationPolicy）。
 *
 * 描述期望在何种 Input / 生命周期时机尝试激活。
 */
UENUM(BlueprintType)
enum class EModularAbilityActivationPolicy : uint8
{
	// Input 触发时尝试激活一次。
	OnInputTriggered,

	// Input 按住期间持续尝试保持激活（WhileInputActive）。
	WhileInputActive,

	// Avatar 绑定后尝试激活（OnSpawn）。
	OnSpawn
};

/**
 * Ability 的激活组（ActivationGroup）。
 *
 * 描述与其他 Ability 同时运行时的互斥 / 阻塞关系。
 */
UENUM(BlueprintType)
enum class EModularAbilityActivationGroup : uint8
{
	// 与任意其他 Ability 独立运行。
	Independent,

	// Exclusive 且可被同组其它 Exclusive 替换（Replaceable）。
	Exclusive_Replaceable,

	// Exclusive 且阻塞其它 Exclusive（Blocking）。
	Exclusive_Blocking,

	MAX UMETA(Hidden)
};

/** 激活失败时可驱动的 Montage 反馈消息（配合 GameplayMessageSubsystem）。 */
USTRUCT(BlueprintType)
struct FModularAbilityMontageFailureMessage
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<APlayerController> PlayerController = nullptr;

	// 本次失败的 GameplayTag 集合。
	UPROPERTY(BlueprintReadWrite)
	FGameplayTagContainer FailureTags;

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UAnimMontage> FailureMontage = nullptr;
};

/**
 * Modular GameplayAbility 基类。
 *
 * 在 UGameplayAbility 上扩展 ActivationPolicy / ActivationGroup、失败反馈与 CameraMode 覆盖。
 */
UCLASS(Abstract, HideCategories=("Input"), Meta=(ShortTooltip="扩展 UGameplayAbility：ActivationGroup / ActivationPolicy、失败反馈与工具函数。"))
class MODULARGAMEPLAYABILITIES_API UModularGameplayAbility : public UAngelscriptGASAbility
{
	GENERATED_BODY()

	// @todo 反模式：后续应移除对 ASC 的 friend 依赖。
	friend class UModularAbilitySystemComponent;

public:
	/** 构造并写入默认 Instancing / Replication / Activation 策略。 */
	UModularGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 自 CurrentActorInfo 取得 UModularAbilitySystemComponent。 */
	UFUNCTION(BlueprintCallable, Category = "Ability")
	UModularAbilitySystemComponent* GetModularAbilitySystemComponentFromActorInfo() const;

	/** 自 CurrentActorInfo 取得 AModularPlayerController（若存在）。 */
	UFUNCTION(BlueprintCallable, Category = "Ability")
	AModularPlayerController* GetModularPlayerControllerFromActorInfo() const;

	/** 自 CurrentActorInfo 取得 AController（沿 Owner 链回退）。 */
	UFUNCTION(BlueprintCallable, Category = "Ability")
	AController* GetControllerFromActorInfo() const;

	/** 自 CurrentActorInfo 取得 Avatar 上的 AModularCharacter。 */
	UFUNCTION(BlueprintCallable, Category = "Ability")
	AModularCharacter* GetModularCharacterFromActorInfo() const;

	/** 自 CurrentActorInfo 在 Avatar 上查找 UModularPawnComponent。 */
	UFUNCTION(BlueprintCallable, Category = "Ability")
	UModularPawnComponent* GetHeroComponentFromActorInfo() const;

	/** 当前 ActivationPolicy。 */
	EModularAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }
	/** 当前 ActivationGroup。 */
	EModularAbilityActivationGroup GetActivationGroup() const { return ActivationGroup; }

	/** Avatar 就绪且策略为 OnSpawn 时尝试自动激活。 */
	void TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const;

	/**
	 * 是否允许切换到目标 ActivationGroup。
	 *
	 * @return 合法迁移时为 true。
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Ability", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool CanChangeActivationGroup(EModularAbilityActivationGroup NewGroup) const;

	/**
	 * 切换到目标 ActivationGroup（同步更新 ASC 计数与互斥关系）。
	 *
	 * @return 成功切换或已在目标组时为 true。
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Ability", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool ChangeActivationGroup(EModularAbilityActivationGroup NewGroup);

	/** 为当前 Ability 设置 CameraMode 覆盖。 */
	UFUNCTION(BlueprintCallable, Category = "Ability")
	void SetCameraMode(TSubclassOf<UModalCameraMode> CameraMode);

	/** 清除 CameraMode 覆盖（EndAbility 时亦会调用）。 */
	UFUNCTION(BlueprintCallable, Category = "Ability")
	void ClearCameraMode();

	/** 激活失败入口：依次调用 Native 与 Blueprint 事件。 */
	void OnAbilityFailedToActivate(const FGameplayTagContainer& FailedReason) const
	{
		NativeOnAbilityFailedToActivate(FailedReason);
		ScriptOnAbilityFailedToActivate(FailedReason);
	}

protected:
	// C++ 侧激活失败回调。
	virtual void NativeOnAbilityFailedToActivate(const FGameplayTagContainer& FailedReason) const;

	// Blueprint 可实现的激活失败事件。
	UFUNCTION(BlueprintImplementableEvent)
	void ScriptOnAbilityFailedToActivate(const FGameplayTagContainer& FailedReason) const;

	//~UGameplayAbility Interface
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void SetCanBeCanceled(bool bCanBeCanceled) override;
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	// @todo Lyra 在 GameplayEffectContext 中附带 AbilitySourceObject 用于溯源；此处是否也需要对齐。
	virtual FGameplayEffectContextHandle MakeEffectContext(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const override;
	virtual void                         ApplyAbilityTagsToGameplayEffectSpec(FGameplayEffectSpec& Spec, FGameplayAbilitySpec* AbilitySpec) const override;
	virtual bool                         DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	//~UGameplayAbility Interface End

	/** Pawn Avatar 设置后的 C++ 钩子（蓝图见 K2_OnPawnAvatarSet）。 */
	virtual void OnPawnAvatarSet();

	/** 解析 Ability 的 SourceLevel / IModularAbilitySourceInterface / EffectCauser，供 EffectContext 使用。 */
	virtual void GetAbilitySource(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, float& OutSourceLevel, const IModularAbilitySourceInterface*& OutAbilitySource, AActor*& OutEffectCauser) const;

	/** Ability 被授予到 ASC 时触发（Blueprint：OnAbilityAdded）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = Ability, DisplayName = "OnAbilityAdded")
	void K2_OnAbilityAdded();

	/** Ability 从 ASC 移除时触发（Blueprint：OnAbilityRemoved）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = Ability, DisplayName = "OnAbilityRemoved")
	void K2_OnAbilityRemoved();

	/** ASC 与 Pawn Avatar 初始化完成时触发（Blueprint：OnPawnAvatarSet）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = Ability, DisplayName = "OnPawnAvatarSet")
	void K2_OnPawnAvatarSet();

protected:
	// 本 Ability 的 ActivationPolicy（默认在 CDO / Blueprint 配置）。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Activation")
	EModularAbilityActivationPolicy ActivationPolicy;

	// 本 Ability 的 ActivationGroup。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Activation")
	EModularAbilityActivationGroup ActivationGroup;

	// 额外 Instanced Cost（UModularAbilityCost），在 CheckCost / ApplyCost 中处理。
	UPROPERTY(EditDefaultsOnly, Instanced, Category = Costs)
	TArray<TObjectPtr<UModularAbilityCost>> AdditionalCosts;

	// FailureTag → 面向玩家的 FText 提示。
	UPROPERTY(EditDefaultsOnly, Category = "Advanced")
	TMap<FGameplayTag, FText> FailureTagToUserFacingMessages;

	// FailureTag → 失败时播放的 UAnimMontage。
	UPROPERTY(EditDefaultsOnly, Category = "Advanced")
	TMap<FGameplayTag, TObjectPtr<UAnimMontage>> FailureTagToAnimMontage;

	// 临时调试：取消时是否打印额外日志。
	UPROPERTY(EditDefaultsOnly, Category = "Advanced")
	bool bLogCancellation;

	// 当前 Ability 覆盖的 CameraMode 类。
	TSubclassOf<UModalCameraMode> ActiveCameraMode;
};
