// Copyright Chronicler.

#pragma once

#include "GameFeature/GameFeatureAction_WorldActionBase.h"

#include "GameFeatureAction_TagRelationship.generated.h"

struct FComponentRequestHandle;
struct FWorldContext;
class UModularAbilitySystemComponent;
class UModularAbilityTagRelationshipMapping;

USTRUCT()
struct FGameFeatureTagRelationshipEntry
{
	GENERATED_BODY()

	// 挂入目标：该类 Actor 上的 ModularAbilitySystemComponent。
	UPROPERTY(EditAnywhere, Category = "Abilities")
	TSoftClassPtr<AActor> ActorClass;

	// 要挂入的 Ability Tag 关系表，其约束可横跨不同来源授予的技能。
	UPROPERTY(EditAnywhere, Category = "Abilities", meta = (AssetBundles = "Client,Server"))
	TSoftObjectPtr<const UModularAbilityTagRelationshipMapping> TagRelationshipMapping;
};

/**
 * 在 GameFeature 激活期间向指定类 Actor 的 ASC 挂入 Ability Tag 关系表。
 *
 * 两端都挂：关系表参与激活条件判定，客户端预测激活同样依赖它。
 * ASC 对多来源的关系表取并集，撤销时只摘除本 Action 挂入的那份。
 */
UCLASS(MinimalAPI, meta = (DisplayName = "Add Ability Tag Relationship"))
class UGameFeatureAction_TagRelationship final : public UGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()

public:
	//~Begin UGameFeatureAction Interface
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	//~End UGameFeatureAction Interface

	//~Begin UObject Interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~End UObject Interface

	UPROPERTY(EditAnywhere, Category = "Abilities", meta = (TitleProperty = "ActorClass", ShowOnlyInnerProperties))
	TArray<FGameFeatureTagRelationshipEntry> TagRelationshipList;

private:
	struct FAppliedMapping
	{
		TWeakObjectPtr<AActor>                         Actor;
		int32                                          EntryIndex = INDEX_NONE;
		TWeakObjectPtr<UModularAbilitySystemComponent> ASC;
		const UModularAbilityTagRelationshipMapping*   Mapping = nullptr;
	};

	struct FPerContextData
	{
		TArray<FAppliedMapping>                     AppliedMappings;
		TArray<TSharedPtr<FComponentRequestHandle>> ExtensionRequests;
	};

	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;

	//~Begin UGameFeatureAction_WorldActionBase Interface
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	//~End UGameFeatureAction_WorldActionBase Interface

	void Reset(FPerContextData& ActiveData);
	void HandleActorExtension(AActor* Actor, FName EventName, int32 EntryIndex, FGameFeatureStateChangeContext ChangeContext);
	void AddMapping(AActor* Actor, int32 EntryIndex, FPerContextData& ActiveData);
	void RemoveMappings(AActor* Actor, FPerContextData& ActiveData);
};
