#pragma once

#include "GameFramework/Actor.h"
#include "ColorTransferTypes.h"

#include "ColorCloverDoor.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/** 四叶草的叶片数，按左上、右上、左下、右下排列 */
constexpr int32 ColorCloverLeafCount = 4;

/**
 * 四叶草门：四片叶子各自只接受一种颜色，从面向门的玩家看去为左上白、右上蓝、左下红、右下黑。
 *
 * 命中点按 Actor 局部坐标划分象限：Forward 须朝向玩家射门的一侧，玩家看到的左侧为 +Y，上方为 +Z。
 * 四片都染满后全员无缝切往 NextLevel，不需要走进门；未设置 NextLevel 时销毁自身。
 */
UCLASS(MinimalAPI)
class AColorCloverDoor : public AActor, public IColorShotReceiver
{
	GENERATED_BODY()

public:
	UE_API AColorCloverDoor();

	//~Begin IColorShotReceiver Interface
	UE_API virtual void ReceiveColorShot(const FColorShotHit& Shot) override;
	//~End IColorShotReceiver Interface

	//~Begin AActor Interface
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

	UFUNCTION(BlueprintPure, Category = "Yan|ColorTransfer")
	UE_API bool IsFilled() const;

protected:
	//~Begin AActor Interface
	UE_API virtual void BeginPlay() override;
	//~End AActor Interface

	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<USceneComponent> Root;

	/** 门体，承接射线 */
	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	/** 四叶草纹样面板，材质参数 LeafColor0..3 / LeafFill0..3 按左上、右上、左下、右下排列 */
	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UStaticMeshComponent> FaceMesh;

	/** 每片叶子染满所需的颜料量 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "1.0"))
	float RequiredPerLeaf = 50.0f;

	UPROPERTY(EditInstanceOnly, Category = "Yan|ColorTransfer")
	TSoftObjectPtr<UWorld> NextLevel;

private:
	UFUNCTION()
	void OnRep_LeafProgress();

	void TravelToNextLevel();

	UPROPERTY(ReplicatedUsing = OnRep_LeafProgress)
	float LeafProgress[ColorCloverLeafCount] = {};

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FaceMaterial;

	FTimerHandle TravelTimer;
};

#undef UE_API
