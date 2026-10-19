// Bot 创建组件声明。

#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "Templates/SubclassOf.h"
#include "ModularBotCreationComponent.generated.h"

#define UE_API MODULARGAMEPLAYEXPERIENCES_API

class AAIController;
class UModularExperienceDefinition;

/**
 * Bot 创建组件。
 *
 * 挂在 GameState 上，等 Experience 加载完成后在服务端批量生成 AI 控制器并让其入场。
 * 生成数量可由启动 URL 的 NumBots 选项覆盖。
 */
UCLASS(MinimalAPI, Blueprintable, Abstract)
class UModularBotCreationComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UE_API explicit UModularBotCreationComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	//~End UActorComponent Interface

protected:
	/** 期望生成的 Bot 数量。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Gameplay)
	int32 NumBotsToCreate = 5;

	/** 生成 Bot 时使用的 AI 控制器类；未设置则不生成。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Gameplay)
	TSubclassOf<AAIController> BotControllerClass;

	/** 可供随机选取的 Bot 名字池。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Gameplay)
	TArray<FString> RandomBotNames;

	// 本局尚未被取用的名字，取一个少一个
	TArray<FString> RemainingBotNames;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AAIController>> SpawnedBotList;

	/** 生成一个 Bot。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Gameplay)
	UE_API virtual void SpawnOneBot();

	/** 移除一个已生成的 Bot。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Gameplay)
	UE_API virtual void RemoveOneBot();

	/** 一次性把 Bot 补足到目标数量。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly, Category=Gameplay)
	UE_API void ServerCreateBots();

#if WITH_SERVER_CODE
public:
	void Cheat_AddBot() { SpawnOneBot(); }
	void Cheat_RemoveBot() { RemoveOneBot(); }

	/** 从名字池取一个名字；池空时按序号生成一个占位名。 */
	UE_API FString CreateBotName(int32 PlayerIndex);
#endif

private:
	void OnExperienceLoaded(const UModularExperienceDefinition* Experience);
};

#undef UE_API
