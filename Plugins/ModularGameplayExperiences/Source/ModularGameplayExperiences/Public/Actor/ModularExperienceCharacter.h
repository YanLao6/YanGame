// Copyright Chronicler.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagAssetInterface.h"
#include "ModularCharacter.h"
#include "ActorComponent/ModularPawnComponent.h"
#include "ModularExperienceCharacter.generated.h"

#define UE_API MODULARGAMEPLAYEXPERIENCES_API

/**
 * @TODO: 其功能已经被 AModularExperiencePawn 代替
 * Experience 项目推荐的角色基类：挂载 UModularPawnComponent 并实现 IGameplayTagAssetInterface 占位。
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, Config="Game", meta=(ShortTooltip="玩家角色基类（Experience / Modular 管线）。"))
class AModularExperienceCharacter : public AModularCharacter, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	/** 构造：创建 ModularPawnComponent 并配置默认 MovementComponent。 */
	UE_API explicit AModularExperienceCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** @implements IGameplayTagAssetInterface：当前无默认 Tag，子类可覆盖。 */
	UE_API virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	UE_API virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	UE_API virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	UE_API virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;

protected:
	UE_API virtual void BeginPlay() override;

public:
	UE_API virtual void Tick(float DeltaTime) override;
	UE_API virtual void PossessedBy(AController* NewController) override;
	UE_API virtual void UnPossessed() override;
	UE_API virtual void OnRep_Controller() override;
	UE_API virtual void OnRep_PlayerState() override;

	UE_API virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Character", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UModularPawnComponent> PawnExtensionComponent;
};

#undef UE_API
