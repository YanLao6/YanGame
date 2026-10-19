#pragma once

#include "GameFramework/Actor.h"

#include "ColorExecutionZone.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class UBoxComponent;

/**
 * 处死区域：角色进入即处死，不判断颜料颜色，供关卡按需摆放（陷阱、深渊、处刑台等）。
 *
 * 判定只在服务器上定时轮询区域内的角色，经 UColorPigmentRulesComponent::AuthRequestDeath
 * 走既有死亡流程；该流程一次死亡只裁决一次，本 Actor 不需要自行去重。
 */
UCLASS(MinimalAPI)
class AColorExecutionZone : public AActor
{
	GENERATED_BODY()

public:
	UE_API AColorExecutionZone();

protected:
	//~Begin AActor Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End AActor Interface

	/** 处死判定范围，摆放时按需调整位置与缩放 */
	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UBoxComponent> ExecutionVolume;

	/** 服务器结算区域内角色的间隔 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.01", ForceUnits = "s"))
	float UpdateInterval = 0.1f;

private:
	// 服务器定时结算区域内的角色
	void AuthUpdatePawnsInVolume();

	FTimerHandle UpdateTimer;
};

#undef UE_API
