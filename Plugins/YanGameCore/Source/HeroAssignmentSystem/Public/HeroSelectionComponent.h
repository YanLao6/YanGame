#pragma once

#include "Components/ControllerComponent.h"

#include "HeroSelectionComponent.generated.h"

#define UE_API HEROASSIGNMENTSYSTEM_API

class APlayerState;
class UHeroCatalog;
class UHeroSelectionBoardComponent;

/** 选择结果变化时广播，供选人界面刷新。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelectedHeroChanged, FPrimaryAssetId, SelectedHeroId);

/**
 * 大厅角色选择组件。
 *
 * 由大厅 Experience 注入到 PlayerController，对战 Experience 不注入，因此选择状态
 * 不会残留到对局中。选择经 Server RPC 上报并写入 UHeroSelectionSubsystem，
 * 该子系统位于 GameInstance 上，可跨 ServerTravel 存活，供对战关卡的
 * UHeroAssignmentComponent 读取。
 *
 * PlayerController 仅存在于服务器与其拥有者客户端，故选择请求天然是私有的；
 * 需要对其他玩家可见的部分通过复制的 SelectedHeroId 暴露。
 */
UCLASS(MinimalAPI, Blueprintable, Meta = (BlueprintSpawnableComponent))
class UHeroSelectionComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	UE_API explicit UHeroSelectionComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 供选人界面读取的角色目录。 */
	UFUNCTION(BlueprintPure, Category = "Hero")
	UHeroCatalog* GetHeroCatalog() const { return HeroCatalog; }

	/** 当前已选角色的 Id，未选择时为无效值。 */
	UFUNCTION(BlueprintPure, Category = "Hero")
	FPrimaryAssetId GetSelectedHeroId() const { return SelectedHeroId; }

	/** 已选角色在目录中的索引，未选择或目录不匹配时返回 INDEX_NONE。 */
	UFUNCTION(BlueprintPure, Category = "Hero")
	UE_API int32 GetSelectedHeroIndex() const;

	/** 请求选择角色，由拥有者客户端调用；服务器校验后生效。 */
	UFUNCTION(BlueprintCallable, Category = "Hero")
	UE_API void RequestSelectHero(FPrimaryAssetId HeroId);

	/** 按目录索引请求选择角色，供选人界面直接绑定列表项。 */
	UFUNCTION(BlueprintCallable, Category = "Hero")
	UE_API void RequestSelectHeroByIndex(int32 Index);

	/** 请求切换确认状态，由拥有者客户端调用；全员确认后服务器发起切图。 */
	UFUNCTION(BlueprintCallable, Category = "Hero")
	UE_API void RequestSetReady(bool bInReady);

	/** 本玩家是否已确认选择。 */
	UFUNCTION(BlueprintPure, Category = "Hero")
	UE_API bool IsReady() const;

	/**
	 * 目录中该索引的角色是否已被其他玩家选走。
	 *
	 * 供选人界面禁用按钮。看板缺失时一律返回 false，界面据此退化为不做互斥展示。
	 */
	UFUNCTION(BlueprintPure, Category = "Hero")
	UE_API bool IsHeroIndexTakenByOther(int32 Index) const;

	/** 全场选人看板，选人界面经其监听队友进度；大厅未注入时为 nullptr。 */
	UFUNCTION(BlueprintPure, Category = "Hero")
	UE_API UHeroSelectionBoardComponent* GetSelectionBoard() const;

	UPROPERTY(BlueprintAssignable, Category = "Hero")
	FOnSelectedHeroChanged OnSelectedHeroChanged;

	//~Begin UActorComponent Interface
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End UActorComponent Interface

protected:
	/** 本大厅可选角色目录，须与对战关卡引用同一份资产。 */
	UPROPERTY(EditDefaultsOnly, Category = "Hero")
	TObjectPtr<UHeroCatalog> HeroCatalog;

private:
	UFUNCTION(Server, Reliable)
	UE_API void ServerSelectHero(FPrimaryAssetId HeroId);

	UFUNCTION(Server, Reliable)
	UE_API void ServerSetReady(bool bInReady);

	UFUNCTION()
	UE_API void OnRep_SelectedHeroId();

	UE_API APlayerState* GetOwningPlayerState() const;

	UPROPERTY(ReplicatedUsing = OnRep_SelectedHeroId)
	FPrimaryAssetId SelectedHeroId;
};

#undef UE_API
