#pragma once

#include "GameFramework/Actor.h"

#include "ColorMovingFloor.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class UStaticMeshComponent;
class UMoverComponent;
class UChaosPathedMovementControllerComponent;
class UChaosPathedMovementMode;
class UChaosPointMovementPathPattern;
class UNetworkPhysicsSettingsComponent;

/**
 * 在摆放位置与 EndLocation 之间往返的地板，每次被机关触发移向另一端。
 *
 * 平台通过 Chaos Mover 在物理仿真中沿路径运动，触发请求按物理帧同步。
 */
UCLASS(MinimalAPI)
class AColorMovingFloor : public AActor
{
	GENERATED_BODY()

public:
	UE_API AColorMovingFloor(const FObjectInitializer& ObjectInitializer);

	/** 移向另一端，移动途中调用无效。仅在服务器上调用。 */
	UE_API void AuthStartMoving();

	//~Begin AActor Interface
	UE_API virtual void PreInitializeComponents() override;
	//~End AActor Interface

protected:
	//~Begin AActor Interface
	UE_API virtual void BeginPlay() override;
	//~End AActor Interface

	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UMoverComponent> PlatformMover;

	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UChaosPathedMovementControllerComponent> MovementController;

	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UNetworkPhysicsSettingsComponent> NetworkPhysicsSettings;

	/** 终点的世界坐标 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer")
	FVector EndLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.1"))
	float MoveDuration = 3.0f;

private:
	bool IsMoving() const;

	UFUNCTION()
	void OnPathStarted();

	// 路径配置在组件初始化前确定，仿真期间不再修改。
	void ConfigureMovementPath();

	UPROPERTY()
	TObjectPtr<UChaosPathedMovementMode> PathMode;

	UPROPERTY()
	TObjectPtr<UChaosPointMovementPathPattern> PointPattern;

	bool bMovingToEnd = false;

	bool bStartRequestPending = false;
};

#undef UE_API
