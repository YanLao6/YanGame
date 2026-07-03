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
#include "DestructionTypes.h"
#include "BulletClusterComponent.generated.h"

class URealtimeDestructibleMeshComponent;

USTRUCT()
struct FPendingClusteringRequest
{
	GENERATED_BODY()

	UPROPERTY()
	FVector ImpactPoint = FVector::ZeroVector;

	UPROPERTY()
	FVector ImpactNormal = FVector::UpVector;

	UPROPERTY()
	float Radius = 10.0f;

	UPROPERTY()
	float ChunkIndex = INDEX_NONE;

	UPROPERTY()
	FVector ToolForwardVector = FVector::ForwardVector;

	UPROPERTY()
	FVector ToolOriginWorld = FVector::ZeroVector;

	UPROPERTY()
	float Depth = 10.0f;
};

/**
 * 子弹碰撞请求聚类组件，用于优化破坏操作的批处理效率。
 *
 * 收集短时间窗口内发生的多次子弹碰撞，将相邻碰撞点归并为簇，
 * 作为一次较大破坏而非多个小孔洞处理，以减少 Boolean 操作次数并提升性能。
 *
 * [工作原理]
 * 1. 破坏请求通过 RegisterRequest() 存入待处理缓冲区
 * 2. 从首个请求起收集 ClusterWindowTime（默认 0.3 秒）内的请求
 * 3. 时间窗口到期后，使用 Union-Find 算法对相邻碰撞点进行聚类
 * 4. 仅处理达到 ClusterCountThreshold（默认 5）数量的簇
 * 5. 调用拥有者网格的 ApplyOp() 执行实际破坏
 *
 * [使用方式]
 * 当 RealtimeDestructibleMeshComponent 启用 BulletClustering 时，
 * 本组件会被自动创建并附加。手动使用时，调用 SetOwnerMesh() 指定拥有者。
 */
UCLASS(ClassGroup = (RealtimeDestruction), meta = (BlueprintSpawnableComponent))
class UBulletClusterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBulletClusterComponent();

	// ===============================================
	// 属性
	// ===============================================

	// 聚类时间窗口（秒）
	UPROPERTY()
	float ClusterWindowTime = 0.3f;

	UPROPERTY()
	float MergeDistanceThreshold = 10.0f;

	UPROPERTY()
	float MaxClusterRadius = 20.0f;

	UPROPERTY()
	int ClusterCountThreshold = 5;

	UPROPERTY()
	float ClusterRadiusOffset = 1.0f;

	// ===============================================
	// 方法
	// ===============================================
	/** 初始化聚类参数 */
	void Init(float InMergeDistance, float InMaxCluserRadius, int InClusterCountThreshold, float InClusterRadiusOffset);

	/** 设置拥有者网格组件 */
	void SetOwnerMesh(URealtimeDestructibleMeshComponent* InOwnerMesh);

	/** 注册破坏请求到待处理缓冲区 */
	UFUNCTION()
	void RegisterRequest(const FRealtimeDestructionRequest& Request);

protected:
	//~Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End UActorComponent Interface

private:
	// 拥有者网格组件
	UPROPERTY()
	TWeakObjectPtr<URealtimeDestructibleMeshComponent> OwnerMesh;

	// 待处理的聚类请求缓冲区
	UPROPERTY()
	TArray<FPendingClusteringRequest> PendingRequests;

	// 聚类时间窗口定时器
	FTimerHandle ClusterTimerHandle;
	bool bTimerActive = false;

	// 时间窗口到期回调
	void OnClusterWindowExpired();

	// 执行聚类算法，返回聚类结果
	TArray<FBulletCluster> ProcessClustering();

	// 根据聚类结果执行破坏操作
	void ExecuteDestruction(const TArray<FBulletCluster>& Clusters);

	// 清空待处理请求缓冲区
	void ClearPendingRequests();
};
