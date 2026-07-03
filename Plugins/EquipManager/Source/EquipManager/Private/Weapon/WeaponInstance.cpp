// 武器实例实现。


#include "Weapon/WeaponInstance.h"

#include "GameFramework/InputDeviceSubsystem.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(WeaponInstance)

UWeaponInstance::UWeaponInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

//~Begin UEquipmentInstance Interface
void UWeaponInstance::OnEquipped()
{
	Super::OnEquipped();

	UWorld* World = GetWorld();
	check(World);
	// 记录装备时间，供交互冷却或动画层逻辑查询。
	TimeLastEquipped = World->GetTimeSeconds();

	// 装备武器后立即应用输入设备属性，例如手柄震动或自适应扳机。
	ApplyDeviceProperties();
}

void UWeaponInstance::OnUnequipped()
{
	Super::OnUnequipped();

	// 卸下武器时移除之前激活的全部输入设备属性。
	RemoveDeviceProperties();
}
//~End UEquipmentInstance Interface

void UWeaponInstance::UpdateFiringTime()
{
	UWorld* World = GetWorld();
	check(World);
	// 用最近一次开火时间参与最近交互时间的计算。
	TimeLastFired = World->GetTimeSeconds();
}

float UWeaponInstance::GetTimeSinceLastInteractedWith() const
{
	UWorld* World = GetWorld();
	check(World);
	const double WorldTime = World->GetTimeSeconds();

	double Result = WorldTime - TimeLastEquipped;

	if (TimeLastFired > 0.0)
	{
		// 装备和开火都算交互，这里取离现在最近的一次。
		const double TimeSinceFired = WorldTime - TimeLastFired;
		Result                      = FMath::Min(Result, TimeSinceFired);
	}

	return Result;
}

TSubclassOf<UAnimInstance> UWeaponInstance::PickBestAnimLayer(bool bEquipped, const FGameplayTagContainer& CosmeticTags) const
{
	// 根据当前装备状态选择对应的动画层集合，再结合外观标签挑选最佳层。
	const FModularAnimLayerSelectionSet& SetToQuery = (bEquipped ? EquippedAnimSet : UneuippedAnimSet);
	return SetToQuery.SelectBestLayer(CosmeticTags);
}

const FPlatformUserId UWeaponInstance::GetOwningUserId() const
{
	if (const APawn* Pawn = GetPawn())
	{
		return Pawn->GetPlatformUserId();
	}
	return PLATFORMUSERID_NONE;
}

void UWeaponInstance::ApplyDeviceProperties()
{
	const FPlatformUserId UserId = GetOwningUserId();

	if (UserId.IsValid())
	{
		if (UInputDeviceSubsystem* InputDeviceSubsystem = UInputDeviceSubsystem::Get())
		{
			// 将所有配置的设备属性注册到当前拥有者的输入设备上。
			for (TObjectPtr<UInputDeviceProperty>& DeviceProp : ApplicableDeviceProperties)
			{
				FActivateDevicePropertyParams Params = {};
				Params.UserId                        = UserId;

				// 默认作用于该平台用户的主输入设备。
				// 如需指定设备，可在这里额外填写 DeviceId。
				//Params.DeviceId = <指定的设备ID>;

				// 使用循环模式，让设备属性在持有武器期间持续生效。
				Params.bLooping = true;

				DevicePropertyHandles.Emplace(InputDeviceSubsystem->ActivateDeviceProperty(DeviceProp, Params));
			}
		}
	}
}

void UWeaponInstance::RemoveDeviceProperties()
{
	const FPlatformUserId UserId = GetOwningUserId();

	if (UserId.IsValid() && !DevicePropertyHandles.IsEmpty())
	{
		// 清理当前武器曾经激活过的全部设备属性。
		if (UInputDeviceSubsystem* InputDeviceSubsystem = UInputDeviceSubsystem::Get())
		{
			InputDeviceSubsystem->RemoveDevicePropertyHandles(DevicePropertyHandles);
			DevicePropertyHandles.Empty();
		}
	}
}

void UWeaponInstance::OnDeathStarted(AActor* OwningActor)
{
	// 拥有者死亡时也要兜底清理设备属性，避免效果残留到后续输入流程。
	RemoveDeviceProperties();
}
