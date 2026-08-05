#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "YanKinesisControllable.generated.h"

class USceneComponent;

UINTERFACE(MinimalAPI, BlueprintType, Blueprintable)
class UYanKinesisControllable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 可被念力控制的对象。
 *
 * 实现本接口即表明该 Actor 能成为念力会话的被控目标，并会在玩家屏幕上获得一个指示器。
 * 但实现接口本身不产生登记：选取与指示器扫描均查 UYanKinesisRegistrySubsystem 的登记表，
 * 而登记由 UYanKinesisTargetComponent 完成。两者职责分离——
 * 接口回答「能不能控、控哪里、被控时做什么」，组件回答「世界里有哪些可控物」。
 *
 * 组件自身也实现了本接口作为默认实现：简单物件挂上组件即可被控，无须写任何代码；
 * 需要自定义判定的 Actor（如死亡后不可控、已被他人控制时排他）自行实现本接口，
 * 其实现优先于组件的默认实现。
 */
class IYanKinesisControllable
{
	GENERATED_BODY()

public:
	/**
	 * 当前是否允许被 InInstigator 控制。
	 * 每次选取与每次指示器扫描都会调用，实现须廉价且无副作用。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Kinesis")
	bool CanBeKinesisControlled(const AActor* InInstigator) const;

	virtual bool CanBeKinesisControlled_Implementation(const AActor* InInstigator) const
	{
		return true;
	}

	/**
	 * 指示器与施力的锚点组件，决定屏幕指示器贴在模型的哪个部位。
	 * 返回空时由调用方回落到 Actor 的根组件。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Kinesis")
	USceneComponent* GetKinesisAnchorComponent() const;

	virtual USceneComponent* GetKinesisAnchorComponent_Implementation() const
	{
		return nullptr;
	}

	/** 被选为控制目标时触发，仅在发起方本地端调用，用于起手表现 */
	UFUNCTION(BlueprintNativeEvent, Category = "Kinesis")
	void OnKinesisControlBegin(AActor* InInstigator);

	virtual void OnKinesisControlBegin_Implementation(AActor* InInstigator)
	{
	}

	/** 控制结束时触发，与 OnKinesisControlBegin 成对 */
	UFUNCTION(BlueprintNativeEvent, Category = "Kinesis")
	void OnKinesisControlEnd(AActor* InInstigator);

	virtual void OnKinesisControlEnd_Implementation(AActor* InInstigator)
	{
	}
};
