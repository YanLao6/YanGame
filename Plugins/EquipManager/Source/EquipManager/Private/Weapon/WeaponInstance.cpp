// 武器实例实现。


#include "Weapon/WeaponInstance.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Cosmetics/ModularPawnComponent_CharacterParts.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GameFramework/Pawn.h"
#include "ModularGameplayAbilitiesLogChannels.h"


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

void UWeaponInstance::RefreshCosmetics()
{
	Super::RefreshCosmetics();

	ApplyAnimLayer();
}
//~End UEquipmentInstance Interface

void UWeaponInstance::ApplyAnimLayer()
{
	USkeletalMeshComponent* OwningMesh = GetOwningMesh();
	if (OwningMesh == nullptr)
	{
		return;
	}

	if (OwningMesh->GetAnimInstance() == nullptr)
	{
		// 动画实例尚未建立，链接无处落脚；其初始化完成后装备管理组件会再驱动一次重放。
		LinkedAnimLayer = nullptr;
		return;
	}

	const TSubclassOf<UAnimInstance> DesiredLayer = PickBestAnimLayer(IsEquipped(), DetermineCosmeticTags());

	/**
	 * 链接是否成立须向网格求证，不能只凭本实例的记录。
	 *
	 * 动画实例重新初始化会清空其上已链接的全部层，而重新初始化未必换掉动画实例对象本身——
	 * 动画蓝图类不变时引擎会原地复用它。只凭记录判断，就会把已被清空的链接当作仍然有效，
	 * 于是在目标层恰好没变的情况下直接跳过，角色因此停在空手动画上。
	 */
	if ((DesiredLayer != nullptr) && (OwningMesh->GetLinkedAnimLayerInstanceByClass(DesiredLayer) != nullptr))
	{
		LinkedAnimLayer = DesiredLayer;
		return;
	}

	// 上一件武器的层若还挂着则先摘掉；已被清空的层无须再摘。
	if ((LinkedAnimLayer != nullptr) && (LinkedAnimLayer != DesiredLayer) &&
	    (OwningMesh->GetLinkedAnimLayerInstanceByClass(LinkedAnimLayer) != nullptr))
	{
		OwningMesh->UnlinkAnimClassLayers(LinkedAnimLayer);
	}

	if (DesiredLayer != nullptr)
	{
		OwningMesh->LinkAnimClassLayers(DesiredLayer);
	}

	LinkedAnimLayer = DesiredLayer;

	UE_LOG(LogModularGameplayAbilities, Verbose, TEXT("WeaponInstance [%s]：动画层链接为 [%s]，装备状态 %d。"),
	       *GetName(), *GetNameSafe(DesiredLayer), IsEquipped() ? 1 : 0);
}

FGameplayTagContainer UWeaponInstance::DetermineCosmeticTags() const
{
	if (const APawn* OwningPawn = GetPawn())
	{
		if (const UModularPawnComponent_CharacterParts* CosmeticComponent = OwningPawn->FindComponentByClass<UModularPawnComponent_CharacterParts>())
		{
			// 动画层规则按外观标签匹配，取全部合并标签而不作前缀过滤，与规则表的配置口径一致。
			return CosmeticComponent->GetCombinedTags(FGameplayTag());
		}
	}

	return FGameplayTagContainer();
}

USkeletalMeshComponent* UWeaponInstance::GetOwningMesh() const
{
	if (const APawn* OwningPawn = GetPawn())
	{
		// 拥有者未必派生自 ACharacter，按「首个骨骼网格即主网格」的约定解析，与附属 Actor 的挂载口径一致。
		return OwningPawn->FindComponentByClass<USkeletalMeshComponent>();
	}

	return nullptr;
}

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
