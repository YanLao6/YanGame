#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Kinesis/YanKinesisControllable.h"

#include "YanKinesisTargetComponent.generated.h"

#define UE_API YANGAMEMOVER_API

class UUserWidget;

/**
 * 念力可控标记组件：挂上即成为念力的合法目标。
 *
 * 承担两件事——向 UYanKinesisRegistrySubsystem 登记自身，以及为未实现
 * IYanKinesisControllable 的 Actor 提供一份默认实现。故简单物件挂上本组件即可被抓取，
 * 而需要自定义判定的 Actor（死亡后不可控、同一时刻只允许一人控制等）
 * 自行实现接口，其实现优先于本组件。
 */
UCLASS(MinimalAPI, ClassGroup = (Kinesis), Meta = (BlueprintSpawnableComponent))
class UYanKinesisTargetComponent : public UActorComponent, public IYanKinesisControllable
{
	GENERATED_BODY()

public:
	UE_API UYanKinesisTargetComponent();

	/** 总开关：关闭后既不能被选中，也不再显示屏幕指示器 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinesis")
	bool bKinesisEnabled = true;

	/** 自身被抓取的距离上限（cm），0 表示不额外限制，仅受施法者的能力半径约束 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinesis", Meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxControlDistance = 0.f;

	/** 指示器与施力的锚点，留空则回落 Owner 根组件 */
	UPROPERTY(EditAnywhere, Category = "Kinesis")
	FComponentReference AnchorComponent;

	/** 本目标专用的指示器 Widget，留空则用指示器组件配置的默认类 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kinesis")
	TSoftClassPtr<UUserWidget> IndicatorWidgetClass;

	/**
	 * 当前是否允许被 InInstigator 控制。
	 * Owner 实现了 IYanKinesisControllable 时以其判定为准，否则回落本组件的默认实现。
	 */
	UE_API bool IsControllableBy(const AActor* InInstigator) const;

	/** 解析锚点组件：Owner 接口实现 → 本组件配置 → Owner 根组件 */
	UE_API USceneComponent* ResolveAnchorComponent() const;

	/** 转发控制起止到接口实现方，调用方无须关心该实现落在 Owner 还是本组件上 */
	UE_API void NotifyControlBegin(AActor* InInstigator);
	UE_API void NotifyControlEnd(AActor* InInstigator);

protected:
	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End UActorComponent Interface

	//~Begin IYanKinesisControllable Interface
	UE_API virtual bool CanBeKinesisControlled_Implementation(const AActor* InInstigator) const override;
	UE_API virtual USceneComponent* GetKinesisAnchorComponent_Implementation() const override;
	//~End IYanKinesisControllable Interface

private:
	// 接口实现方：Owner 实现了接口则为 Owner，否则为本组件自身
	UE_API UObject* ResolveInterfaceProvider() const;
};

#undef UE_API
