#pragma once

#include "Components/ControllerComponent.h"

#include "YanMinimapComponent.generated.h"

#define UE_API YANGAMEPLAY_API

/**
 * 小地图上的一个标记。
 */
USTRUCT(BlueprintType)
struct FYanMinimapMarker
{
	GENERATED_BODY()

	/**
	 * 归一化地图坐标，范围 [-1, 1]，原点为地图中心。
	 *
	 * 保持世界轴向而非屏幕轴向：X 对应世界 X（北），Y 对应世界 Y（东）。
	 * 控件需自行换轴，正北朝上的画法为 屏幕X = Y、屏幕Y = -X。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Minimap")
	FVector2D NormalizedPosition = FVector2D::ZeroVector;

	/** 该 Actor 的水平朝向（度），供绘制方向锥。 */
	UPROPERTY(BlueprintReadOnly, Category = "Minimap")
	float YawDegrees = 0.f;

	/** 队伍编号，未分队时为 -1。 */
	UPROPERTY(BlueprintReadOnly, Category = "Minimap")
	int32 TeamId = -1;

	/** 真实位置是否已越出地图范围；为 true 时 NormalizedPosition 已被夹到边界。 */
	UPROPERTY(BlueprintReadOnly, Category = "Minimap")
	bool bClampedToEdge = false;

	/** 被标记的 Actor。 */
	UPROPERTY(BlueprintReadOnly, Category = "Minimap")
	TWeakObjectPtr<AActor> Actor;
};

/**
 * 小地图数据源：把世界坐标换算成小地图控件可直接使用的归一化坐标。
 *
 * 只负责坐标换算与目标收集，不持有任何控件；地形底图由场景中的正交
 * SceneCapture2D 渲染到 RenderTarget，与本组件通过同一份地图范围配置对齐——
 * MapCenter 须等于捕获相机的水平位置，MapExtent 须等于其正交宽度的一半。
 *
 * 采用固定朝北而非随视角旋转：本作场地是记忆型的小竞技场，
 * 固定方位更利于玩家建立空间感，视角朝向另由 GetViewYawDegrees 单独提供。
 */
UCLASS(MinimalAPI, Blueprintable, Meta = (BlueprintSpawnableComponent))
class UYanMinimapComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	UE_API explicit UYanMinimapComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 本地玩家自身的标记；无 Pawn 时返回 false。 */
	UFUNCTION(BlueprintCallable, Category = "Minimap")
	UE_API bool GetSelfMarker(FYanMinimapMarker& OutMarker) const;

	/** 除自己以外全部已生成 Pawn 的标记。 */
	UFUNCTION(BlueprintCallable, Category = "Minimap")
	UE_API void GetOtherMarkers(TArray<FYanMinimapMarker>& OutMarkers) const;

	/** 本地视角的水平朝向（度），供控件绘制视野扇形。 */
	UFUNCTION(BlueprintPure, Category = "Minimap")
	UE_API float GetViewYawDegrees() const;

protected:
	/** 地图中心的世界水平坐标，须与捕获相机的水平位置一致。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap")
	FVector2D MapCenter = FVector2D::ZeroVector;

	/** 地图半宽（cm），须等于捕获相机正交宽度的一半。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap", meta = (ClampMin = "1.0"))
	float MapExtent = 2500.f;

private:
	// 由 Actor 构造标记，越界时夹到边界并置位 bClampedToEdge
	UE_API FYanMinimapMarker MakeMarker(AActor* Target) const;
};

#undef UE_API
