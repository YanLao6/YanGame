// 局内配装发放组件实现。

#include "Loadout/InGameLoadoutComponent.h"

#include "ActorComponent/ModularExperienceComponent.h"
#include "ActorComponent/ModularPawnComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Equipment/QuickBarComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/InventoryItemDefinition.h"
#include "Inventory/InventoryItemInstance.h"
#include "Inventory/InventoryManagerComponent.h"
#include "Loadout/InGameLoadoutDefinition.h"
#include "ModularGameplayAbilitiesLogChannels.h"
#include "ModularGameplayTags.h"
#include "Stash/EquipManagerSettings.h"
#include "Stash/StashSubsystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InGameLoadoutComponent)

UInGameLoadoutComponent::UInGameLoadoutComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInGameLoadoutComponent::AddLoadoutDefinition(TSoftObjectPtr<UInGameLoadoutDefinition> InDefinition)
{
	if (InDefinition.IsNull())
	{
		return;
	}

	if (bGranted)
	{
		UE_LOG(LogModularGameplayAbilities, Warning, TEXT("开局配装已发放，此时追加配装资产 [%s] 不会生效。"), *InDefinition.ToString());
		return;
	}

	LoadoutDefinitions.AddUnique(InDefinition);
}

void UInGameLoadoutComponent::RemoveLoadoutDefinition(TSoftObjectPtr<UInGameLoadoutDefinition> InDefinition)
{
	LoadoutDefinitions.Remove(InDefinition);
}

//~Begin UActorComponent Interface
void UInGameLoadoutComponent::BeginPlay()
{
	Super::BeginPlay();

	AController* OwningController = GetController<AController>();
	if (OwningController == nullptr)
	{
		return;
	}

	// 本地控制器负责交出玩家意图；监听服务器上这两个身份是同一个控制器。
	if (OwningController->IsLocalController())
	{
		SubmitLoadoutFromStash();
	}

	if (OwningController->HasAuthority())
	{
		OwningController->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessedPawnChanged);

		if (const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr)
		{
			if (UModularExperienceComponent* ExperienceComponent = GameState->FindComponentByClass<UModularExperienceComponent>())
			{
				ExperienceComponent->CallOrRegister_OnExperienceLoaded(FOnModularExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
			}
		}

		// 上行可能迟到或根本不来，到点就以模式配置兜底。
		if (LoadoutRequestTimeout > 0.0f)
		{
			GetWorld()->GetTimerManager().SetTimer(RequestTimeoutHandle, this, &ThisClass::OnLoadoutRequestTimeout, LoadoutRequestTimeout, false);
		}
		else
		{
			bRequestSettled = true;
		}
	}
}

void UInGameLoadoutComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (StashChangedHandle.IsValid())
	{
		if (const APlayerController* OwningController = GetController<APlayerController>())
		{
			if (const ULocalPlayer* LocalPlayer = OwningController->GetLocalPlayer())
			{
				if (UStashSubsystem* Stash = LocalPlayer->GetSubsystem<UStashSubsystem>())
				{
					Stash->OnStashChanged.Remove(StashChangedHandle);
				}
			}
		}

		StashChangedHandle.Reset();
	}

	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(RequestTimeoutHandle);
	}

	RevokeLoadout();

	Super::EndPlay(EndPlayReason);
}

//~End UActorComponent Interface

void UInGameLoadoutComponent::OnExperienceLoaded(const UModularExperienceDefinition* Experience)
{
	bExperienceLoaded = true;

	TryGrantLoadout();
}

void UInGameLoadoutComponent::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (OldPawn != ReadyPawn)
	{
		// 旧 Pawn 的就绪状态不适用于新 Pawn。
		ReadyPawn = nullptr;
	}

	if (NewPawn == nullptr)
	{
		return;
	}

	if (UGameFrameworkComponentManager* Manager = UGameFrameworkComponentManager::GetForActor(NewPawn))
	{
		// 只认 Pawn 扩展组件推进的状态：它是把 PawnData Fragment 应用完毕的那一个，
		// 别的 feature 达到就绪并不代表英雄专属配装已经加进来。
		// 就绪通知可能已经发生过，故要求补发一次，否则挂得晚的组件永远等不到。
		Manager->RegisterAndCallForActorInitState(
			NewPawn,
			UModularPawnComponent::NAME_ActorFeatureName,
			ModularGameplayTags::InitState_GameplayReady,
			FActorInitStateChangedDelegate::CreateUObject(this, &ThisClass::OnPawnReady),
			/*bCallImmediately=*/ true);
	}
}

void UInGameLoadoutComponent::OnPawnReady(const FActorInitStateChangedParams& Params)
{
	if (Params.FeatureState != ModularGameplayTags::InitState_GameplayReady)
	{
		return;
	}

	ReadyPawn = Cast<APawn>(Params.OwningActor);

	TryGrantLoadout();
}

void UInGameLoadoutComponent::TryGrantLoadout()
{
	if (bGranted || !bExperienceLoaded || !bRequestSettled)
	{
		return;
	}

	const AController* OwningController = GetController<AController>();
	if ((OwningController == nullptr) || !OwningController->HasAuthority())
	{
		return;
	}

	// 装备链路要在 Pawn 上生成实例，而英雄专属配装由 PawnData 在其就绪前加入，
	// 因此要等的不只是「有 Pawn」，而是「这具 Pawn 已就绪」。
	if ((ReadyPawn == nullptr) || (OwningController->GetPawn() != ReadyPawn))
	{
		return;
	}

	GrantLoadout();
}

void UInGameLoadoutComponent::GrantLoadout()
{
	AController* OwningController = GetController<AController>();
	if (OwningController == nullptr)
	{
		return;
	}

	UInventoryManagerComponent* Inventory = OwningController->FindComponentByClass<UInventoryManagerComponent>();
	if (Inventory == nullptr)
	{
		UE_LOG(LogModularGameplayAbilities, Error, TEXT("开局配装：[%s] 上没有背包组件，物品不会被发放。"), *OwningController->GetPathName());
		return;
	}

	TArray<const UInGameLoadoutDefinition*> Definitions;
	Definitions.Reserve(LoadoutDefinitions.Num());
	for (const TSoftObjectPtr<UInGameLoadoutDefinition>& SoftDefinition : LoadoutDefinitions)
	{
		if (const UInGameLoadoutDefinition* Definition = SoftDefinition.LoadSynchronous())
		{
			Definitions.Add(Definition);
		}
	}

	const FResolvedLoadout Resolved = LoadoutResolver::Resolve(Definitions, PendingRequest, UEquipManagerSettings::GetItemCatalog());

	bGranted = true;

	UQuickBarComponent* QuickBar = OwningController->FindComponentByClass<UQuickBarComponent>();

	for (const FResolvedLoadoutEntry& Entry : Resolved.Entries)
	{
		TSubclassOf<UInventoryItemDefinition> ItemDef = Entry.ItemDefinition.LoadSynchronous();
		if (ItemDef == nullptr)
		{
			UE_LOG(LogModularGameplayAbilities, Warning, TEXT("开局配装：物品定义 [%s] 加载失败。"), *Entry.ItemDefinition.ToString());
			continue;
		}

		UInventoryItemInstance* Instance = Inventory->AddItemDefinition(ItemDef, FMath::Max(1, Entry.StackCount));
		if (Instance == nullptr)
		{
			UE_LOG(LogModularGameplayAbilities, Warning, TEXT("开局配装：物品 [%s] 未能加入背包。"), *ItemDef->GetName());
			continue;
		}

		GrantedItems.Add(Instance);

		if (Entry.QuickBarSlot < 0)
		{
			continue;
		}

		if (QuickBar == nullptr)
		{
			UE_LOG(LogModularGameplayAbilities, Warning, TEXT("开局配装：[%s] 上没有快捷栏组件，物品 [%s] 只会留在背包中。"), *OwningController->GetPathName(), *ItemDef->GetName());
			continue;
		}

		// 配置指定了目标槽位，因此校验该槽位本身而不另找空位。
		if (!QuickBar->CanAddItemToSlot(Entry.QuickBarSlot, Instance))
		{
			UE_LOG(LogModularGameplayAbilities, Warning, TEXT("开局配装：槽位 %d 无效、已被占用或不接受该物品类别，物品 [%s] 只会留在背包中。"), Entry.QuickBarSlot, *ItemDef->GetName());
			continue;
		}

		QuickBar->AddItemToSlot(Entry.QuickBarSlot, Instance);

		OccupiedSlots.Add(Entry.QuickBarSlot);
	}

	// 只激活决议指定的那个槽位，且该槽位确实落位成功才激活。
	if ((QuickBar != nullptr) && OccupiedSlots.Contains(Resolved.SlotToActivate))
	{
		// 激活槽位会触发装备流程，在 Pawn 上生成装备实例与附属 Actor。
		QuickBar->SetActiveSlotIndex(Resolved.SlotToActivate);
	}
}

void UInGameLoadoutComponent::RevokeLoadout()
{
	AController* OwningController = GetController<AController>();
	if ((OwningController == nullptr) || !bGranted)
	{
		return;
	}

	// 先清空槽位，激活中的槽位会在此过程中自动卸下对应装备。
	if (UQuickBarComponent* QuickBar = OwningController->FindComponentByClass<UQuickBarComponent>())
	{
		for (int32 SlotIndex : OccupiedSlots)
		{
			QuickBar->RemoveItemFromSlot(SlotIndex);
		}
	}

	if (UInventoryManagerComponent* Inventory = OwningController->FindComponentByClass<UInventoryManagerComponent>())
	{
		for (const TWeakObjectPtr<UInventoryItemInstance>& Item : GrantedItems)
		{
			if (UInventoryItemInstance* Instance = Item.Get())
			{
				Inventory->RemoveItemInstance(Instance);
			}
		}
	}

	OccupiedSlots.Reset();
	GrantedItems.Reset();
	bGranted = false;
}

void UInGameLoadoutComponent::SubmitLoadoutFromStash()
{
	const APlayerController* OwningController = GetController<APlayerController>();
	if (OwningController == nullptr)
	{
		return;
	}

	const ULocalPlayer* LocalPlayer = OwningController->GetLocalPlayer();
	UStashSubsystem*    Stash       = LocalPlayer ? LocalPlayer->GetSubsystem<UStashSubsystem>() : nullptr;
	if (Stash == nullptr)
	{
		// 没有仓库子系统时不上行，服务器会在超时后以模式配置兜底。
		return;
	}

	if (!Stash->IsLoaded())
	{
		// 仓库仍在读取，读完再上行；此间服务器那侧由超时兜住。
		if (!StashChangedHandle.IsValid())
		{
			StashChangedHandle = Stash->OnStashChanged.AddUObject(this, &ThisClass::OnStashChanged);
		}
		return;
	}

	if (StashChangedHandle.IsValid())
	{
		Stash->OnStashChanged.Remove(StashChangedHandle);
		StashChangedHandle.Reset();
	}

	ServerSubmitLoadout(Stash->MakeLoadoutRequest());
}

void UInGameLoadoutComponent::OnStashChanged()
{
	SubmitLoadoutFromStash();
}

void UInGameLoadoutComponent::OnLoadoutRequestTimeout()
{
	if (bRequestSettled)
	{
		return;
	}

	UE_LOG(LogModularGameplayAbilities, Log, TEXT("开局配装：等待玩家配装上行超时，改以模式配置发放。"));

	bRequestSettled = true;
	TryGrantLoadout();
}

void UInGameLoadoutComponent::ServerSubmitLoadout_Implementation(const FLoadoutRequest& Request)
{
	if (bGranted)
	{
		// 已经发放完毕，迟到的上行不再改变本局内容。
		return;
	}

	PendingRequest  = Request;
	bRequestSettled = true;

	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(RequestTimeoutHandle);
	}

	TryGrantLoadout();
}
