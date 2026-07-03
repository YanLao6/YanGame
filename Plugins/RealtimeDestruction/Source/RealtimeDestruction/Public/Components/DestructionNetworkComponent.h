// Copyright (c) 2026 LazyDevelopers <lazydeveloper24@gmail.com>. All rights reserved.
// This plugin is distributed under the Fab Standard License.
//
// This product was independently developed by us while participating in the Epic Project, a developer-support
// program of the KRAFTON JUNGLE GameTech Lab. All rights, title, and interest in and to the product are exclusively
// vested in us. Krafton, Inc. was not involved in its development and distribution and disclaims all representations
// and warranties, express or implied, and assumes no responsibility or liability for any consequences arising from
// the use of this product.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RealtimeDestructibleMeshComponent.h"
#include "DestructionNetworkComponent.generated.h"

/**
 * 将破坏请求转发到 Server 的网络组件。
 *
 * 将此组件添加到 PlayerController 后，
 * DestructionProjectileComponent 将自动查找并使用它。
 *
 * 使用示例：
 * 1. 打开 BP_PlayerController
 * 2. 添加组件 -> DestructionNetworkComponent
 * 3. 完成！
 */
UCLASS(ClassGroup=(RealtimeDestruction), meta=(BlueprintSpawnableComponent, DisplayName="Destruction Network"))
class REALTIMEDESTRUCTION_API UDestructionNetworkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDestructionNetworkComponent();

	/**
	 * 将破坏请求转发到 Server，由 DestructionProjectileComponent 自动调用。
	 *
	 * @param DestructComp - 目标可破坏网格组件
	 * @param Request - 破坏请求信息（位置、法线、半径）
	 */
	UFUNCTION(BlueprintCallable, Category="Destruction")
	void RequestDestruction(URealtimeDestructibleMeshComponent* DestructComp, const FRealtimeDestructionRequest& Request);

protected:
	//~Begin UActorComponent Interface
	virtual void BeginPlay() override;
	//~End UActorComponent Interface

	/**
	 * 在 Server 处理破坏（Server RPC）- 旧版方法
	 * 由 Client 调用，在 Server 执行
	 */
	UFUNCTION(Server, Reliable)
	void ServerApplyDestruction(URealtimeDestructibleMeshComponent* DestructComp, const FRealtimeDestructionRequest& Request);

	/**
	 * 在 Server 处理破坏（Server RPC）- 压缩版方法
	 * 网络带宽减少约 65%
	 */
	UFUNCTION(Server, Reliable)
	void ServerApplyDestructionCompact(URealtimeDestructibleMeshComponent* DestructComp, const FCompactDestructionOp& CompactOp);

	/**
	 * 验证破坏请求（在 Server 调用）
	 * 调用 RealtimeDestructibleMeshComponent 的 ValidateDestructionRequest
	 */
	bool ValidateDestructionRequest(
		URealtimeDestructibleMeshComponent* DestructComp,
		const FRealtimeDestructionRequest& Request,
		EDestructionRejectReason& OutReason) const;

protected:
	/** 最大允许破坏半径（反作弊） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction|Validation")
	float MaxAllowedRadius = 100.0f;

	/** 是否启用请求验证 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction|Validation")
	bool bEnableValidation = true;

	/**
	 * 是否使用压缩网络数据
	 * true：使用 FCompactDestructionOp（11 字节）
	 * false：使用 FRealtimeDestructionRequest（32+ 字节）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction|Network")
	bool bUseCompactData = true;

private:
	/** 序列号计数器（用于压缩数据） */
	int32 LocalSequence = 0;
};
