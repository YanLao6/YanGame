// Copyright (c) 2026 LazyDevelopers <lazydeveloper24@gmail.com>. All rights reserved.
// This plugin is distributed under the Fab Standard License.
//
// This product was independently developed by us while participating in the Epic Project, a developer-support
// program of the KRAFTON JUNGLE GameTech Lab. All rights, title, and interest in and to the product are exclusively
// vested in us. Krafton, Inc. was not involved in its development and distribution and disclaims all representations
// and warranties, express or implied, and assumes no responsibility or liability for any consequences arising from
// the use of this product.

#include "BulletClusterComponent.h"
#include "Components/RealtimeDestructibleMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UBulletClusterComponent::UBulletClusterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBulletClusterComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UBulletClusterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearPendingRequests();

	Super::EndPlay(EndPlayReason);
}

void UBulletClusterComponent::Init(float InMergeDistance, float InMaxCluserRadius, int InClusterCountThreshold, float InRaidusOffset)
{
	MergeDistanceThreshold = InMergeDistance;
	MaxClusterRadius = InMaxCluserRadius;
	ClusterCountThreshold = InClusterCountThreshold;
	ClusterRadiusOffset = InRaidusOffset;
}

void UBulletClusterComponent::SetOwnerMesh(URealtimeDestructibleMeshComponent* InOwnerMesh)
{
	OwnerMesh = InOwnerMesh;
}

void UBulletClusterComponent::RegisterRequest(const FRealtimeDestructionRequest& Request)
{
	// 添加请求
	FPendingClusteringRequest NewRequest;
	NewRequest.ImpactPoint = Request.ImpactPoint;
	NewRequest.ImpactNormal = Request.ImpactNormal;
	NewRequest.Radius = Request.ShapeParams.Radius;
	NewRequest.ChunkIndex = Request.ChunkIndex;
	NewRequest.ToolForwardVector = Request.ToolForwardVector;
	NewRequest.ToolOriginWorld = Request.ImpactPoint - (Request.ToolForwardVector * Request.ShapeParams.SurfaceMargin);
	NewRequest.Depth = (Request.ShapeParams.Height + Request.ShapeParams.SurfaceMargin) * 0.9f;
	PendingRequests.Add(NewRequest);

	// 启动计时器
	if (!bTimerActive && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			ClusterTimerHandle,
			this,
			&UBulletClusterComponent::OnClusterWindowExpired,
			ClusterWindowTime,
			false);

		bTimerActive = true;
	}

}


void UBulletClusterComponent::OnClusterWindowExpired()
{
	bTimerActive = false;

	// 如果累积的请求数未达到阈值，则初始化
	if (PendingRequests.Num() < ClusterCountThreshold)
	{
		// 清除缓冲区
		ClearPendingRequests();
		return;
	}

	//执行聚类
	TArray<FBulletCluster> Clusters = ProcessClustering();
	 
	// 破坏
	if (Clusters.Num() > 0)
	{
		ExecuteDestruction(Clusters);
	}

	// 清除缓冲区
	ClearPendingRequests();

}

TArray<FBulletCluster> UBulletClusterComponent::ProcessClustering()
{
	TArray<FBulletCluster> ResultClusters;
	int32 N = PendingRequests.Num();


	// 如果未超过阈值，则返回
	if (N < ClusterCountThreshold)
	{
		return ResultClusters;
	}

	// 并查集
	FUnionFind ClusterUF;
	ClusterUF.Init(N);

	const float CosThreshold = FMath::Cos(FMath::DegreesToRadians(15.0f));

	for (int32 i = 0; i < N; ++i)
	{
		for (int32 j = i + 1; j < N; ++j)
		{
			float Dist = FVector::Dist(
				PendingRequests[i].ImpactPoint,
				PendingRequests[j].ImpactPoint
			);

			if (Dist <= MergeDistanceThreshold)
			{
				ClusterUF.Union(i, j);
			}
		}
	}
	
	

	//聚类分组
	TMap<int32, FBulletCluster> RootToCluster;
	 
	for (int32 i = 0; i < N; ++i)
	{
		int32 Root = ClusterUF.Find(i);
		FPendingClusteringRequest& Req = PendingRequests[i];

		FBulletCluster* FoundCluster = RootToCluster.Find(Root);

		// 如果尚未注册
		if (!FoundCluster)
		{
			FBulletCluster NewCluster;
			NewCluster.Init(Req.ImpactPoint, Req.ImpactNormal, Req.ToolForwardVector, Req.ToolOriginWorld, Req.Radius, Req.ChunkIndex, Req.Depth);
			RootToCluster.Add(Root, NewCluster);
		}
		else
		{
			// 当新的子弹添加到群集时，
			// 预测生成的圆的半径大小，以决定是否将其放入。
			float PredicatedRauius = FoundCluster->PredictRadius(Req.ImpactPoint, Req.Radius);

			if (PredicatedRauius <= MaxClusterRadius)
			{
				FoundCluster->AddMember(Req.ImpactPoint, Req.ImpactNormal, Req.ToolForwardVector, Req.Radius, Req.ChunkIndex);
			}

		}
	}
	 

	for (auto& Pair : RootToCluster)
	{
		if (Pair.Value.MemberPoints.Num() < ClusterCountThreshold)
		{
			continue;
		}

		ResultClusters.Add(Pair.Value);
	}

	return ResultClusters;
}

void UBulletClusterComponent::ExecuteDestruction(const TArray<FBulletCluster>& Clusters)
{  
	URealtimeDestructibleMeshComponent* Mesh = OwnerMesh.Get();
	if (!Mesh || !IsValid(Mesh)) return;

	// 仅在服务器上执行
	if (!Mesh->GetOwner()->HasAuthority())
	{
		return;
	}
	UWorld* World = GetWorld();;
	if (!World)
	{
		return;
	}
	
	const ENetMode NetMode = World->GetNetMode();
	/*
	 *因为它在 for 循环内，所以每次迭代都会发生内存重新分配
	*这里也考虑 3 x 3 x 3 分配 27 个
	 */
	TArray<int32> AffectedChunks;
	AffectedChunks.Reserve(27);
	for (const FBulletCluster& Cluster : Clusters)
	{
		float FinalRadius = Cluster.Radius * ClusterRadiusOffset;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Clustering_FindChunks);
			Mesh->FindChunksInRadius(Cluster.Center, FinalRadius, AffectedChunks);
		}
		if (AffectedChunks.Num() == 0) continue;

		// 所有块都使用相同的中心，以保持高度一致
		for (int32 ChunkIndex : AffectedChunks)
		{
			FRealtimeDestructionRequest Request;
			Request.ImpactPoint = Cluster.Center; // 为所有块使用相同的中心
			Request.ImpactNormal = Cluster.Normal;
			Request.ToolShape = EDestructionToolShape::Cylinder;
			Request.ShapeParams.Radius = FinalRadius;
			Request.ChunkIndex = ChunkIndex;
			Request.ToolForwardVector = Cluster.AverageForwardVector;
			Request.ToolOriginWorld = Cluster.ToolOriginWorld;
			Request.ShapeParams.Height = Cluster.Depth;

			 
			Request.ToolMeshPtr = Mesh->CreateToolMeshPtrFromShapeParams(
				Request.ToolShape, Request.ShapeParams);
			  
			Mesh->ExecuteDestructionInternal(Request); 
		 

			// 在服务器上直接执行
			if (NetMode == NM_DedicatedServer || NetMode == NM_ListenServer)
			{
				//通过Multicast传播到客户端
				FRealtimeDestructionOp Op;
				Op.Request = Request;
				
				if (Mesh->bUseServerBatching)
				{
					Mesh->EnqueueForServerBatch(Op);
				}
				else
				{
					TArray<FCompactDestructionOp> CompactOps;
					CompactOps.Add(FCompactDestructionOp::Compress(Op.Request, 0));
					Mesh->MulticastApplyOpsCompact(CompactOps);
				}
			}

		}
	}

	// 安排碎片清理
	Mesh->bPendingCleanup = true;
}

void UBulletClusterComponent::ClearPendingRequests()
{
	// 同时重置计时器
	if (bTimerActive && GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ClusterTimerHandle);
		bTimerActive = false;
	}

	// 重置待处理队列
	PendingRequests.Empty();
}