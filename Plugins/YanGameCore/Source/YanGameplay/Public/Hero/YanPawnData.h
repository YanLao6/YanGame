// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataAsset/IAbilityPawnDataInterface.h"
#include "DataAsset/ModularPawnData.h"
#include "YanPawnData.generated.h"

/**
 * 
 */
UCLASS()
class YANGAMEPLAY_API UYanPawnData : public UModularPawnData, public IAbilityPawnDataInterface
{
	GENERATED_BODY()

public:
	explicit UYanPawnData(const FObjectInitializer& ObjectInitializer);

	//~Begin IAbilityPawnDataInterface
	virtual TArray<UModularAbilitySet*>            GetAbilitySet() const override { return AbilitySets; };
	virtual UModularAbilityTagRelationshipMapping* GetTagRelationshipMapping() const override { return AbilityTagRelationshipMapping; };
	//~End IAbilityPawnDataInterface

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pawn")
	TArray<TObjectPtr<UModularAbilitySet>> AbilitySets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pawn")
	TObjectPtr<UModularAbilityTagRelationshipMapping> AbilityTagRelationshipMapping;
};
