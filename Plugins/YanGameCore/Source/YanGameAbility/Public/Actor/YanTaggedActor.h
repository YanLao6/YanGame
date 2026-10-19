#pragma once

#include "GameFramework/Actor.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"

#include "YanTaggedActor.generated.h"

#define UE_API YANGAMEABILITY_API

/**
 * 携带 GameplayTag 的场景 Actor。
 * 供技能查询与效果筛选使用，标签在编辑器中静态配置，运行期不变。
 */
UCLASS(MinimalAPI)
class AYanTaggedActor : public AActor, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	UE_API AYanTaggedActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin IGameplayTagAssetInterface
	UE_API virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	//~End IGameplayTagAssetInterface

	//~Begin UObject Interface
#if WITH_EDITOR
	UE_API virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif
	//~End UObject Interface

protected:
	// 该 Actor 的静态 Gameplay 标签，与 AActor::Tags 互斥以免两套标签体系混用
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Actor)
	FGameplayTagContainer StaticGameplayTags;
};

#undef UE_API
