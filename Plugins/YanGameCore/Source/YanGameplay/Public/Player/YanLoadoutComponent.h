#pragma once

#include "Components/PlayerStateComponent.h"
#include "GameplayAbilities/ModularAbilitySet.h"

#include "YanLoadoutComponent.generated.h"

#define UE_API YANGAMEPLAY_API

class UModularAbilitySet;
class UModularAbilitySystemComponent;

/** 已选特长或已解锁槽位变化时广播，供选取界面与 HUD 刷新。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnYanLoadoutChanged);

/**
 * 一项可选特长：一个展示名加一份技能集。
 */
USTRUCT(BlueprintType)
struct FYanSpecialtyEntry
{
	GENERATED_BODY()

	/** 界面展示名。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	FText DisplayName;

	/** 选中本特长时授予的技能集。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	TObjectPtr<UModularAbilitySet> AbilitySet;
};

/**
 * 特长装配组件：开局给一个槽位，之后靠占领资源点逐个解锁，直到候选特长全部选完。
 *
 * 挂在 PlayerState 而非 Pawn，因为 ASC 本身就在 PlayerState 上——
 * 技能授予与解锁进度都不随 Pawn 死亡消失，重生后既无须重授也不回退。
 *
 * 选定即锁死，只增不换，因此授予是增量的：新选中的技能集叠加到同一份回收凭据上，
 * 不走「全部收回再重授」。后者会在每次新选时打断玩家正在生效的位移技能。
 *
 * 「还能不能选」由 UnlockedSlotCount 与已选数量之差推导，不另设开关，
 * 避免开关与实际槽位余量各说各话。
 *
 * 一切裁决在服务器，客户端的选择请求经 Server RPC 校验后复制回来。
 */
UCLASS(MinimalAPI, Blueprintable, Meta = (BlueprintSpawnableComponent))
class UYanLoadoutComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	UE_API explicit UYanLoadoutComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 全部候选特长，供选取界面遍历。 */
	UFUNCTION(BlueprintPure, Category = "Loadout")
	const TArray<FYanSpecialtyEntry>& GetAvailableSpecialties() const { return AvailableSpecialties; }

	/** 已选特长在候选表中的索引，按选定先后排列。 */
	UFUNCTION(BlueprintPure, Category = "Loadout")
	const TArray<int32>& GetSelectedIndices() const { return SelectedIndices; }

	/** 该索引的特长是否已被选中；选中后不可撤销。 */
	UFUNCTION(BlueprintPure, Category = "Loadout")
	bool IsSpecialtySelected(int32 Index) const { return SelectedIndices.Contains(Index); }

	/** 累计已解锁的槽位数：开局为 InitialSlotCount，每占领一次资源点加一。 */
	UFUNCTION(BlueprintPure, Category = "Loadout")
	int32 GetUnlockedSlotCount() const { return UnlockedSlotCount; }

	/** 已解锁但尚未使用的槽位数，界面据此决定按钮可否点击。 */
	UFUNCTION(BlueprintPure, Category = "Loadout")
	int32 GetPendingSlotCount() const { return FMath::Max(0, UnlockedSlotCount - SelectedIndices.Num()); }

	/** 是否有待使用的槽位，即是否该提示玩家去选。 */
	UFUNCTION(BlueprintPure, Category = "Loadout")
	bool HasPendingSelection() const { return GetPendingSlotCount() > 0; }

	/** 槽位已解锁到候选表上限，资源点对本玩家不再有意义。 */
	UFUNCTION(BlueprintPure, Category = "Loadout")
	bool IsFullyUnlocked() const { return UnlockedSlotCount >= AvailableSpecialties.Num(); }

	/** 当前资源点占领进度，0 到 1；只复制给拥有者。 */
	UFUNCTION(BlueprintPure, Category = "Loadout")
	float GetCaptureProgress() const { return CaptureProgress; }

	/** 请求选定一个特长，由拥有者客户端调用；服务器校验槽位余量、索引范围与是否重复后生效。 */
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	UE_API void RequestSelectSpecialty(int32 Index);

	/** 解锁一个槽位，仅服务器有效；资源点占领达标时调用。 */
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	UE_API void GrantSelectionSlot();

	/** 写入占领进度，仅服务器有效；由资源点逐帧推进。 */
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	UE_API void SetCaptureProgress(float NewProgress);

	UPROPERTY(BlueprintAssignable, Category = "Loadout")
	FOnYanLoadoutChanged OnLoadoutChanged;

	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End UActorComponent Interface

protected:
	/** 候选特长表；对战 Experience 注入本组件时配置。 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout")
	TArray<FYanSpecialtyEntry> AvailableSpecialties;

	/** 开局即可选定的槽位数。 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout", meta = (ClampMin = "0"))
	int32 InitialSlotCount = 1;

	/** 占领进度催更的步长，见 SetCaptureProgress。 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float CaptureProgressNetStep = 0.05f;

private:
	UFUNCTION(Server, Reliable)
	UE_API void ServerSelectSpecialty(int32 Index);

	UFUNCTION()
	UE_API void OnRep_LoadoutState();

	// 把单个特长的技能集叠加到 ASC，凭据累积在 GrantedHandles 上
	UE_API void GrantSpecialty(int32 Index);

	UE_API UModularAbilitySystemComponent* GetOwningAbilitySystemComponent() const;

	UPROPERTY(ReplicatedUsing = OnRep_LoadoutState)
	TArray<int32> SelectedIndices;

	UPROPERTY(ReplicatedUsing = OnRep_LoadoutState)
	int32 UnlockedSlotCount = 0;

	UPROPERTY(Replicated)
	float CaptureProgress = 0.0f;

	// 上一次催更时的进度，仅服务器持有；不参与复制
	float LastNetPushedProgress = 0.0f;

	// 已授予内容的回收凭据，仅服务器持有
	FModularAbilitySet_GrantedHandles GrantedHandles;
};

#undef UE_API
