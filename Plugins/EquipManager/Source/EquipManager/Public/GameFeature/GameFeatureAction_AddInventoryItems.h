// 背包物品发放 GameFeatureAction 声明。

#pragma once

#include "CoreMinimal.h"
#include "GameFeature/GameFeatureAction_WorldActionBase.h"
#include "Templates/SubclassOf.h"

#include "GameFeatureAction_AddInventoryItems.generated.h"

class AController;
class UInventoryItemDefinition;
class UInventoryItemInstance;
class UQuickBarComponent;
struct FComponentRequestHandle;
struct FWorldContext;

/**
 * 单条物品发放配置。
 *
 * 描述一件要写入背包的物品，以及它是否需要同时占用快捷栏槽位。
 */
USTRUCT(BlueprintType)
struct FInventoryItemGrant
{
	GENERATED_BODY()

	/** 要发放的物品定义。 */
	UPROPERTY(EditAnywhere, Category=Inventory, meta=(AssetBundles="Client,Server"))
	TSoftClassPtr<UInventoryItemDefinition> ItemDefinition;

	/** 发放的堆叠数量。 */
	UPROPERTY(EditAnywhere, Category=Inventory, meta=(ClampMin=1))
	int32 StackCount = 1;

	/** 物品要占用的快捷栏槽位；保持 `-1` 表示只进背包，不进入快捷栏也不会被装备。 */
	UPROPERTY(EditAnywhere, Category=Inventory, meta=(ClampMin=-1))
	int32 QuickBarSlot = INDEX_NONE;
};

/**
 * 针对某个控制器类的物品发放条目。
 */
USTRUCT()
struct FGameFeatureInventoryEntry
{
	GENERATED_BODY()

	/** 接收物品的控制器类；物品写入其身上的背包组件与快捷栏组件。 */
	UPROPERTY(EditAnywhere, Category=Inventory)
	TSoftClassPtr<AActor> ActorClass;

	/** 该控制器类要发放的物品列表。 */
	UPROPERTY(EditAnywhere, Category=Inventory)
	TArray<FInventoryItemGrant> GrantedItems;

	/** 发放完成后是否自动激活序号最小的已占用槽位，使对应物品立即装备到 Pawn 上。 */
	UPROPERTY(EditAnywhere, Category=Inventory)
	bool bActivateFirstOccupiedSlot = false;
};

/**
 * 物品发放 GameFeatureAction。
 *
 * 按控制器类向背包写入初始物品，并可选地占用快捷栏槽位。
 * 由于装备链路需要控制器已控制 Pawn，实际发放会推迟到控制器获得 Pawn 之后执行。
 */
UCLASS(MinimalAPI, meta=(DisplayName="Add Inventory Items"))
class UGameFeatureAction_AddInventoryItems final : public UGameFeatureAction_WorldActionBase
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

	/** 按控制器类划分的物品发放配置。 */
	UPROPERTY(EditAnywhere, Category=Inventory, meta=(TitleProperty="ActorClass", ShowOnlyInnerProperties))
	TArray<FGameFeatureInventoryEntry> ItemsList;

private:
	// 单个控制器上已发放的内容，用于失效时精确回滚。
	// 同一控制器类可以配置多条条目，此结构按控制器汇总它们的发放结果。
	struct FActorExtensions
	{
		// 已写入背包的物品实例，使用弱引用避免物品在别处被销毁后产生悬垂指针。
		TArray<TWeakObjectPtr<UInventoryItemInstance>> Items;
		// 本次发放实际占用的快捷栏槽位。
		TArray<int32>                                  OccupiedSlots;
		// 等待 Pawn 就绪的委托句柄，每条待发放条目各持有一个。
		TArray<FDelegateHandle>                        PawnChangedHandles;
		// 已受理但仍在等待 Pawn 的条目序号。
		TSet<int32>                                    PendingEntries;
		// 已完成发放的条目序号，保证同一控制器的同一条目只发一次。
		TSet<int32>                                    GrantedEntries;
	};

	struct FPerContextData
	{
		TMap<AActor*, FActorExtensions>             ActiveExtensions;
		TArray<TSharedPtr<FComponentRequestHandle>> ComponentRequests;
	};

	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;

	//~Begin UGameFeatureAction_WorldActionBase Interface
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	//~End UGameFeatureAction_WorldActionBase Interface

	void Reset(FPerContextData& ActiveData);
	void HandleActorExtension(AActor* Actor, FName EventName, int32 EntryIndex, FGameFeatureStateChangeContext ChangeContext);

	// 控制器尚未控制 Pawn 时，先登记条目并订阅 Pawn 变更通知。
	void WaitForPawn(AController* Controller, FActorExtensions& Extensions, int32 EntryIndex, FGameFeatureStateChangeContext ChangeContext);
	// 向控制器身上的背包与快捷栏写入指定条目配置的物品。
	void GrantItems(AController* Controller, int32 EntryIndex, FActorExtensions& Extensions);
	// 撤销此前发放的物品并解除 Pawn 变更订阅。
	void RemoveItems(AActor* Actor, FPerContextData& ActiveData);
};
