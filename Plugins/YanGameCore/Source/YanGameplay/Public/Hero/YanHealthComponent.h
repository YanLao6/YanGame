// 生命组件声明。

#pragma once

#include "CoreMinimal.h"
#include "Components/GameFrameworkComponent.h"
#include "YanHealthComponent.generated.h"

#define UE_API YANGAMEPLAY_API

class UYanHealthComponent;
class UModularAbilitySystemComponent;
class UYanHealthSet;
struct FGameplayEffectSpec;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FYanHealth_DeathEvent, AActor*, OwningActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FYanHealth_AttributeChanged, UYanHealthComponent*, HealthComponent, float, OldValue, float, NewValue, AActor*, Instigator);

/** 死亡流程所处的阶段。 */
UENUM(BlueprintType)
enum class EYanDeathState : uint8
{
	NotDead = 0,
	DeathStarted,
	DeathFinished
};

/**
 * 生命组件。
 *
 * 自身不保存血量，只是 UYanHealthSet 的对外门面：把属性变化转成蓝图可绑定的委托，
 * 并维护一份可复制的死亡阶段，在生命归零时抛出死亡事件与击杀消息。
 */
UCLASS(MinimalAPI, Blueprintable, Meta=(BlueprintSpawnableComponent))
class UYanHealthComponent : public UGameFrameworkComponent
{
	GENERATED_BODY()

public:
	UE_API explicit UYanHealthComponent(const FObjectInitializer& ObjectInitializer);

	/** 取指定 Actor 上的生命组件；不存在时返回空。 */
	UFUNCTION(BlueprintPure, Category = "Yan|Health")
	static UYanHealthComponent* FindHealthComponent(const AActor* Actor)
	{
		return (Actor ? Actor->FindComponentByClass<UYanHealthComponent>() : nullptr);
	}

	/** 绑定到指定的能力系统组件，并开始监听其生命属性。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Health")
	UE_API void InitializeWithAbilitySystem(UModularAbilitySystemComponent* InASC);

	/** 解除与能力系统的绑定。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Health")
	UE_API void UninitializeFromAbilitySystem();

	/** 当前生命值。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Health")
	UE_API float GetHealth() const;

	/** 当前生命上限。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Health")
	UE_API float GetMaxHealth() const;

	/** 当前生命值占上限的比例，取值 0 到 1。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Health")
	UE_API float GetHealthNormalized() const;

	UFUNCTION(BlueprintCallable, Category = "Yan|Health")
	EYanDeathState GetDeathState() const { return DeathState; }

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Yan|Health", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool IsDeadOrDying() const { return (DeathState > EYanDeathState::NotDead); }

	/** 开始死亡流程。 */
	UE_API virtual void StartDeath();

	/** 结束死亡流程。 */
	UE_API virtual void FinishDeath();

	/** 对拥有者施加足以致死的伤害。 */
	UE_API virtual void DamageSelfDestruct(bool bFellOutOfWorld = false);

	/** 生命值变化时广播；客户端上的 Instigator 可能为空。 */
	UPROPERTY(BlueprintAssignable)
	FYanHealth_AttributeChanged OnHealthChanged;

	/** 生命上限变化时广播；客户端上的 Instigator 可能为空。 */
	UPROPERTY(BlueprintAssignable)
	FYanHealth_AttributeChanged OnMaxHealthChanged;

	/** 死亡流程开始时广播。 */
	UPROPERTY(BlueprintAssignable)
	FYanHealth_DeathEvent OnDeathStarted;

	/** 死亡流程结束时广播。 */
	UPROPERTY(BlueprintAssignable)
	FYanHealth_DeathEvent OnDeathFinished;

protected:
	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void OnUnregister() override;
	//~End UActorComponent Interface

	// 由 Ability 扩展组件的 Avatar 通知驱动，见 BeginPlay 中的订阅
	UE_API void HandleAbilitySystemInitialized();
	UE_API void HandleAbilitySystemUninitialized();

	// 清除死亡相关的松散 Tag，用于初始化与复活
	UE_API void ClearGameplayTags();

	UE_API virtual void HandleHealthChanged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue);
	UE_API virtual void HandleMaxHealthChanged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue);
	UE_API virtual void HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue);

	UFUNCTION()
	UE_API virtual void OnRep_DeathState(EYanDeathState OldDeathState);

	UPROPERTY()
	TObjectPtr<UModularAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<const UYanHealthSet> HealthSet;

	// 死亡阶段由服务端推进后复制到客户端
	UPROPERTY(ReplicatedUsing = OnRep_DeathState)
	EYanDeathState DeathState;
};

#undef UE_API
