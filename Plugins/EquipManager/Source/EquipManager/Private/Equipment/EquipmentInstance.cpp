// 装备实例实现。


#include "Equipment/EquipmentInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "Equipment/EquipmentDefinition.h"
#include "Equipment/EquipmentManagerComponent.h"
#include "Iris/ReplicationSystem/ReplicationFragmentUtil.h"
#include "Net/UnrealNetwork.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipmentInstance)

//~Begin UObject Interface
UEquipmentInstance::UEquipmentInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

UWorld* UEquipmentInstance::GetWorld() const
{
	// 装备实例本身不是 Actor，因此需要通过拥有它的 Pawn 反查 World。
	if (APawn* OwningPawn = GetPawn())
	{
		return OwningPawn->GetWorld();
	}
	else
	{
		return nullptr;
	}
}
//~End UObject Interface

APawn* UEquipmentInstance::GetPawn() const
{
	// 当前实现约定装备实例以 Pawn 作为 Outer。
	return Cast<APawn>(GetOuter());
}

APawn* UEquipmentInstance::GetTypedPawn(TSubclassOf<APawn> PawnType) const
{
	APawn* Result = nullptr;
	if (UClass* ActualPawnType = PawnType)
	{
		// 只有拥有者类型满足要求时才返回，避免外部重复做 Cast 判空。
		if (GetOuter()->IsA(ActualPawnType))
		{
			Result = Cast<APawn>(GetOuter());
		}
	}
	return Result;
}

void UEquipmentInstance::SpawnEquipmentActors(const TArray<FEquipmentActorToSpawn>& ActorsToSpawn)
{
	if (APawn* OwningPawn = GetPawn())
	{
		// 挂点名取自骨骼网格，故优先附着到网格；拥有者未必派生自 ACharacter，
		// 因此按「首个骨骼网格即主网格」的约定解析，取不到才退回根组件。
		USceneComponent* AttachTarget = OwningPawn->GetRootComponent();
		if (USkeletalMeshComponent* PawnMesh = OwningPawn->FindComponentByClass<USkeletalMeshComponent>())
		{
			AttachTarget = PawnMesh;
		}

		// 按装备定义逐个生成附属 Actor，并记录到实例内部统一管理。
		for (const FEquipmentActorToSpawn& SpawnInfo : ActorsToSpawn)
		{
			AActor* NewActor = GetWorld()->SpawnActorDeferred<AActor>(SpawnInfo.ActorToSpawn, FTransform::Identity, OwningPawn);
			NewActor->FinishSpawning(FTransform::Identity, /*使用默认变换=*/ true);
			NewActor->SetActorRelativeTransform(SpawnInfo.AttachTransform);
			NewActor->AttachToComponent(AttachTarget, FAttachmentTransformRules::KeepRelativeTransform, SpawnInfo.AttachSocket);

			SpawnedActors.Add(NewActor);
		}
	}
}

void UEquipmentInstance::SetEquipmentActorsHidden(bool bHidden)
{
	bEquipmentActorsHidden = bHidden;
	ApplyEquipmentActorsVisibility();
}

void UEquipmentInstance::ApplyEquipmentActorsVisibility()
{
	for (AActor* Actor : SpawnedActors)
	{
		if (Actor)
		{
			Actor->SetActorHiddenInGame(bEquipmentActorsHidden);
		}
	}
}

void UEquipmentInstance::OnRep_SpawnedActors()
{
	// 附属 Actor 可能晚于生效状态到达，故到达时按已记录的期望值补一次。
	ApplyEquipmentActorsVisibility();
}

void UEquipmentInstance::DestroyEquipmentActors()
{
	// 卸下装备时销毁由本实例创建的全部附属 Actor，避免场景残留。
	for (AActor* Actor : SpawnedActors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}
}

void UEquipmentInstance::OnEquipped()
{
	bActivated = true;

	// 外观统一经重放入口应用，装备与网格重建两条路径因此走同一段逻辑。
	RefreshCosmetics();

	// 将原生生命周期转发给蓝图事件，便于在资源层实现表现逻辑。
	K2_OnEquipped();
}

void UEquipmentInstance::OnUnequipped()
{
	bActivated = false;

	// 卸下同样是一次外观变化：未装备状态自有其动画层，不能只是把已装备的那层摘掉。
	RefreshCosmetics();

	// 将原生生命周期转发给蓝图事件，便于在资源层实现表现逻辑。
	K2_OnUnequipped();
}

void UEquipmentInstance::RefreshCosmetics()
{
	K2_RefreshCosmetics();
}

//~Begin UObject Interface
void UEquipmentInstance::RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context, UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	using namespace UE::Net;

	// 为当前对象创建并注册 Iris 复制片段。
	FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject(this, Context, RegistrationFlags);
}
//~End UObject Interface

void UEquipmentInstance::OnRep_Instigator()
{
	// 生效意图以来源物品表达，而 Instigator 可能晚于装备条目到达，
	// 故其到达时须让管理组件重新解析一次，否则本实例会一直停在未生效状态。
	if (APawn* OwningPawn = GetPawn())
	{
		if (UEquipmentManagerComponent* EquipmentManager = OwningPawn->FindComponentByClass<UEquipmentManagerComponent>())
		{
			EquipmentManager->ResolveActiveInstance();
		}
	}
}

//~Begin UObject Interface
void UEquipmentInstance::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// 同步由装备实例生成的附属 Actor 列表到客户端。
	DOREPLIFETIME(ThisClass, SpawnedActors);
}
//~End UObject Interface
