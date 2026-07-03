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
#include "StructuralIntegrity/StructuralIntegrityTypes.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "ProceduralMeshComponent.h"
#include "DebrisTypes.generated.h"

//////////////////////////////////////////////////////////////////////////
// Debris 枚举
//////////////////////////////////////////////////////////////////////////

/**
 * Debris 类型分类
 */
UENUM(BlueprintType)
enum class EDebrisType : uint8
{
	Cosmetic,    // 仅本地，生命周期短，不影响 Gameplay
	Gameplay     // Server 权威，网络复制，启用物理交互
};

/**
 * Debris 体积等级
 */
UENUM(BlueprintType)
enum class EDebrisTier : uint8
{
	Tiny,      // < 100 cm³ - 替换为粒子效果
	Small,     // 100-500 cm³ - Sphere 碰撞
	Medium,    // 500-2000 cm³ - Box 碰撞
	Large,     // 2000-10000 cm³ - Convex Hull
	Massive    // > 10000 cm³ - Complex 碰撞
};

//////////////////////////////////////////////////////////////////////////
// Debris 结构体
//////////////////////////////////////////////////////////////////////////

/**
 * 每个等级的 Debris 配置
 */
USTRUCT(BlueprintType)
struct REALTIMEDESTRUCTION_API FDebrisTierConfig
{
	GENERATED_BODY()

	// 本等级的体积上限（cm³）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DebrisTier")
	float VolumeThreshold = 0.0f;

	// Debris 生命周期（秒，0 为永久）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DebrisTier")
	float Lifespan = 3.0f;

	// 最大 Debris 数量
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DebrisTier", meta = (ClampMin = "1"))
	int32 MaxCount = 50;

	// 是否为纯视觉（false = Gameplay 类型，网络复制）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DebrisTier")
	bool bIsCosmetic = true;

	FDebrisTierConfig() = default;

	FDebrisTierConfig(float InVolumeThreshold, float InLifespan, int32 InMaxCount, bool bInIsCosmetic)
		: VolumeThreshold(InVolumeThreshold)
		, Lifespan(InLifespan)
		, MaxCount(InMaxCount)
		, bIsCosmetic(bInIsCosmetic)
	{
	}
};

/**
 * Debris 生成配置
 */
USTRUCT(BlueprintType)
struct REALTIMEDESTRUCTION_API FDebrisSpawnSettings
{
	GENERATED_BODY()

	// 是否启用 Debris 生成
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris")
	bool bEnableDebrisSpawn = true;

	// Gameplay Debris 体积阈值（cm³），超过此值视为 Gameplay 类型
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris")
	float GameplayVolumeThreshold = 2000.0f;

	// 纯视觉 Debris 默认生命周期（秒）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris|Cosmetic")
	float CosmeticLifespan = 3.0f;

	// 最大纯视觉 Debris 数量
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris|Cosmetic", meta = (ClampMin = "1"))
	int32 MaxCosmeticDebris = 50;

	// Gameplay Debris 默认生命周期（秒，0 为永久）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris|Gameplay")
	float GameplayLifespan = 0.0f;

	// 最大 Gameplay Debris 数量
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris|Gameplay", meta = (ClampMin = "1"))
	int32 MaxGameplayDebris = 20;

	// Debris 初始水平冲量强度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris|Physics")
	float InitialImpulseHorizontal = 100.0f;

	// Debris 初始垂直冲量强度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris|Physics")
	float InitialImpulseVertical = 150.0f;

	// 根据体积判断 Debris 类型
	EDebrisType GetDebrisType(float Volume) const
	{
		return (Volume <= GameplayVolumeThreshold) ? EDebrisType::Cosmetic : EDebrisType::Gameplay;
	}

	// 根据类型返回生命周期
	float GetLifespanForType(EDebrisType Type) const
	{
		return (Type == EDebrisType::Cosmetic) ? CosmeticLifespan : GameplayLifespan;
	}

	// 根据类型返回最大数量
	int32 GetMaxCountForType(EDebrisType Type) const
	{
		return (Type == EDebrisType::Cosmetic) ? MaxCosmeticDebris : MaxGameplayDebris;
	}
};

/**
 * 压缩的 Debris 同步操作（用于网络传输）
 * 遵循现有 FCompactDestructionOp 的模式
 */
USTRUCT()
struct REALTIMEDESTRUCTION_API FCompactDebrisOp
{
	GENERATED_BODY()

	// 打包的 Cell Key 数组：(ChunkId << 16) | CellId
	UPROPERTY()
	TArray<int32> PackedCellKeys;

	// 组 ID
	UPROPERTY()
	int32 GroupId = INDEX_NONE;

	// 质心位置（1cm 精度）
	UPROPERTY()
	FVector_NetQuantize CenterOfMass;

	// 近似体积（cm³，压缩传输）
	UPROPERTY()
	float ApproximateVolume = 0.0f;

	// 序列号
	UPROPERTY()
	uint16 Sequence = 0;

	FCompactDebrisOp() = default;

	// 打包 Cell Key
	static void PackCellKeys(const TArray<FCellKey>& Keys, TArray<int32>& OutPacked)
	{
		OutPacked.Reset();
		OutPacked.Reserve(Keys.Num());
		for (const FCellKey& Key : Keys)
		{
			OutPacked.Add((Key.ChunkId << 16) | (Key.CellId & 0xFFFF));
		}
	}

	// 解包 Cell Key
	static void UnpackCellKeys(const TArray<int32>& Packed, TArray<FCellKey>& OutKeys)
	{
		OutKeys.Reset();
		OutKeys.Reserve(Packed.Num());
		for (int32 P : Packed)
		{
			OutKeys.Add(FCellKey(P >> 16, P & 0xFFFF));
		}
	}

	// 从 FDetachedCellGroup 创建
	static FCompactDebrisOp FromDetachedGroup(const FDetachedCellGroup& Group, uint16 InSequence)
	{
		FCompactDebrisOp Op;
		PackCellKeys(Group.CellKeys, Op.PackedCellKeys);
		Op.GroupId = Group.GroupId;
		Op.CenterOfMass = Group.CenterOfMass;
		Op.ApproximateVolume = Group.ApproximateMass;
		Op.Sequence = InSequence;
		return Op;
	}

	// 还原为 FDetachedCellGroup
	FDetachedCellGroup ToDetachedGroup() const
	{
		FDetachedCellGroup Group;
		Group.GroupId = GroupId;
		UnpackCellKeys(PackedCellKeys, Group.CellKeys);
		Group.CenterOfMass = CenterOfMass;
		Group.ApproximateMass = ApproximateVolume;
		return Group;
	}
};

//////////////////////////////////////////////////////////////////////////
// PMC Debris 追踪
//////////////////////////////////////////////////////////////////////////

/**
 * Gameplay Debris 追踪数据（用于 PMC → DMC 转换）
 *
 * UDynamicMeshComponent 仅支持 TriMesh（Complex）碰撞，无法进行动态物理模拟。
 * 因此物理模拟阶段使用 UProceduralMeshComponent（PMC），
 * 稳定后转换为 UDynamicMeshComponent（DMC）以支持二次破坏。
 */
struct REALTIMEDESTRUCTION_API FGameplayDebrisTracker
{
	/** PMC 组件（用于物理模拟） */
	TWeakObjectPtr<UProceduralMeshComponent> PMC;

	/** 原始网格数据（为 DMC 转换而保留） */
	TSharedPtr<UE::Geometry::FDynamicMesh3> OriginalMesh;

	/** 稳定状态持续时间（秒） */
	float StableTime = 0.0f;

	/** Debris 类型 */
	EDebrisType DebrisType = EDebrisType::Gameplay;

	/** 稳定判定速度阈值（cm/s） */
	static constexpr float StableVelocityThreshold = 5.0f;

	/** 所需稳定持续时间（秒） */
	static constexpr float StableTimeRequired = 0.5f;

	FGameplayDebrisTracker() = default;

	FGameplayDebrisTracker(UProceduralMeshComponent* InPMC, TSharedPtr<UE::Geometry::FDynamicMesh3> InMesh, EDebrisType InType = EDebrisType::Gameplay)
		: PMC(InPMC)
		, OriginalMesh(InMesh)
		, StableTime(0.0f)
		, DebrisType(InType)
	{
	}

	/** 检查 PMC 是否有效 */
	bool IsValid() const { return PMC.IsValid(); }

	/** 是否已完成稳定判定 */
	bool IsStabilized() const { return StableTime >= StableTimeRequired; }

	/** 重置稳定计时器 */
	void ResetStableTime() { StableTime = 0.0f; }

	/** 累加稳定时间 */
	void AccumulateStableTime(float DeltaTime) { StableTime += DeltaTime; }
};
