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
#include "Components/RealtimeDestructibleMeshComponent.h"
#include "DestructionTypes.h"
#include "Components/SceneComponent.h"

#include "DestructionProjectileComponent.generated.h"
class UMaterialInterface;
class UNiagaraSystem;

//=============================================================================
// Delegates
//=============================================================================

// 当发送销毁请求时调用
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDestructionRequested, const FVector&, ImpactPoint, const FVector&, ImpactNormal);

// 当击中不可销毁对象时调用
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNonDestructibleHit, const FHitResult&, HitResult);

/**
 * 用于射弹的销毁组件。
 *
 * 将此组件附加到射弹 Actor 上以：
 * 1. 向击中的 RealtimeDestructibleMeshComponents 发送销毁请求
 * 2. 显示即时反馈贴花
 * 3. 自动销毁射弹
 *
 * [网络要求]
 * 在多人游戏中，此组件仅处理由服务器生成的射弹的销毁。
 * 仅客户端的射弹（用于视觉效果）不会触发销毁。
 *
 * 推荐模式 (客户端预测 + 服务器权威):
 * 1. 客户端：生成本地射弹（用于效果/反馈，可选）
 * 2. 客户端 -> 服务器 RPC -> 服务器：生成射弹（用于实际命中检测）
 * 3. 在服务器射弹命中时，通过 MulticastApplyOps 将销毁传播到所有客户端
 *
 * 用法:
 * 1. 将此组件添加到您的射弹蓝图中
 * 2. 配置 HoleRadius 和贴花设置
 * 3. 对于多人游戏：确保在服务器上生成射弹
 */
UCLASS(ClassGroup = (RealtimeDestruction), meta = (BlueprintSpawnableComponent))
class REALTIMEDESTRUCTION_API UDestructionProjectileComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UDestructionProjectileComponent();

#if	WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//=========================================================================
	// 销毁设置
	//=========================================================================

	/**
	 * 是否自动绑定到 OnHit 事件。
	 *
	 * true: 组件自动检测碰撞（独立使用时）
	 * false: 需要外部调用 RequestDestructionManual()
	 *        （当另一个组件或 Actor 已处理与 URealtimeDestructibleMeshComponent 相关的碰撞时）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction")
	bool bAutoBindHit = false;

	/** 孔洞半径 (cm) - 用于兼容性 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction")
	float HoleRadius = 10.0f;

	// 用于更改工具形状的变量
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Shape")
	EDestructionToolShape ToolShape = EDestructionToolShape::Cylinder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Shape|Debug")
	bool bShowToolShape = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Shape|Debug")
	bool bShowAffetedChunks = false;

	//=========================================================================
	// 圆柱体特定参数
	//=========================================================================
	/** 圆柱体半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Shape|Cylinder",
		meta = (EditCondition = "ToolShape == EDestructionToolShape::Cylinder", EditConditionHides))
	float CylinderRadius = 10.0f;

	/** 圆柱体高度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Shape|Cylinder",
		meta = (EditCondition = "ToolShape == EDestructionToolShape::Cylinder", EditConditionHides))
	float CylinderHeight = 400.0f;

	/** 圆形横截面的径向分段数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Shape|Cylinder",
		meta = (ClampMin = 3, ClampMax = 64,
			EditCondition = "ToolShape == EDestructionToolShape::Cylinder",
			EditConditionHides))
	int32 RadialSteps = 12;

	/** 高度细分数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Shape|Cylinder",
		meta = (ClampMin = 0, ClampMax = 32,
			EditCondition = "ToolShape == EDestructionToolShape::Cylinder",
			EditConditionHides))
	int32 HeightSubdivisions = 0;

	/** 圆柱体是否封顶 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Shape|Cylinder",
		meta = (EditCondition = "ToolShape == EDestructionToolShape::Cylinder",
			EditConditionHides))
	bool bCapped = true;

	//=========================================================================
	// 球体特定参数
	//=========================================================================

	/** 球体半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Shape|Sphere",
		meta = (EditCondition = "ToolShape == EDestructionToolShape::Sphere", EditConditionHides))
	float SphereRadius = 10.0f;

	/** 球体纬度细分数 (phi) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Shape|Sphere",
		meta = (ClampMin = 3, ClampMax = 128,
			EditCondition = "ToolShape == EDestructionToolShape::Sphere",
			EditConditionHides))
	int32 SphereStepsPhi = 8;

	/** 球体经度细分数 (theta) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction|Shape|Sphere",
		meta = (ClampMin = 3, ClampMax = 128,
			EditCondition = "ToolShape == EDestructionToolShape::Sphere",
			EditConditionHides))
	int32 SphereStepsTheta = 16;

	/** 碰撞后自动销毁射弹 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction")
	bool bDestroyOnHit = true;

	/** 击中不可销毁对象时也销毁射弹 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction")
	bool bDestroyOnNonDestructibleHit = true;

	//=========================================================================
	// 贴花参数
	//=========================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction|Decal")
	bool  bUseDecalSizeOverride = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction|Decal", meta = (EditCondition = "bUseDecalSizeOverride", EditConditionHides))
	FVector DecalSizeOverride = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction|Decal", meta = (EditCondition = "bUseDecalSizeOverride", EditConditionHides))
	FVector DecalLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction|Decal", meta = (EditCondition = "bUseDecalSizeOverride", EditConditionHides))
	FRotator DecalRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction|Decal", meta = (EditCondition = "!bUseDecalSizeOverride", EditConditionHides))
	float DecalSizeMultiplier = 1.0f;

	//=========================================================================
	// 贴花编辑器值存储
	//=========================================================================

	UPROPERTY()
	FVector DecalLocationInEditor = FVector::ZeroVector;

	UPROPERTY()
	FRotator DecalRotationInEditor = FRotator::ZeroRotator;

	UPROPERTY()
	FVector DecalScaleInEditor = FVector::OneVector;

	UPROPERTY()
	FVector ToolShapeLocationInEditor = FVector::ZeroVector;

	UPROPERTY()
	FRotator ToolShapeRotationInEditor = FRotator::ZeroRotator;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> DecalMaterialInEditor = nullptr;

	UPROPERTY()
	TObjectPtr<UImpactProfileDataAsset> CachedDecalDataAsset;

	UPROPERTY()
	FName DecalConfig = FName("Default");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decal")
	FName DecalConfigID = FName("Default");

	UPROPERTY()
	FName CachedConfigID;


	//=========================================================================
	// 即时反馈设置
	//=========================================================================

	/** 显示即时反馈（在服务器响应之前） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction|Feedback")
	bool bShowImmediateFeedback = true;

	//=========================================================================
	// 事件
	//=========================================================================

	/** 当发送销毁请求时调用 */
	UPROPERTY(BlueprintAssignable, Category="Destruction|Events")
	FOnDestructionRequested OnDestructionRequested;

	/** 当击中不可销毁对象时调用 */
	UPROPERTY(BlueprintAssignable, Category="Destruction|Events")
	FOnNonDestructibleHit OnNonDestructibleHit;

	//=========================================================================
	// 手动调用函数
	//=========================================================================

	/**
	 * 手动发送销毁请求。
	 * 使用此函数代替自动碰撞检测。
	 */
	UFUNCTION(BlueprintCallable, Category="Destruction")
	void RequestDestructionManual(const FHitResult& HitResult);

	UFUNCTION(BlueprintCallable, Category = "Destruction")
	void RequestDestructionAtLocation(const FVector& Center);

public:
	UFUNCTION(BlueprintCallable, Category="Destruction|Decal")
	void GetCalculateDecalSize(FName SurfaceType,FVector& LocationOffset,  FRotator& RotatorOffset, FVector& SizeOffset) const;

	/** 碰撞事件处理器 */

	// 可在蓝图中直接绑定到碰撞体 OnComponentHit 事件的函数
	UFUNCTION(BlueprintCallable, Category = "Destruction")
	bool RequestDestructionFromProjectile(UPrimitiveComponent* HitComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit, bool bDestroyProjectile = true);

	UFUNCTION(BlueprintCallable, Category = "Destruction")
	bool  RequestDestructionFromHitScan(URealtimeDestructibleMeshComponent* DestructComp, const FHitResult& Hit, bool bDestroyProjectile = false);

	UFUNCTION()
	void ProcessProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION(BlueprintCallable, Category = "Destruction")
	void ProcessSphereDestructionRequestForChunk(URealtimeDestructibleMeshComponent* DestructComp, const FVector& ExplosionCenter );

	UFUNCTION(BlueprintCallable, Category = "Destruction")
	void UpdateCachedDecalDataAssetIfNeeded();

protected:
	virtual void BeginPlay() override;

	//=========================================================================
	// 内部函数
	//=========================================================================


private:
	bool ProcessDestructionRequestForChunk(URealtimeDestructibleMeshComponent* DestructComp, const FHitResult& Hit);

	bool EnsureToolMesh();

	void SetShapeParameters(FRealtimeDestructionRequest& OutRequest);

	void DrawDebugToolShape(const FVector& Center, const FVector& Direction, const FColor& Color) const;

	void DrawDebugAffetedChunks(const FBox& ChunkBox, const FColor& Color) const;

	void DrawDebugCylinderInternal(const FVector& Center, const FVector& Direction, const FColor& Color) const;

	void DrawDebugSphereInternal(const FVector& Center, const FColor& Color) const;

	FVector GetToolDirection(const FHitResult& Hit, AActor* Owner) const;

	TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe> ToolMeshPtr = nullptr;

	bool BooleanSourceMesh(URealtimeDestructibleMeshComponent* DestructComp, const FHitResult& Hit, bool bDestroyProjectile = true);

	float SurfaceMargin = 0.0f;
};
