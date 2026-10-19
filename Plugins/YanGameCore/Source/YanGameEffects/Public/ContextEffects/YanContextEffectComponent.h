#pragma once

#include "Components/ActorComponent.h"
#include "ContextEffects/YanContextEffectsInterface.h"
#include "GameplayTagContainer.h"

#include "YanContextEffectComponent.generated.h"

#define UE_API YANGAMEEFFECTS_API

class UYanContextEffectsLibrary;

/**
 * Actor 的上下文特效响应者。
 *
 * 持有该 Actor 当前的上下文标签与特效库，收到播放请求后把请求上下文与自身上下文合并，
 * 查表生成音效与 Niagara；请求未指定附着目标时也由本组件补齐。
 * 上下文与特效库都可在运行时替换，用于换装、换地形材质套件等场景。
 */
UCLASS(MinimalAPI, ClassGroup = (Yan), hidecategories = (Variable, Tags, ComponentTick, ComponentReplication, Activation, Cooking, AssetUserData, Collision), CollapseCategories, meta = (BlueprintSpawnableComponent))
class UYanContextEffectComponent : public UActorComponent, public IYanContextEffectsInterface
{
	GENERATED_BODY()

public:
	UE_API UYanContextEffectComponent();

	/** 该 Actor 的常驻上下文 */
	UPROPERTY(EditAnywhere, Category = "ContextEffects")
	FGameplayTagContainer DefaultEffectContexts;

	UPROPERTY(EditAnywhere, Category = "ContextEffects")
	TSet<TSoftObjectPtr<UYanContextEffectsLibrary>> DefaultContextEffectsLibraries;

	UFUNCTION(BlueprintCallable, Category = "ContextEffects")
	UE_API void UpdateEffectContexts(const FGameplayTagContainer& NewEffectContexts);

	UFUNCTION(BlueprintCallable, Category = "ContextEffects")
	UE_API void UpdateLibraries(const TSet<TSoftObjectPtr<UYanContextEffectsLibrary>>& NewContextEffectsLibraries);

	//~Begin IYanContextEffectsInterface Interface
	UE_API virtual void PlayContextEffect_Implementation(const FYanContextEffectRequest& Request) override;
	//~End IYanContextEffectsInterface Interface

protected:
	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	//~End UActorComponent Interface

private:
	/** 请求未指定附着目标时的兜底：插槽通常在骨骼网格体上，而角色根组件多为胶囊体 */
	USceneComponent* ResolveAttachTarget() const;

	UPROPERTY(Transient)
	FGameplayTagContainer CurrentContexts;

	/** 解析后的特效库，是本 Actor 特效库集合的唯一真值 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UYanContextEffectsLibrary>> ResolvedLibraries;
};

#undef UE_API
