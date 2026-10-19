// 开局配装注入 GameFeatureAction 声明。

#pragma once

#include "CoreMinimal.h"
#include "GameFeature/GameFeatureAction_WorldActionBase.h"
#include "Templates/SubclassOf.h"

#include "GameFeatureAction_AddLoadout.generated.h"

class AActor;
class UInGameLoadoutComponent;
class UInGameLoadoutDefinition;
struct FComponentRequestHandle;
struct FWorldContext;

/**
 * 开局配装注入 GameFeatureAction。
 *
 * 向指定控制器类挂上发放组件并交给它本局的配装资产，实际发放时机由组件自行把控。
 * 本 Action 不碰背包与快捷栏：发放要等 Experience 就绪、控制器已控制 Pawn、玩家配装上行，
 * 这些条件只有组件看得全。
 *
 * 两端都挂：服务器负责决议与落位，拥有者客户端负责把自己的配装交上去。
 */
UCLASS(MinimalAPI, meta=(DisplayName="Add Loadout"))
class UGameFeatureAction_AddLoadout final : public UGameFeatureAction_WorldActionBase
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

	/** 接收配装的控制器类；发放组件挂在它身上。 */
	UPROPERTY(EditAnywhere, Category=Loadout)
	TSoftClassPtr<AActor> ActorClass;

	/** 本局的开局配装。 */
	UPROPERTY(EditAnywhere, Category=Loadout, meta=(AssetBundles="Client,Server"))
	TSoftObjectPtr<UInGameLoadoutDefinition> LoadoutDefinition;

private:
	// 本 Action 在某个控制器上留下的痕迹。
	struct FAddedComponent
	{
		TWeakObjectPtr<UInGameLoadoutComponent> Component;

		// 组件是否由本 Action 创建；复用别处的组件时只摘掉自己加的配装，不销毁它。
		bool bCreatedByThisAction = false;
	};

	struct FPerContextData
	{
		TMap<AActor*, FAddedComponent> AddedComponents;

		TArray<TSharedPtr<FComponentRequestHandle>> ComponentRequests;
	};

	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;

	//~Begin UGameFeatureAction_WorldActionBase Interface
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	//~End UGameFeatureAction_WorldActionBase Interface

	void Reset(FPerContextData& ActiveData);
	void HandleActorExtension(AActor* Actor, FName EventName, FGameFeatureStateChangeContext ChangeContext);

	// 挂上发放组件；配装在注册前写入，组件启动时配置便已就位。
	void AddLoadoutComponent(AActor* Actor, FPerContextData& ActiveData);
	// 摘掉本 Action 挂上的发放组件。
	void RemoveLoadoutComponent(AActor* Actor, FPerContextData& ActiveData);
};
