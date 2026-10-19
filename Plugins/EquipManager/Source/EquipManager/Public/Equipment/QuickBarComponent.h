// 快捷栏组件声明。

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilities/ModularAbilityGateProvider.h"
#include "GameplayAbilities/ModularAbilitySet.h"
#include "QuickBarComponent.generated.h"

#define UE_API EQUIPMANAGER_API

class UModularAbilitySystemComponent;
class UEquipmentManagerComponent;
class UEquipmentInstance;
class UInventoryItemInstance;
class FLifetimeProperty;

/**
 * 单个快捷栏槽位的准入规则。
 *
 * 规则数组按索引对应槽位；未配置到的槽位与查询为空的槽位均不作限制。
 */
USTRUCT(BlueprintType)
struct FQuickBarSlotRule
{
	GENERATED_BODY()

	/** 该槽位接受的物品类别查询；留空表示接受任意物品。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=QuickBar)
	FGameplayTagQuery AcceptQuery;
};

/**
 * 快捷栏组件。
 *
 * 负责维护固定槽位中的物品实例，并根据激活槽位自动向装备管理组件申请装备或卸下物品。
 */
UCLASS(MinimalAPI, Blueprintable, meta=(BlueprintSpawnableComponent))
class UQuickBarComponent : public UActorComponent, public IModularAbilityGateProvider
{
	GENERATED_BODY()

public:
	/** 构造快捷栏组件并启用复制。 */
	UE_API UQuickBarComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UObject Interface
	// 注册快捷栏槽位与当前激活索引的复制属性。
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End UObject Interface

	//~Begin UActorComponent Interface
	// 组件开始运行时拉取配置覆盖并补齐槽位数组长度。
	UE_API virtual void BeginPlay() override;
	// 从 ASC 摘除门禁提供方登记，避免本组件销毁后仍被查询。
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End UActorComponent Interface

	//~Begin IModularAbilityGateProvider Interface
	UE_API virtual EModularAbilityGateResult QueryAbilityGate(FGameplayTag GateTag) const override;
	//~End IModularAbilityGateProvider Interface

	/** 向前循环切换到下一个非空槽位。 */
	UFUNCTION(BlueprintCallable, Category="QuickBar")
	UE_API void CycleActiveSlotForward();

	/** 向后循环切换到上一个非空槽位。 */
	UFUNCTION(BlueprintCallable, Category="QuickBar")
	UE_API void CycleActiveSlotBackward();

	/**
	 * 请求切换到指定槽位。
	 *
	 * 客户端调用会本地立即生效并同时向服务器发起请求，因而切槽表现与技能可用性不必等待往返；
	 * 服务器随后的权威值到达即确认，被拒绝时会由服务器回推权威值纠正。
	 * 空槽与非法索引在两端用同一份规则拒绝，故预测不会因规则不一致而频繁纠正。
	 */
	UFUNCTION(BlueprintCallable, Category="QuickBar")
	UE_API void SetActiveSlotIndex(int32 NewIndex);

	/** 返回全部快捷栏槽位中的物品实例。 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false)
	UE_API TArray<UInventoryItemInstance*> GetSlots() const
	{
		return Slots;
	}

	/**
	 * 返回当前生效的槽位索引，未激活时为 `-1`。
	 *
	 * 预测尚未被确认时返回预测值，故 UI 与技能判定读到的都是玩家刚按下的那个槽位。
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false)
	UE_API int32 GetActiveSlotIndex() const { return bHasPredictedActiveSlot ? PredictedActiveSlotIndex : ActiveSlotIndex; }

	/** 判断该槽位当前能否被激活；两端共用，是预测与服务器校验的同一份规则。 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false)
	UE_API bool CanActivateSlot(int32 SlotIndex) const;

	/**
	 * 返回激活该槽位的输入 Tag，按 `InputTag.QuickBar.Slot.<Index>` 的命名约定由索引拼出。
	 *
	 * 约定而非配置：切槽技能在 AbilitySet 中已按同名 Tag 绑定输入，此处不再重复声明映射。
	 * 索引非法或该 Tag 未注册（如槽位数多于已定义的按键）时返回空 Tag，调用方据此隐藏按键提示。
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false)
	static UE_API FGameplayTag GetSlotInputTag(int32 SlotIndex);

	/**
	 * 返回门禁该槽位技能的 Tag，按 `Ability.Gate.QuickBar.Slot.<Index>` 的命名约定由索引拼出。
	 *
	 * 槽位物品的技能在入槽时即被预授予，故其 Spec 早于装备生效抵达客户端；该 Tag 写在 Spec 上，
	 * 激活时由本组件按当前生效槽位作答，从而使非生效槽位的技能无法激活。
	 * 与 GetSlotInputTag 同为约定而非配置；索引非法或该 Tag 未注册时返回空 Tag，此时该槽位技能不设门禁。
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false)
	static UE_API FGameplayTag GetSlotGateTag(int32 SlotIndex);

	/** 返回当前激活槽位中的物品实例。 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false)
	UE_API UInventoryItemInstance* GetActiveSlotItem() const;

	/** 返回第一个空闲槽位索引，若没有空位则返回 `INDEX_NONE`。 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false)
	UE_API int32 GetNextFreeItemSlot() const;

	/** 返回第一个既空闲又接受该物品的槽位索引，若不存在则返回 `INDEX_NONE`。 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false)
	UE_API int32 GetNextFreeItemSlotForItem(const UInventoryItemInstance* Item) const;

	/**
	 * 判断物品当前能否放入指定槽位。
	 *
	 * 纯查询且不依赖服务器权威，客户端可直接调用以预判拖拽结果。
	 * 判定包含索引合法、槽位空闲与类别匹配三项；物品与槽位的类别必须同时存在或同时缺省。
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure=false)
	UE_API bool CanAddItemToSlot(int32 SlotIndex, const UInventoryItemInstance* Item) const;

	/** 将物品放入指定槽位，仅服务器可调用。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	UE_API void AddItemToSlot(int32 SlotIndex, UInventoryItemInstance* Item);

	/**
	 * 从头查找第一个接受该物品的空闲槽位并放入，仅服务器可调用，返回是否写入成功。
	 *
	 * 未配置类别的物品只会落入同样未配置类别的槽位。
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	UE_API bool TryAddItem(UInventoryItemInstance* Item);

	/** 从指定槽位移除物品，仅服务器可调用，并返回被移除的物品。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	UE_API UInventoryItemInstance* RemoveItemFromSlot(int32 SlotIndex);

	/**
	 * 用外部配置替换槽位数量与准入规则，供 UQuickBarConfigComponent 在启动时推送。
	 *
	 * 仅在槽位尚未装入任何物品时允许缩减槽位数，因而不会丢弃已入槽的物品。
	 */
	UE_API void ApplyConfigOverride(int32 InNumSlots, const TArray<FQuickBarSlotRule>& InSlotRules);

private:
	// 使槽位数组及与其一一对应的句柄、装备实例数组长度不少于 NumSlots。
	void EnsureSlotArrays();

	/**
	 * 拥有者改换 Pawn 时把槽位内容重新落到新 Pawn 上。
	 *
	 * 快捷栏随 Controller 存续，装备实例与技能却分别绑在 Pawn 与其 ASC 上，
	 * 重生后两者都需重建；ASC 初始化晚于 Possess，故推迟到下一帧再同步。
	 */
	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	// 按当前 Pawn 重放全部非空槽位的技能授予与装备实例创建；ASC 尚未就绪时自行推迟重试。
	void SyncSlotsToCurrentPawn();
	// 请求切换槽位；RequestId 为客户端请求序号，随回执原样带回以便识别是哪一次请求的结果。
	UFUNCTION(Server, Reliable)
	void ServerSetActiveSlotIndex(int32 NewIndex, uint8 RequestId);

	/**
	 * 权威索引的回执，接受、拒绝与服务器自发变更都经此送达拥有端。
	 *
	 * 不依赖属性复制来确认：权威值与请求值相同时复制不会触发，且复制不携带请求序号。
	 * RequestId 为 0 表示服务器自发变更（如物品被移出槽位），此时无条件清除客户端预测。
	 */
	UFUNCTION(Client, Reliable)
	void ClientSyncActiveSlotIndex(int32 AuthoritativeIndex, uint8 RequestId);

	// 服务器落位权威索引并向拥有端发出回执；RequestId 为 0 表示本次变更非客户端请求触发。
	void ApplyAuthoritativeSlot(int32 NewIndex, uint8 RequestId);

	// 应用本地生效槽位的全部后果：装备生效、门禁 Tag、对外广播。
	void ApplyActiveSlotChanged();

	// 拥有者是否为本地控制器；本地控制时无需回执，权威变更已就地生效。
	bool IsOwnerLocallyControlled() const;

	// 汇总物品定义上各片段贡献的类别 Tag。
	static void GatherItemSlotCategoryTags(const UInventoryItemInstance* Item, FGameplayTagContainer& OutTags);

	// 为该槽位物品创建装备实例并生成隐藏的附属 Actor，仅服务器执行。
	void CreateSlotEquipment(int32 SlotIndex);
	// 销毁该槽位的装备实例及其附属 Actor，仅服务器执行。
	void DestroySlotEquipment(int32 SlotIndex);
	// 把当前激活槽位的物品设为生效意图，服务器与客户端各自依本地激活索引调用。
	void RefreshActiveSlotEquipment();

	// 预授予该槽位物品的技能，并在其 Spec 上写入本槽位的门禁 Tag。
	void GrantSlotAbilities(int32 SlotIndex);
	// 回收该槽位预授予的技能。
	void TakeBackSlotAbilities(int32 SlotIndex);

	/**
	 * 把自己登记为 ASC 上的门禁提供方，已登记到同一个 ASC 时无操作。
	 *
	 * 未登记期间本组件负责的门禁 Tag 会被判为不满足，槽位技能因此一律打不出，
	 * 故凡是 ASC 可能刚刚就绪或刚刚改换的时机都要调用一次。
	 */
	void EnsureGateProviderRegistered();

	// 由门禁 Tag 反解槽位索引，与 GetSlotGateTag 共用同一条命名约定；不属于快捷栏的 Tag 返回 INDEX_NONE。
	static int32 ParseSlotIndexFromGateTag(const FGameplayTag& GateTag);

	// 在拥有者控制的 Pawn 上查找装备管理组件。
	UEquipmentManagerComponent*     FindEquipmentManager() const;
	// 快捷栏挂在 Controller 上，能力系统组件位于其 PlayerState。
	UModularAbilitySystemComponent* FindAbilitySystemComponent() const;

protected:
	/** 快捷栏默认可容纳的槽位数量。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NumSlots = 4;

	/** 各槽位的准入规则，按索引对应槽位；长度可短于槽位数，未覆盖的槽位不作限制。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=QuickBar)
	TArray<FQuickBarSlotRule> SlotRules;

	// 槽位数组复制更新后广播消息。
	UFUNCTION()
	void OnRep_Slots();

	// 激活槽位索引复制更新后广播消息。
	UFUNCTION()
	void OnRep_ActiveSlotIndex();

private:
	// 快捷栏中每个槽位存放的物品实例。
	UPROPERTY(ReplicatedUsing=OnRep_Slots)
	TArray<TObjectPtr<UInventoryItemInstance>> Slots;

	// 服务器权威的激活槽位索引。
	UPROPERTY(ReplicatedUsing=OnRep_ActiveSlotIndex)
	int32 ActiveSlotIndex = -1;

	// 客户端预测中的激活槽位索引；与权威值分开存放，避免本地写入污染待纠正的权威值。
	int32 PredictedActiveSlotIndex = -1;

	// 预测是否仍待确认；仅当收到最新一次请求的回执或服务器自发变更时清除。
	bool bHasPredictedActiveSlot = false;

	// 最近一次发往服务器的请求序号；回执序号与之相等才说明最新预测已被确认。
	// 取值循环于 1..255，0 保留给服务器自发变更。连续快切时旧回执因此不会清掉更新的预测。
	uint8 LastSentRequestId = 0;

	// 各槽位预授予技能的回收句柄，按索引对应槽位；仅服务器持有，客户端各项为空。
	UPROPERTY()
	TArray<FModularAbilitySet_GrantedHandles> SlotGrantedHandles;

	// 各槽位持有的装备实例，按索引对应槽位；仅服务器持有，客户端经装备列表复制获得实例。
	UPROPERTY()
	TArray<TObjectPtr<UEquipmentInstance>> SlotEquipmentInstances;

	// 已登记门禁提供方的 ASC。留作凭据以便在 PlayerState 改换后摘除旧登记并改登记到新的。
	TWeakObjectPtr<UModularAbilitySystemComponent> RegisteredGateAbilitySystem;
};


/** 快捷栏槽位内容变化时广播的消息。 */
USTRUCT(BlueprintType)
struct FQuickBarSlotsChangedMessage
{
	GENERATED_BODY()

	/** 触发本次快捷栏变化的拥有者。 */
	UPROPERTY(BlueprintReadOnly, Category=Inventory)
	TObjectPtr<AActor> Owner = nullptr;

	/** 当前最新的全部槽位内容。 */
	UPROPERTY(BlueprintReadOnly, Category = Inventory)
	TArray<TObjectPtr<UInventoryItemInstance>> Slots;
};


/** 快捷栏激活索引变化时广播的消息。 */
USTRUCT(BlueprintType)
struct FQuickBarActiveIndexChangedMessage
{
	GENERATED_BODY()

	/** 触发本次激活索引变化的拥有者。 */
	UPROPERTY(BlueprintReadOnly, Category=Inventory)
	TObjectPtr<AActor> Owner = nullptr;

	/** 当前最新的激活槽位索引。 */
	UPROPERTY(BlueprintReadOnly, Category=Inventory)
	int32 ActiveIndex = 0;
};

#undef UE_API
