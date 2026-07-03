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
#include "GameFramework/Actor.h"
#include "DebrisActor.generated.h"

// 前向声明
namespace UE { namespace Geometry { class FDynamicMesh3; } }
class UProceduralMeshComponent;
class URealtimeDestructibleMeshComponent;
class UBoxComponent;

UCLASS()
class REALTIMEDESTRUCTION_API ADebrisActor : public AActor
{
	GENERATED_BODY()

public:
	ADebrisActor();

	// 组件层级：BoxComponent 作为根节点（负责物理），ProceduralMesh 仅用于渲染
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debris")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debris")
	TObjectPtr<UProceduralMeshComponent> DebrisMesh;

	// 网络复制属性

	/** 用于客户端匹配的唯一 Debris ID */
	UPROPERTY(ReplicatedUsing = OnRep_DebrisParams)
	int32 DebrisId;

	/** 本地 CellId 列表（不复制，存储解码结果） */
	UPROPERTY()
	TArray<int32> CellIds;

	/** 压缩的单元包围盒最小值（用于网络复制） */
	UPROPERTY(Replicated)
	FIntVector CellBoundsMin;

	/** 压缩的单元包围盒最大值（用于网络复制） */
	UPROPERTY(Replicated)
	FIntVector CellBoundsMax;

	/** 压缩的单元位图（用于网络复制） */
	UPROPERTY(Replicated)
	TArray<uint8> CellBitmap;

	/** 源网格的拥有者 Actor（从 CellID 生成 Debris 时必需） */
	UPROPERTY(Replicated)
	TObjectPtr<AActor> SourceMeshOwner;

	/** 源 Chunk 索引 */
	UPROPERTY(Replicated)
	int32 SourceChunkIndex;

	/** Debris 材质 */
	UPROPERTY(Replicated)
	TObjectPtr<UMaterialInterface> DebrisMaterial;

	// 配置
	UPROPERTY(EditDefaultsOnly, Category = "Debris")
	float DebrisLifetime;


	// 公开方法

	/** 仅 Server 调用：初始化 Debris */
	void InitializeDebris(int32 InDebrisId, const TArray<int32>& InCellIds, int32 InChunkIndex, URealtimeDestructibleMeshComponent* InSourcMesh, UMaterialInterface* InMaterial);

	/** 仅 Server 调用：启用物理模拟 */
	void EnablePhysics();

	/** 应用客户端本地网格数据 */
	void ApplyLocalMesh(UProceduralMeshComponent* LocalMesh);

	/** 设置碰撞盒的半径范围 */
	void SetCollisionBoxExtent(const FVector& Extent);

	/** 仅 Server 调用：直接使用预生成的网格数据设置 Mesh */
	void SetMeshDirectly(const TArray<FVector>& Vertices,
		const TArray<int32>& Triangles,
		const TArray<FVector>& Normals,
		const TArray<FVector2D>& UVs);

protected:
	//~Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

	UFUNCTION()
	void OnRep_DebrisParams();

private:
	// 根据 DebrisId 查找本地网格组件
	UProceduralMeshComponent* FindLocalDebrisMesh(int32 InDebrisId);

	// 从 CellId 生成网格（降级回退路径）
	void GenerateMeshFromCells();

	// 从 SourceMeshOwner 获取可破坏网格组件
	URealtimeDestructibleMeshComponent* GetSourceMeshComponent() const;

	// 生命周期到期回调
	void OnLifetimeExpired();

	// 将 CellId 编码为位图（Server 调用）
	void EncodeCellsToBitmap(const TArray<int32>& InCellIds, const struct FGridCellLayout& GridLayout);

	// 将位图解码为 CellId（Client 调用）
	void DecodeBitmapToCells(const struct FGridCellLayout& GridLayout);

	bool bMeshReady;
};