#pragma once

#include "AbilitySystemInterface.h"
#include "GameplayTagStack.h"
#include "ModularPlayerState.h"
#include "ActorComponent/ModularAbilitySystemComponent.h"
#include "Player/ModularExperiencePlayerState.h"

#include "ModularAbilityPlayerState.generated.h"

#define UE_API MODULARGAMEPLAYABILITIES_API

/**
 * 持有 UModularAbilitySystemComponent 与 Stat Tag Stack 的 PlayerState。
 */
UCLASS(MinimalAPI, Config = "Game")
class AModularAbilityPlayerState : public AModularExperiencePlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	/** 构造并创建 ASC 子对象。 */
	UE_API AModularAbilityPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 取得本 PlayerState 的 Modular ASC。 */
	UFUNCTION(BlueprintCallable, Category = "Lyra|PlayerState")
	UModularAbilitySystemComponent* GetModularAbilitySystemComponent() const { return AbilitySystemComponent; }

	UE_API virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UE_API virtual void PostInitializeComponents() override;

	/** 增加 Tag 的 StackCount（StackCount < 1 时无效，仅限 Authority）。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ModularAbility")
	UE_API void AddStatTagStack(FGameplayTag Tag, int32 StackCount);

	/** 减少 Tag 的 StackCount（StackCount < 1 时无效，仅限 Authority）。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ModularAbility")
	UE_API void RemoveStatTagStack(FGameplayTag Tag, int32 StackCount);

	/** 查询 Tag 当前层数（不存在则为 0）。 */
	UFUNCTION(BlueprintCallable, Category = "ModularAbility")
	UE_API int32 GetStatTagStackCount(FGameplayTag Tag) const;

	/** 是否存在至少一层指定 Tag。 */
	UFUNCTION(BlueprintCallable, Category = "ModularAbility")
	UE_API bool HasStatTag(FGameplayTag Tag) const;

	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static UE_API const FName NAME_ModularAbilityReady;

protected:
	//~Begin AModularExperiencePlayerState Interface
	UE_API virtual void PreRevokePawnData() override;
	UE_API virtual void PostApplyPawnData() override;
	//~End AModularExperiencePlayerState Interface

	// Player 角色使用的 ASC 子对象。
	UPROPERTY(VisibleAnywhere, Category = "ModularAbility|PlayerState")
	TObjectPtr<UModularAbilitySystemComponent> AbilitySystemComponent;
};

#undef UE_API
