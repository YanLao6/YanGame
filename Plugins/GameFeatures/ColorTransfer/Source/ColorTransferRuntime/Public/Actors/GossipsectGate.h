#pragma once

#include "GameFramework/Actor.h"
#include "ColorTransferTypes.h"

#include "GossipsectGate.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class UBoxComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * 八卦门：左半接受黑色、右半接受白色，两半都染满后变为传送门（或按 bDestroyWhenOpened 直接销毁）。
 *
 * "左右"以 Actor 的 Right 轴划分，Forward 须朝向玩家射门的一侧。
 * 开门后全部玩家站进门前的 PortalVolume 即前往下一关；
 * 只有部分玩家进入时开始倒计时，超时则重开本关（重新加载 Experience，全员回到出生点）。
 * 切关一律走无缝 ServerTravel，SteamSockets 下非无缝切关会监听失败。
 */
UCLASS(MinimalAPI)
class AGossipsectGate : public AActor, public IColorShotReceiver
{
	GENERATED_BODY()

public:
	UE_API AGossipsectGate();

	//~Begin IColorShotReceiver Interface
	UE_API virtual void ReceiveColorShot(const FColorShotHit& Shot) override;
	//~End IColorShotReceiver Interface

	//~Begin AActor Interface
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

	UFUNCTION(BlueprintPure, Category = "Yan|ColorTransfer")
	UE_API bool IsOpen() const;

protected:
	//~Begin AActor Interface
	UE_API virtual void BeginPlay() override;
	//~End AActor Interface

	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<USceneComponent> Root;

	/** 门体，承接射线 */
	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	/** 八卦纹样面板，材质参数 BlackFill / WhiteFill / PortalOpen 由门状态驱动 */
	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UStaticMeshComponent> FaceMesh;

	/** 判定"进门"的范围，只用于查询 */
	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UBoxComponent> PortalVolume;

	/** 每半边染满所需的颜料量 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "1.0"))
	float RequiredPerHalf = 30.0f;

	/** 只有部分玩家进门时，等待其余玩家的时长（秒） */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.0"))
	float EnterGracePeriod = 3.0f;

	UPROPERTY(EditInstanceOnly, Category = "Yan|ColorTransfer", Meta = (EditCondition = "!bDestroyWhenOpened"))
	TSoftObjectPtr<UWorld> NextLevel;

	/** 染满后直接销毁、让出通路，而不是变为传送门（用于关卡中途的门） */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer")
	bool bDestroyWhenOpened = false;

private:
	UFUNCTION()
	void OnRep_DyeProgress();

	// 定时检查进门情况，仅在服务器开门后运行
	void EvaluatePortal();

	void TravelToNextLevel();

	// 发起切关前停止检查，防止切图期间重复触发
	void StopPortal();

	bool IsPawnInPortal(const APawn* Pawn) const;

	UPROPERTY(ReplicatedUsing = OnRep_DyeProgress)
	float BlackProgress = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_DyeProgress)
	float WhiteProgress = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FaceMaterial;

	FTimerHandle PortalTimer;

	// 部分玩家进门后的截止时刻，负数表示未在倒计时
	double EnterDeadline = -1.0;

	bool bTravelStarted = false;
};

#undef UE_API
