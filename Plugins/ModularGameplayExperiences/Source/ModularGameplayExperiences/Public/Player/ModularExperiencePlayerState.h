// Copyright Chronicler.


#pragma once

#include "GameplayTagStack.h"
#include "ModularPlayerState.h"
#include "DataAsset/ModularPawnData.h"
#include "GameMode/ModularExperienceDefinition.h"

#include "ModularExperiencePlayerState.generated.h"

#define UE_API MODULARGAMEPLAYEXPERIENCES_API

/** 客户端连接类型（复制）。 */
UENUM()
enum class EModularPlayerConnectionType : uint8
{
	/** 活跃玩家。 */
	Player = 0,

	/** 对局中连接的观战者。 */
	LiveSpectator,

	/** 离线回放/Demo 观战。 */
	ReplaySpectator,

	/** 已停用（断开）。 */
	InactivePlayer
};

/**
 * 支持数据驱动 PawnData 的 PlayerState，并与 Experience 加载流程协作。
 */
UCLASS(MinimalAPI, Config="Game", Blueprintable)
class AModularExperiencePlayerState : public AModularPlayerState
{
	GENERATED_BODY()

public:
	/** 构造：默认连接类型为 Player。 */
	UE_API explicit AModularExperiencePlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 模板方式获取 PawnData。 */
	template <class T>
	const T* GetPawnData() const { return Cast<T>(PawnData); }

	/** Server：设置复制用的 PawnData（幂等首写）。 */
	UE_API virtual void SetPawnData(const UModularPawnData* InPawnData);

	/**
	 * Server：显式更换 PawnData。
	 *
	 * 撤销旧 PawnData 的 PlayerState 级注入后写入新值并重新应用。与 SetPawnData 的首写语义
	 * 区分开，使初始化竞态下的重复写入仍被 SetPawnData 拦截，而换装意图必须经由本入口表达。
	 * 更换 PawnClass 后需由调用方触发重生，Pawn 级注入随旧 Pawn 销毁回收。
	 */
	UE_API virtual void ChangePawnData(const UModularPawnData* InPawnData);

	/** 获取复制到客户端的观察视角旋转（观战用）。 */
	UE_API FRotator GetReplicatedViewRotation() const;

	/** 在 Server 设置复制的观察视角旋转。 */
	UE_API void SetReplicatedViewRotation(const FRotator& NewRotation);

	/** @implements APlayerState：客户端初始化后推进 Pawn InitState。 */
	UE_API virtual void ClientInitialize(AController* Controller) override;
	UE_API virtual void PreInitializeComponents() override;
	/** 非客户端：注册 Experience 加载回调以写入 PawnData。 */
	UE_API virtual void PostInitializeComponents() override;
	/** 销毁前撤销 PlayerState 级注入，避免跨关卡残留。 */
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	// Experience 加载完成：从 GameMode 解析并 SetPawnData
	UE_API void OnExperienceLoaded(const UModularExperienceDefinition* CurrentExperience);

	UFUNCTION()
	UE_API void OnRep_PawnData();

	/** 应用当前 PawnData 中 PlayerState 与 Controller 作用域的 Fragment。 */
	UE_API void ApplyPawnDataFragments();

	/** 按逆序撤销上次应用的 PlayerState 与 Controller 作用域 Fragment。 */
	UE_API void RevokePawnDataFragments();

	/** 构造以指定 Actor 为目标的 Fragment 应用上下文。 */
	UE_API FPawnDataFragmentContext MakeFragmentContext(AActor* TargetActor) const;

	/** 撤销注入前调用，子类可在此清理依赖旧数据的运行时状态。 */
	virtual void PreRevokePawnData() {}

	/** 注入完成后调用，子类可在此广播就绪事件。 */
	virtual void PostApplyPawnData() {}

	UPROPERTY(ReplicatedUsing=OnRep_PawnData)
	TObjectPtr<const UModularPawnData> PawnData;

	/**
	 * 上一次实际应用 Fragment 所依据的 PawnData。
	 *
	 * PawnData 复制到达客户端时旧值已被覆盖，撤销必须以本字段为准。
	 */
	UPROPERTY(Transient)
	TObjectPtr<const UModularPawnData> AppliedPawnData;

	/** 与 AppliedPawnData->Fragments 等长并按索引对齐的运行时状态，未应用的位置为空。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPawnDataFragmentState>> FragmentStates;

	UPROPERTY(Replicated)
	FGameplayTagStackContainer StatTags;

private:
	UPROPERTY(Replicated)
	EModularPlayerConnectionType MyPlayerConnectionType;

	UPROPERTY(Replicated)
	FRotator ReplicatedViewRotation;
};

#undef UE_API
