// 局内配装发放组件声明。

#pragma once

#include "Components/ControllerComponent.h"
#include "CoreMinimal.h"
#include "Loadout/LoadoutResolver.h"
#include "Stash/StashTypes.h"
#include "InGameLoadoutComponent.generated.h"

#define UE_API EQUIPMANAGER_API

class APawn;
class UInGameLoadoutDefinition;
class UInventoryItemInstance;

/**
 * 局内配装发放。
 *
 * 开局装备由本组件一次性发放，取代按 GameFeature 生命周期或 PawnData Fragment 发放的旧路径。
 * 发放推迟到 Experience 就绪且控制器已控制 Pawn 之后：装备链路要在 Pawn 上生成实例，
 * 早于这两者就发等于往还不存在的角色身上挂东西。
 *
 * 玩家配装由拥有者客户端上行，服务器决议后才落位；客户端只交出意图，准入一律由服务器判断。
 */
UCLASS(MinimalAPI, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UInGameLoadoutComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	/** 构造发放组件。 */
	UE_API UInGameLoadoutComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * 追加一份开局配装，须在发放发生前调用。
	 *
	 * 配装分层叠加：模式给全员的那份与英雄专属的那份各自加入，互不覆写对方的配置来源。
	 * 同一份资产重复加入只保留一份，注入方因而不必彼此知情。
	 */
	UE_API void AddLoadoutDefinition(TSoftObjectPtr<UInGameLoadoutDefinition> InDefinition);

	/** 收回此前加入的一份配装；已发放后调用不改变本局内容。 */
	UE_API void RemoveLoadoutDefinition(TSoftObjectPtr<UInGameLoadoutDefinition> InDefinition);

	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End UActorComponent Interface

protected:
	/**
	 * 本局的开局配装，按加入顺序叠加。
	 *
	 * 用 EditAnywhere 而非 EditDefaultsOnly：注入方未必有机会为每个模式建一份组件蓝图，
	 * 实例上写不进就只剩「一模式一子类」这一条路。
	 */
	UPROPERTY(EditAnywhere, Category=Loadout)
	TArray<TSoftObjectPtr<UInGameLoadoutDefinition>> LoadoutDefinitions;

	/**
	 * 等待玩家配装上行的时长，单位秒。
	 *
	 * 客户端的上行可能迟到或根本不来（仓库读取失败、玩家没配装），
	 * 超时后以模式配置兜底发放，否则玩家会空着手进场且没有任何提示。
	 */
	UPROPERTY(EditAnywhere, Category=Loadout, meta=(ClampMin=0.0))
	float LoadoutRequestTimeout = 3.0f;

private:
	// Experience 就绪后推进一次发放条件判断。
	void OnExperienceLoaded(const class UModularExperienceDefinition* Experience);

	// 控制器改换 Pawn 后，等待该 Pawn 进入就绪状态。
	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	/**
	 * Pawn 达到就绪状态后推进一次发放条件判断。
	 *
	 * 发放等到这一步而非 Possess 当场，是因为英雄专属配装由 PawnData Fragment 在
	 * 数据初始化阶段加入，早于就绪但晚于 Possess；在 Possess 当场发放会漏掉那一层。
	 */
	void OnPawnReady(const struct FActorInitStateChangedParams& Params);

	// 条件齐备时发放，否则原样返回等待下一次推进。
	void TryGrantLoadout();

	// 决议并把结果写入背包与快捷栏。
	void GrantLoadout();

	// 回收本组件发放过的物品与槽位。
	void RevokeLoadout();

	// 拥有者客户端把选中的配装交给服务器。
	void SubmitLoadoutFromStash();

	// 仓库读取完成后重试上行。
	void OnStashChanged();

	// 等待上行超时，改以模式配置兜底。
	void OnLoadoutRequestTimeout();

	// 接收客户端上行的配装意图。
	UFUNCTION(Server, Reliable)
	void ServerSubmitLoadout(const FLoadoutRequest& Request);

private:
	// 客户端上行的配装意图，决议时作为覆盖来源。
	UPROPERTY(Transient)
	FLoadoutRequest PendingRequest;

	// 本组件发放出的物品，回收时据此移除。
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UInventoryItemInstance>> GrantedItems;

	// 本组件占用的快捷栏槽位。
	TArray<int32> OccupiedSlots;

	// 等待上行的超时句柄。
	FTimerHandle RequestTimeoutHandle;

	// 仓库变化的监听句柄，上行后解除。
	FDelegateHandle StashChangedHandle;

	// 已就绪并可接收发放的 Pawn；就绪通知早于本组件确认条件时据其补判一次。
	UPROPERTY(Transient)
	TObjectPtr<APawn> ReadyPawn;

	// Experience 是否已就绪。
	bool bExperienceLoaded = false;

	// 是否已收到上行，或已确定不再等待。
	bool bRequestSettled = false;

	// 是否已发放，发放只进行一次。
	bool bGranted = false;
};

#undef UE_API
