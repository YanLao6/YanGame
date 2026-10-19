// 武器界面基类声明。

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "WeaponUserInterface.generated.h"

#define UE_API EQUIPMANAGER_API

class UWeaponInstance;
struct FGeometry;

/**
 * 武器界面基类。
 *
 * 轮询本地玩家当前装备的武器实例，在武器更换时通知蓝图重建界面内容。
 */
UCLASS(MinimalAPI)
class UWeaponUserInterface : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UE_API explicit UWeaponUserInterface(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UUserWidget Interface
	UE_API virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~End UUserWidget Interface

	/** 当前武器发生变化时触发。 */
	UFUNCTION(BlueprintImplementableEvent)
	void OnWeaponChanged(UWeaponInstance* OldWeapon, UWeaponInstance* NewWeapon);

private:
	UPROPERTY(Transient)
	TObjectPtr<UWeaponInstance> CurrentInstance;
};

#undef UE_API
