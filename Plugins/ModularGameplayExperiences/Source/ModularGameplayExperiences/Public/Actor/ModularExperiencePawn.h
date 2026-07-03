// Copyright Chronicler.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagAssetInterface.h"
#include "ModularPawn.h"
#include "ActorComponent/ModularPawnComponent.h"
#include "ModularExperiencePawn.generated.h"

/**
 * Experience 项目推荐的 Pawn 基类：挂载 UModularPawnComponent 并实现 IGameplayTagAssetInterface 占位。 
 */
UCLASS(Blueprintable, BlueprintType, Config="Game", meta=(ShortTooltip="玩家 Pawn 基类（Experience / Modular 管线）。"))
class MODULARGAMEPLAYEXPERIENCES_API AModularExperiencePawn : public AModularPawn, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	/** 构造：创建 ModularPawnComponent。 */
	explicit AModularExperiencePawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pawn", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UModularPawnComponent> PawnExtensionComponent;
};
