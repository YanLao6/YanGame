// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/GameFrameworkInitStateInterface.h"
#include "Components/PawnComponent.h"
#include "DataAsset/ModularPawnData.h"
#include "ModularPawnComponent.generated.h"

#define UE_API MODULARGAMEPLAYEXPERIENCES_API

namespace EEndPlayReason { enum Type : int; }

/**
 * Pawn 扩展组件。
 *
 * 为 Pawn 提供统一的初始化状态编排，并持有可复制的 `UModularPawnData`。
 */
UCLASS(MinimalAPI)
class UModularPawnComponent : public UPawnComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()

public:
	/** 构造 Pawn 扩展组件。 */
	UE_API explicit UModularPawnComponent(const FObjectInitializer& ObjectInitializer);

	/** GameFrameworkComponentManager 中的 Feature 名称（与其它 InitState Feature 编排配合）。 */
	static UE_API const FName NAME_ActorFeatureName;

	//~ Begin IGameFrameworkInitStateInterface interface
	virtual FName GetFeatureName() const override { return NAME_ActorFeatureName; }
	UE_API virtual bool CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const override;
	UE_API virtual void HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) override;
	UE_API virtual void OnActorInitStateChanged(const FActorInitStateChangedParams& Params) override;
	UE_API virtual void CheckDefaultInitialization() override;
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
	UE_API void SetPawnData(const UModularPawnData* InPawnData);

	/** Pawn Possess / UnPossess 导致 Controller 变化时调用。 */
	UE_API void HandleControllerChanged();

	/** PlayerState 复制到达本地时由 Pawn 调用。 */
	UE_API void HandlePlayerStateReplicated();

	/** SetupPlayerInputComponent 完成时调用以推进 InitState。 */
	UE_API void SetupPlayerInputComponent();

protected:

	UE_API virtual void OnRegister() override;
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	UE_API void OnRep_PawnData();

	/** 应用当前 PawnData 中 Pawn 作用域的 Fragment。 */
	UE_API void ApplyPawnDataFragments();

	/** 撤销上次应用的 Pawn 作用域 Fragment。 */
	UE_API void RevokePawnDataFragments();

	/** 构造 Fragment 应用上下文，目标为本 Pawn。 */
	UE_API FPawnDataFragmentContext MakeFragmentContext() const;

	/** 生成或放置实例时指定的 PawnData（Replicated）。 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_PawnData, Category = "Kula|Pawn")
	TObjectPtr<const UModularPawnData> PawnData;

	/** 上一次实际应用 Fragment 所依据的 PawnData，撤销以本字段为准。 */
	UPROPERTY(Transient)
	TObjectPtr<const UModularPawnData> AppliedPawnData;

	/** 与 AppliedPawnData->Fragments 等长并按索引对齐的运行时状态，未应用的位置为空。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPawnDataFragmentState>> FragmentStates;
};

#undef UE_API
