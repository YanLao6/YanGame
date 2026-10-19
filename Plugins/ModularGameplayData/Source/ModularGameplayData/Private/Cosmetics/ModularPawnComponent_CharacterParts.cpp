// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cosmetics/ModularPawnComponent_CharacterParts.h"

#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Cosmetics/ModularCharacterPartTypes.h"
#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularPawnComponent_CharacterParts)

FString FModularAppliedCharacterPartEntry::GetDebugString() const
{
	return FString::Printf(TEXT("(PartClass: %s, Socket: %s, Instance: %s)"),
		*GetPathNameSafe(Part.PartClass), *Part.SocketName.ToString(), *GetPathNameSafe(SpawnedComponent));
}

void FModularCharacterPartList::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	bool bDestroyedAnyActors = false;
	for (int32 Index : RemovedIndices)
	{
		bDestroyedAnyActors |= DestroyActorForEntry(Entries[Index]);
	}

	if (bDestroyedAnyActors && ensure(OwnerComponent))
	{
		OwnerComponent->BroadcastChanged();
	}
}

void FModularCharacterPartList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	bool bCreatedAnyActors = false;
	for (int32 Index : AddedIndices)
	{
		bCreatedAnyActors |= SpawnActorForEntry(Entries[Index]);
	}

	if (bCreatedAnyActors && ensure(OwnerComponent))
	{
		OwnerComponent->BroadcastChanged();
	}
}

void FModularCharacterPartList::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	bool bChangedAnyActors = false;

	// 不支持增量更新已生成的部件，一律销毁重建
	for (int32 Index : ChangedIndices)
	{
		FModularAppliedCharacterPartEntry& Entry = Entries[Index];

		bChangedAnyActors |= DestroyActorForEntry(Entry);
		bChangedAnyActors |= SpawnActorForEntry(Entry);
	}

	if (bChangedAnyActors && ensure(OwnerComponent))
	{
		OwnerComponent->BroadcastChanged();
	}
}

FModularCharacterPartHandle FModularCharacterPartList::AddEntry(FModularCharacterPart NewPart)
{
	FModularCharacterPartHandle Result;
	Result.PartHandle = PartHandleCounter++;

	if (ensure(OwnerComponent && OwnerComponent->GetOwner() && OwnerComponent->GetOwner()->HasAuthority()))
	{
		FModularAppliedCharacterPartEntry& NewEntry = Entries.AddDefaulted_GetRef();
		NewEntry.Part                               = NewPart;
		NewEntry.PartHandle                         = Result.PartHandle;

		// 监听服务器上本机也要生成，故此处同样调用
		if (SpawnActorForEntry(NewEntry))
		{
			OwnerComponent->BroadcastChanged();
		}

		MarkItemDirty(NewEntry);
	}

	return Result;
}

void FModularCharacterPartList::RemoveEntry(FModularCharacterPartHandle Handle)
{
	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FModularAppliedCharacterPartEntry& Entry = *EntryIt;
		if (Entry.PartHandle != Handle.PartHandle)
		{
			continue;
		}

		const bool bDestroyedActor = DestroyActorForEntry(Entry);
		EntryIt.RemoveCurrent();
		MarkArrayDirty();

		if (bDestroyedActor && ensure(OwnerComponent))
		{
			OwnerComponent->BroadcastChanged();
		}

		break;
	}
}

void FModularCharacterPartList::ClearAllEntries(bool bBroadcastChangeDelegate)
{
	bool bDestroyedAnyActors = false;
	for (FModularAppliedCharacterPartEntry& Entry : Entries)
	{
		bDestroyedAnyActors |= DestroyActorForEntry(Entry);
	}

	Entries.Reset();
	MarkArrayDirty();

	if (bDestroyedAnyActors && bBroadcastChangeDelegate && ensure(OwnerComponent))
	{
		OwnerComponent->BroadcastChanged();
	}
}

FGameplayTagContainer FModularCharacterPartList::CollectCombinedTags() const
{
	FGameplayTagContainer Result;

	for (const FModularAppliedCharacterPartEntry& Entry : Entries)
	{
		if (Entry.SpawnedComponent == nullptr)
		{
			continue;
		}

		if (IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Entry.SpawnedComponent->GetChildActor()))
		{
			TagInterface->GetOwnedGameplayTags(/*inout*/ Result);
		}
	}

	return Result;
}

bool FModularCharacterPartList::SpawnActorForEntry(FModularAppliedCharacterPartEntry& Entry)
{
	// 专用服务器不生成任何外观部件
	if (!ensure(OwnerComponent) || OwnerComponent->IsNetMode(NM_DedicatedServer) || Entry.Part.PartClass == nullptr)
	{
		return false;
	}

	USceneComponent* ComponentToAttachTo = OwnerComponent->GetSceneComponentToAttachTo();
	if (!ComponentToAttachTo)
	{
		return false;
	}

	UChildActorComponent* PartComponent = NewObject<UChildActorComponent>(OwnerComponent->GetOwner());

	PartComponent->SetupAttachment(ComponentToAttachTo, Entry.Part.SocketName);
	PartComponent->SetChildActorClass(Entry.Part.PartClass);
	// 子 Actor 默认不设 Owner；部件的 bOwnerNoSee / bOnlyOwnerSee 要靠 Owner 链指回属主 Pawn 才能命中
	PartComponent->SetChildActorOwnerOnCreation(true);
	PartComponent->RegisterComponent();

	if (AActor* SpawnedActor = PartComponent->GetChildActor())
	{
		if (Entry.Part.CollisionMode == EModularCharacterPartCollisionMode::NoCollision)
		{
			SpawnedActor->SetActorEnableCollision(false);
		}

		// ChildActorComponent 不会建立 tick 依赖，需手动建立以免部件比骨骼晚一帧
		if (USceneComponent* SpawnedRootComponent = SpawnedActor->GetRootComponent())
		{
			SpawnedRootComponent->AddTickPrerequisiteComponent(ComponentToAttachTo);
		}
	}

	Entry.SpawnedComponent = PartComponent;

	return true;
}

bool FModularCharacterPartList::DestroyActorForEntry(FModularAppliedCharacterPartEntry& Entry)
{
	if (Entry.SpawnedComponent == nullptr)
	{
		return false;
	}

	Entry.SpawnedComponent->DestroyComponent();
	Entry.SpawnedComponent = nullptr;

	return true;
}

UModularPawnComponent_CharacterParts::UModularPawnComponent_CharacterParts(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UModularPawnComponent_CharacterParts::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, CharacterPartList);
}

FModularCharacterPartHandle UModularPawnComponent_CharacterParts::AddCharacterPart(const FModularCharacterPart& NewPart)
{
	return CharacterPartList.AddEntry(NewPart);
}

void UModularPawnComponent_CharacterParts::RemoveCharacterPart(FModularCharacterPartHandle Handle)
{
	CharacterPartList.RemoveEntry(Handle);
}

void UModularPawnComponent_CharacterParts::RemoveAllCharacterParts()
{
	CharacterPartList.ClearAllEntries(/*bBroadcastChangeDelegate=*/ true);
}

void UModularPawnComponent_CharacterParts::BeginPlay()
{
	Super::BeginPlay();
}

void UModularPawnComponent_CharacterParts::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 组件即将销毁，无需再广播变化
	CharacterPartList.ClearAllEntries(/*bBroadcastChangeDelegate=*/ false);

	Super::EndPlay(EndPlayReason);
}

void UModularPawnComponent_CharacterParts::OnRegister()
{
	Super::OnRegister();

	// CDO 上不建立回指，否则模板实例会被误当作运行时属主
	if (!IsTemplate())
	{
		CharacterPartList.SetOwnerComponent(this);
	}
}

TArray<AActor*> UModularPawnComponent_CharacterParts::GetCharacterPartActors() const
{
	TArray<AActor*> Result;
	Result.Reserve(CharacterPartList.Entries.Num());

	for (const FModularAppliedCharacterPartEntry& Entry : CharacterPartList.Entries)
	{
		if (const UChildActorComponent* PartComponent = Entry.SpawnedComponent)
		{
			if (AActor* SpawnedActor = PartComponent->GetChildActor())
			{
				Result.Add(SpawnedActor);
			}
		}
	}

	return Result;
}

USkeletalMeshComponent* UModularPawnComponent_CharacterParts::GetParentMeshComponent() const
{
	const AActor* OwnerActor = GetOwner();
	if (OwnerActor == nullptr)
	{
		return nullptr;
	}

	if (const ACharacter* OwningCharacter = Cast<ACharacter>(OwnerActor))
	{
		return OwningCharacter->GetMesh();
	}

	// 属主不在 ACharacter 继承链上（如基于 Mover 的 Pawn）：取主网格，即首个骨骼网格组件
	return OwnerActor->FindComponentByClass<USkeletalMeshComponent>();
}

USceneComponent* UModularPawnComponent_CharacterParts::GetSceneComponentToAttachTo() const
{
	if (USkeletalMeshComponent* MeshComponent = GetParentMeshComponent())
	{
		return MeshComponent;
	}

	if (const AActor* OwnerActor = GetOwner())
	{
		return OwnerActor->GetRootComponent();
	}

	return nullptr;
}

FGameplayTagContainer UModularPawnComponent_CharacterParts::GetCombinedTags(FGameplayTag RequiredPrefix) const
{
	FGameplayTagContainer Result = CharacterPartList.CollectCombinedTags();

	if (RequiredPrefix.IsValid())
	{
		return Result.Filter(FGameplayTagContainer(RequiredPrefix));
	}

	return Result;
}

void UModularPawnComponent_CharacterParts::BroadcastChanged()
{
	const bool bReinitPose = true;

	if (USkeletalMeshComponent* MeshComponent = GetParentMeshComponent())
	{
		// 身体网格由部件标签共同决定，配件变化可能改变体型
		const FGameplayTagContainer MergedTags  = GetCombinedTags(FGameplayTag());
		USkeletalMesh*              DesiredMesh = BodyMeshes.SelectBestBodyStyle(MergedTags);

		// 网格未变化时该调用不产生副作用
		MeshComponent->SetSkeletalMesh(DesiredMesh, bReinitPose);

		// 显式指定时覆盖网格自带的物理资产
		if (UPhysicsAsset* PhysicsAsset = BodyMeshes.ForcedPhysicsAsset)
		{
			MeshComponent->SetPhysicsAsset(PhysicsAsset, /*bForceReInit=*/ bReinitPose);
		}
	}

	OnCharacterPartsChanged.Broadcast(this);
}
