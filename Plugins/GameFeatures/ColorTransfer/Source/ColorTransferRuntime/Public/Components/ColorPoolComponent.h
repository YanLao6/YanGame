#pragma once

#include "Components/BoxComponent.h"
#include "ColorTransferTypes.h"

#include "ColorPoolComponent.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

/**
 * 取色池：盒子范围内的角色每秒被注入一次颜料。
 *
 * 挂在水体蓝图上使用，盒子本身不开碰撞、只作查询范围，从而不依赖角色胶囊的重叠事件设置。
 * 只在服务器上结算，结果随颜料属性集复制。
 */
UCLASS(MinimalAPI, ClassGroup = "Yan|ColorTransfer", Meta = (BlueprintSpawnableComponent))
class UColorPoolComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UE_API explicit UColorPoolComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End UActorComponent Interface

protected:
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer")
	EColorPigment Pigment = EColorPigment::Black;

	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.0"))
	float AmountPerSecond = 10.0f;

private:
	void InjectPawnsInVolume();

	FTimerHandle InjectTimer;
};

#undef UE_API
