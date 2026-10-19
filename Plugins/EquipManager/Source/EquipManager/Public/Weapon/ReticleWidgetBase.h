// 武器准星控件基类声明。

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "ReticleWidgetBase.generated.h"

#define UE_API EQUIPMANAGER_API

class UInventoryItemInstance;
class UWeaponInstance;

/**
 * 武器准星控件基类。
 *
 * 由武器的准星配置片段指定具体子类，装备武器时绑定到对应的武器实例，
 * 向蓝图暴露散布角度与首发精准状态，供准星表现读取。
 */
UCLASS(MinimalAPI, Abstract)
class UReticleWidgetBase : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UE_API explicit UReticleWidgetBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 武器绑定完成后触发，供蓝图初始化准星表现。 */
	UFUNCTION(BlueprintImplementableEvent)
	void OnWeaponInitialized();

	/** 绑定到指定武器实例，并同步解析其来源物品实例。 */
	UFUNCTION(BlueprintCallable)
	UE_API void InitializeFromWeapon(UWeaponInstance* InWeapon);

	/** 返回当前武器的散布夹角，单位为度；非远程武器返回 0。 */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	UE_API float ComputeSpreadAngle() const;

	/** 返回散布在屏幕空间的最大半径，单位为像素。 */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	UE_API float ComputeMaxScreenspaceSpreadRadius() const;

	/** 判断当前武器是否处于首发精准状态。 */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	UE_API bool HasFirstShotAccuracy() const;

protected:
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UWeaponInstance> WeaponInstance;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UInventoryItemInstance> InventoryInstance;
};

#undef UE_API
