// Copyright Chronicler.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagAssetInterface.h"
#include "ModularPawn.h"
#include "ActorComponent/ModularPawnComponent.h"
#include "ModularExperiencePawn.generated.h"

#define UE_API MODULARGAMEPLAYEXPERIENCES_API

/**
 * Experience 项目推荐的 Pawn 基类：挂载 UModularPawnComponent 并实现 IGameplayTagAssetInterface 占位。 
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, Config="Game", meta=(ShortTooltip="玩家 Pawn 基类（Experience / Modular 管线）。"))
class AModularExperiencePawn : public AModularPawn, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	/** 构造：创建 ModularPawnComponent。 */
	UE_API explicit AModularExperiencePawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pawn", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UModularPawnComponent> PawnExtensionComponent;
};

#undef UE_API
