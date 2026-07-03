// Copyright Chronicler.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagAssetInterface.h"
#include "ModularCharacter.h"
#include "ActorComponent/ModularPawnComponent.h"
#include "ModularExperienceCharacter.generated.h"

/**
 * @TODO: 其功能已经被 AModularExperiencePawn 代替
 * Experience 项目推荐的角色基类：挂载 UModularPawnComponent 并实现 IGameplayTagAssetInterface 占位。
 */
UCLASS(Blueprintable, BlueprintType, Config="Game", meta=(ShortTooltip="玩家角色基类（Experience / Modular 管线）。"))
class MODULARGAMEPLAYEXPERIENCES_API AModularExperienceCharacter : public AModularCharacter, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	/** 构造：创建 ModularPawnComponent 并配置默认 MovementComponent。 */
	explicit AModularExperienceCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** @implements IGameplayTagAssetInterface：当前无默认 Tag，子类可覆盖。 */
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_Controller() override;
	virtual void OnRep_PlayerState() override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Character", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UModularPawnComponent> PawnExtensionComponent;
};
