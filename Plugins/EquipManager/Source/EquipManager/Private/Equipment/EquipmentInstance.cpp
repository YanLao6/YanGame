// 装备实例实现。


#include "Equipment/EquipmentInstance.h"

#include "Equipment/EquipmentDefinition.h"
#include "GameFramework/Character.h"
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
		// 默认附着到根组件；若拥有者是 Character，则优先附着到 Mesh。
		USceneComponent* AttachTarget = OwningPawn->GetRootComponent();
		if (ACharacter* Char = Cast<ACharacter>(OwningPawn))
		{
			AttachTarget = Char->GetMesh();
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
	// 将原生生命周期转发给蓝图事件，便于在资源层实现表现逻辑。
	K2_OnEquipped();
}

void UEquipmentInstance::OnUnequipped()
{
	// 将原生生命周期转发给蓝图事件，便于在资源层实现表现逻辑。
	K2_OnUnequipped();
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
	// 预留给后续在 Instigator 变化时执行客户端刷新逻辑。
}

//~Begin UObject Interface
void UEquipmentInstance::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// 同步由装备实例生成的附属 Actor 列表到客户端。
	DOREPLIFETIME(ThisClass, SpawnedActors);
}
//~End UObject Interface
