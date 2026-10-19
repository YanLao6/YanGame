// 武器准星控件基类实现。

#include "Weapon/ReticleWidgetBase.h"

#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Inventory/InventoryItemInstance.h"
#include "Weapon/RangedWeaponInstance.h"
#include "Weapon/WeaponInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReticleWidgetBase)

UReticleWidgetBase::UReticleWidgetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UReticleWidgetBase::InitializeFromWeapon(UWeaponInstance* InWeapon)
{
	WeaponInstance = InWeapon;
	InventoryInstance = nullptr;
	if (WeaponInstance)
	{
		InventoryInstance = Cast<UInventoryItemInstance>(WeaponInstance->GetInstigator());
	}
	OnWeaponInitialized();
}

float UReticleWidgetBase::ComputeSpreadAngle() const
{
	if (const URangedWeaponInstance* RangedWeapon = Cast<const URangedWeaponInstance>(WeaponInstance))
	{
		return RangedWeapon->GetCalculatedSpreadAngle() * RangedWeapon->GetCalculatedSpreadAngleMultiplier();
	}

	return 0.0f;
}

bool UReticleWidgetBase::HasFirstShotAccuracy() const
{
	if (const URangedWeaponInstance* RangedWeapon = Cast<const URangedWeaponInstance>(WeaponInstance))
	{
		return RangedWeapon->HasFirstShotAccuracy();
	}

	return false;
}

float UReticleWidgetBase::ComputeMaxScreenspaceSpreadRadius() const
{
	// 散布是一个锥体。取锥体边缘上一条足够远的射线，把它的端点投影回屏幕，
	// 端点到屏幕中心的距离即为准星应当张开的半径。
	// 相机位置与枪口之间存在偏移，因此结果是近似值。
	constexpr float LongShotDistance = 10000.0f;

	APlayerController* PC = GetOwningPlayer();
	if (PC && PC->PlayerCameraManager)
	{
		const float SpreadRadiusRads = FMath::DegreesToRadians(ComputeSpreadAngle() * 0.5f);
		const float SpreadRadiusAtDistance = FMath::Tan(SpreadRadiusRads) * LongShotDistance;

		FVector CamPos;
		FRotator CamOrient;
		PC->PlayerCameraManager->GetCameraViewPoint(CamPos, CamOrient);

		const FVector CamForwDir = CamOrient.RotateVector(FVector::ForwardVector);
		const FVector CamUpDir = CamOrient.RotateVector(FVector::UpVector);

		const FVector OffsetTargetAtDistance = CamPos + (CamForwDir * LongShotDistance) + (CamUpDir * SpreadRadiusAtDistance);

		FVector2D OffsetTargetInScreenspace;
		if (PC->ProjectWorldLocationToScreen(OffsetTargetAtDistance, OffsetTargetInScreenspace, true))
		{
			int32 ViewportSizeX = 0;
			int32 ViewportSizeY = 0;
			PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

			const FVector2D ScreenSpaceCenter(FVector::FReal(ViewportSizeX) * 0.5f, FVector::FReal(ViewportSizeY) * 0.5f);

			return (OffsetTargetInScreenspace - ScreenSpaceCenter).Length();
		}
	}

	return 0.0f;
}
