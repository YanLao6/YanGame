// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/GameFrameworkInitStateInterface.h"
#include "Components/PawnComponent.h"
#include "DataAsset/ModularPawnData.h"
#include "ModularPawnComponent.generated.h"

namespace EEndPlayReason { enum Type : int; }

/**
 * Pawn 扩展组件。
 *
 * 为 Pawn 提供统一的初始化状态编排，并持有可复制的 `UModularPawnData`。
 */
UCLASS()
class MODULARGAMEPLAYEXPERIENCES_API UModularPawnComponent : public UPawnComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()

public:
	/** 构造 Pawn 扩展组件。 */
	explicit UModularPawnComponent(const FObjectInitializer& ObjectInitializer);

	/** GameFrameworkComponentManager 中的 Feature 名称（与其它 InitState Feature 编排配合）。 */
	static const FName NAME_ActorFeatureName;

	//~ Begin IGameFrameworkInitStateInterface interface
	virtual FName GetFeatureName() const override { return NAME_ActorFeatureName; }
	virtual bool CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const override;
	virtual void HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) override;
	virtual void OnActorInitStateChanged(const FActorInitStateChangedParams& Params) override;
	virtual void CheckDefaultInitialization() override;
	//~ End IGameFrameworkInitStateInterface interface

	/** 在指定 Actor 上查找 UModularPawnComponent（若存在）。 */
	UFUNCTION(BlueprintPure, Category = "Pawn")
	static UModularPawnComponent* FindModularPawnComponent(const AActor* Actor)
	{
		return (Actor ? Actor->FindComponentByClass<UModularPawnComponent>() : nullptr);
	}

	/** 以模板类型获取 PawnData（数据驱动属性来源）。 */
	template <class T>
	const T* GetPawnData() const { return Cast<T>(PawnData); }

	/** 设置当前 PawnData（通常在生成或初始化阶段调用）。 */
	void SetPawnData(const UModularPawnData* InPawnData);

	/** Pawn Possess / UnPossess 导致 Controller 变化时调用。 */
	void HandleControllerChanged();

	/** PlayerState 复制到达本地时由 Pawn 调用。 */
	void HandlePlayerStateReplicated();

	/** SetupPlayerInputComponent 完成时调用以推进 InitState。 */
	void SetupPlayerInputComponent();

protected:

	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnRep_PawnData();

	/** 生成或放置实例时指定的 PawnData（Replicated）。 */
	UPROPERTY(EditInstanceOnly, ReplicatedUsing = OnRep_PawnData, Category = "Kula|Pawn")
	TObjectPtr<const UModularPawnData> PawnData;
};
