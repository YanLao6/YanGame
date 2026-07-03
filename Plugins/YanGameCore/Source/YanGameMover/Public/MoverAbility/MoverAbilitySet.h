#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "LayeredMoveBase.h"
#include "MovementModeTransition.h"

#include "MoverAbilitySet.generated.h"

class UMoverComponent;

/**
 * AbilitySet 用于注册 ULayeredMoveLogic 的配置条目。
 */
USTRUCT(BlueprintType)
struct FMoverAbilitySet_LayeredMove
{
	GENERATED_BODY()

	/** 要向 UMoverComponent 注册的 LayeredMoveLogic 类 */
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<ULayeredMoveLogic> LayerClass;
};

/**
 * AbilitySet 用于注册 UBaseMovementModeTransition（Check）的配置条目。
 */
USTRUCT(BlueprintType)
struct FMoverAbilitySet_ModeTransition
{
	GENERATED_BODY()

	/** 要注册的移动模式过渡条件类 */
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UBaseMovementModeTransition> TransitionClass;

	/**
	 * 目标移动模式标签（匹配 UBaseMovementMode::GameplayTags）。
	 * 留空时注册到 UMoverComponent 全局 Transitions；
	 * 填写时遍历 MovementModes，注册到第一个匹配该 Tag 的 Mode。
	 */
	UPROPERTY(EditDefaultsOnly, Meta = (Categories = "Movement.Mode"))
	FGameplayTag TargetModeTag;
};

/**
 * 内部句柄条目：记录已注册的 ModeTransition 及其所在的 Mode 名称。
 */
USTRUCT()
struct FRegisteredModeTransitionEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UBaseMovementModeTransition> Transition = nullptr;

	/** NAME_None 表示注册到 UMoverComponent 全局 Transitions */
	UPROPERTY()
	FName TargetModeName = NAME_None;
};

/**
 * 记录 MoverAbilitySet 已授予内容的句柄，用于后续统一撤销。
 */
USTRUCT(BlueprintType)
struct YANGAMEMOVER_API FMoverAbilitySet_GrantedHandles
{
	GENERATED_BODY()

public:
	/** 记录已注册的 LayeredMoveLogic 类（按类撤销） */
	void AddRegisteredLayer(TSubclassOf<ULayeredMoveLogic> LayerClass);
	/** 记录已注册的 ModeTransition 实例及其目标 Mode 名称 */
	void AddRegisteredTransition(UBaseMovementModeTransition* Transition, FName TargetModeName);

	/** 从 MoverComp 移除已注册的 Layer 与 Transition */
	void TakeFrom(UMoverComponent* MoverComp);

protected:
	UPROPERTY()
	TArray<TSubclassOf<ULayeredMoveLogic>> RegisteredLayers;

	UPROPERTY()
	TArray<FRegisteredModeTransitionEntry> RegisteredTransitions;
};

/**
 * MoverAbilitySet 数据资产。
 *
 * 以数据驱动方式批量注册 ULayeredMoveLogic 与 UBaseMovementModeTransition。
 *
 * 配合 UGameFeatureAction_AddMoverAbilities 使用：
 *   Experience 激活时 → GiveTo 注册全部条目
 *   Experience 停用时 → FMoverAbilitySet_GrantedHandles::TakeFrom 撤销
 */
UCLASS(BlueprintType, Const)
class YANGAMEMOVER_API UMoverAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UMoverAbilitySet(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * 将本 Set 注册到目标移动组件。
	 * @param MoverComp   目标移动组件
	 * @param OutHandles  可选；记录本次注册的句柄，供后续 TakeFrom 撤销
	 */
	void GiveTo(UMoverComponent* MoverComp, FMoverAbilitySet_GrantedHandles* OutHandles) const;

protected:
	/** 要向 MoverComponent 注册的分层移动逻辑列表 */
	UPROPERTY(EditDefaultsOnly, Category = "Layered Moves", meta = (TitleProperty = "LayerClass"))
	TArray<FMoverAbilitySet_LayeredMove> GrantedLayers;

	/** 要向 MoverComponent 注册的移动模式过渡条件列表（Check） */
	UPROPERTY(EditDefaultsOnly, Category = "Mode Transitions", meta = (TitleProperty = "TransitionClass"))
	TArray<FMoverAbilitySet_ModeTransition> GrantedTransitions;
};
