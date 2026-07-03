// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFeature/GameFeatureAction_WorldActionBase.h"
#include "MoverAbility/MoverAbilitySet.h"

#include "GameFeatureAction_AddMoverAbilities.generated.h"

class UMoverAbilitySet;
struct FComponentRequestHandle;

/**
 * Experience 条目：指定目标 Actor 类及要授予的 MoverAbilitySet 列表。
 */
USTRUCT()
struct FGameFeatureMoverEntry
{
	GENERATED_BODY()

	/** 要注入移动能力的 Actor 类 */
	UPROPERTY(EditAnywhere, Category = "Mover")
	TSoftClassPtr<AActor> ActorClass;

	/** 要授予的 MoverAbilitySet 列表 */
	UPROPERTY(EditAnywhere, Category = "Mover", meta = (AssetBundles = "Client,Server"))
	TArray<TSoftObjectPtr<const UMoverAbilitySet>> GrantedMoverAbilitySets;
};

/**
 * GameFeatureAction：Experience 激活时向指定 Actor 的 UMoverComponent 批量注册 UMoverAbilitySet。
 *
 * 目标 Actor 须持有 UMoverComponent。激活时调用 GiveTo，停用时调用 TakeFrom 完整撤销。
 * 
 * WARRING: 不可用
 */
UCLASS(MinimalAPI, meta = (DisplayName = "Add Mover Abilities"))
class UGameFeatureAction_AddMoverAbilities final : public UGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()

public:
	//~Begin UGameFeatureAction interface
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	//~End UGameFeatureAction interface

	//~Begin UObject interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~End UObject interface

	UPROPERTY(EditAnywhere, Category = "Mover", meta = (TitleProperty = "ActorClass", ShowOnlyInnerProperties))
	TArray<FGameFeatureMoverEntry> MoversList;

private:
	struct FActorMoverExtensions
	{
		TArray<FMoverAbilitySet_GrantedHandles> MoverSetHandles;
	};

	struct FPerContextData
	{
		TMap<AActor*, FActorMoverExtensions>        ActiveExtensions;
		TArray<TSharedPtr<FComponentRequestHandle>> ComponentRequests;
	};

	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;

	//~Begin UGameFeatureAction_WorldActionBase interface
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	//~End UGameFeatureAction_WorldActionBase interface

	void Reset(FPerContextData& ActiveData);
	void HandleActorExtension(AActor* Actor, FName EventName, int32 EntryIndex, FGameFeatureStateChangeContext ChangeContext);
	void AddActorMoverAbilities(AActor* Actor, const FGameFeatureMoverEntry& Entry, FPerContextData& ActiveData);
	void RemoveActorMoverAbilities(AActor* Actor, FPerContextData& ActiveData);
};
