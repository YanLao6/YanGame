// 武器界面基类实现。

#include "Weapon/WeaponUserInterface.h"

#include "Equipment/EquipmentManagerComponent.h"
#include "GameFramework/Pawn.h"
#include "Weapon/WeaponInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WeaponUserInterface)

UWeaponUserInterface::UWeaponUserInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWeaponUserInterface::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 装备变化没有对外的通知委托，此处按帧比对当前武器实例
	if (const APawn* Pawn = GetOwningPlayerPawn())
	{
		if (UEquipmentManagerComponent* EquipmentManager = Pawn->FindComponentByClass<UEquipmentManagerComponent>())
		{
			if (UWeaponInstance* NewInstance = EquipmentManager->GetFirstInstanceOfType<UWeaponInstance>())
			{
				if (NewInstance != CurrentInstance && NewInstance->GetInstigator() != nullptr)
				{
					UWeaponInstance* OldWeapon = CurrentInstance;
					CurrentInstance = NewInstance;
					OnWeaponChanged(OldWeapon, CurrentInstance);
				}
			}
		}
	}
}
