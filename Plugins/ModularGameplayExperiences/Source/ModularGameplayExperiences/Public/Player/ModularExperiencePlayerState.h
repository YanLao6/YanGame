// Copyright Chronicler.


#pragma once

#include "GameplayTagStack.h"
#include "ModularPlayerState.h"
#include "DataAsset/ModularPawnData.h"
#include "GameMode/ModularExperienceDefinition.h"

#include "ModularExperiencePlayerState.generated.h"

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
UCLASS(Config="Game", Blueprintable)
class MODULARGAMEPLAYEXPERIENCES_API AModularExperiencePlayerState : public AModularPlayerState
{
	GENERATED_BODY()

public:
	/** 构造：默认连接类型为 Player。 */
	explicit AModularExperiencePlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 模板方式获取 PawnData。 */
	template <class T>
	const T* GetPawnData() const { return Cast<T>(PawnData); }

	/** Server：设置复制用的 PawnData（幂等首写）。 */
	virtual void SetPawnData(const UModularPawnData* InPawnData);

	/** 获取复制到客户端的观察视角旋转（观战用）。 */
	FRotator GetReplicatedViewRotation() const;

	/** 在 Server 设置复制的观察视角旋转。 */
	void SetReplicatedViewRotation(const FRotator& NewRotation);

	/** @implements APlayerState：客户端初始化后推进 Pawn InitState。 */
	virtual void ClientInitialize(AController* Controller) override;
	virtual void PreInitializeComponents() override;
	/** 非客户端：注册 Experience 加载回调以写入 PawnData。 */
	virtual void PostInitializeComponents() override;

protected:
	// Experience 加载完成：从 GameMode 解析并 SetPawnData
	void OnExperienceLoaded(const UModularExperienceDefinition* CurrentExperience);

	UFUNCTION()
	void OnRep_PawnData();

	UPROPERTY(ReplicatedUsing=OnRep_PawnData)
	TObjectPtr<const UModularPawnData> PawnData;

	UPROPERTY(Replicated)
	FGameplayTagStackContainer StatTags;

private:
	UPROPERTY(Replicated)
	EModularPlayerConnectionType MyPlayerConnectionType;

	UPROPERTY(Replicated)
	FRotator ReplicatedViewRotation;
};
