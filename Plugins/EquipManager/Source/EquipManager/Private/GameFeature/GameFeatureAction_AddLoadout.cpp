// 开局配装注入 GameFeatureAction 实现。

#include "GameFeature/GameFeatureAction_AddLoadout.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFeaturesSubsystem.h"
#include "GameFramework/Controller.h"
#include "Loadout/InGameLoadoutComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureAction_AddLoadout)

#define LOCTEXT_NAMESPACE "GameFeatures"

//~Begin UGameFeatureAction Interface
void UGameFeatureAction_AddLoadout::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	FPerContextData& ActiveData = ContextData.FindOrAdd(Context);

	if (!ensureAlways(ActiveData.AddedComponents.IsEmpty()) ||
	    !ensureAlways(ActiveData.ComponentRequests.IsEmpty()))
	{
		Reset(ActiveData);
	}

	Super::OnGameFeatureActivating(Context);
}

void UGameFeatureAction_AddLoadout::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	if (FPerContextData* ActiveData = ContextData.Find(Context))
	{
		Reset(*ActiveData);
	}
}
//~End UGameFeatureAction Interface

//~Begin UObject Interface
#if WITH_EDITOR
EDataValidationResult UGameFeatureAction_AddLoadout::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	if (ActorClass.IsNull())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(LOCTEXT("LoadoutNullActorClass", "Add Loadout 未指定接收配装的控制器类。"));
	}

	if (LoadoutDefinition.IsNull())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(LOCTEXT("LoadoutNullDefinition", "Add Loadout 未指定配装资产，挂上的组件不会发放任何物品。"));
	}

	return Result;
}
#endif
//~End UObject Interface

//~Begin UGameFeatureAction_WorldActionBase Interface
void UGameFeatureAction_AddLoadout::AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld*          World        = WorldContext.World();
	UGameInstance*   GameInstance = WorldContext.OwningGameInstance;
	FPerContextData& ActiveData   = ContextData.FindOrAdd(ChangeContext);

	if ((GameInstance == nullptr) || (World == nullptr) || !World->IsGameWorld() || ActorClass.IsNull())
	{
		return;
	}

	if (UGameFrameworkComponentManager* ComponentManager = UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance))
	{
		UGameFrameworkComponentManager::FExtensionHandlerDelegate AddLoadoutDelegate =
			UGameFrameworkComponentManager::FExtensionHandlerDelegate::CreateUObject(
				this,
				&UGameFeatureAction_AddLoadout::HandleActorExtension,
				ChangeContext);

		ActiveData.ComponentRequests.Add(ComponentManager->AddExtensionHandler(ActorClass, AddLoadoutDelegate));
	}
}
//~End UGameFeatureAction_WorldActionBase Interface

void UGameFeatureAction_AddLoadout::Reset(FPerContextData& ActiveData)
{
	while (!ActiveData.AddedComponents.IsEmpty())
	{
		auto ComponentIt = ActiveData.AddedComponents.CreateIterator();
		RemoveLoadoutComponent(ComponentIt->Key, ActiveData);
	}

	ActiveData.ComponentRequests.Empty();
}

void UGameFeatureAction_AddLoadout::HandleActorExtension(AActor* Actor, FName EventName, FGameFeatureStateChangeContext ChangeContext)
{
	FPerContextData* ActiveData = ContextData.Find(ChangeContext);
	if (ActiveData == nullptr)
	{
		return;
	}

	if ((EventName == UGameFrameworkComponentManager::NAME_ExtensionRemoved) ||
	    (EventName == UGameFrameworkComponentManager::NAME_ReceiverRemoved))
	{
		RemoveLoadoutComponent(Actor, *ActiveData);
		return;
	}

	if ((EventName == UGameFrameworkComponentManager::NAME_ExtensionAdded) ||
	    (EventName == UGameFrameworkComponentManager::NAME_GameActorReady))
	{
		AddLoadoutComponent(Actor, *ActiveData);
	}
}

void UGameFeatureAction_AddLoadout::AddLoadoutComponent(AActor* Actor, FPerContextData& ActiveData)
{
	AController* Controller = Cast<AController>(Actor);
	if (Controller == nullptr)
	{
		UE_LOG(LogGameFeatures, Error, TEXT("Add Loadout：[%s] 不是控制器，发放组件依赖控制器上的背包与快捷栏。"), *GetNameSafe(Actor));
		return;
	}

	// 同一控制器可能收到多次扩展事件，配装只加一次。
	if (ActiveData.AddedComponents.Contains(Actor))
	{
		return;
	}

	// 发放组件按控制器只存在一个，多个注入方各自往里加配装而不是各挂一个组件——
	// 挂两个组件会发两份物品，且谁也不知道对方占了哪些槽位。
	if (UInGameLoadoutComponent* ExistingComponent = Controller->FindComponentByClass<UInGameLoadoutComponent>())
	{
		ExistingComponent->AddLoadoutDefinition(LoadoutDefinition);

		FAddedComponent& Reused = ActiveData.AddedComponents.Add(Actor);
		Reused.Component        = ExistingComponent;

		return;
	}

	// 服务器与拥有者客户端各自创建本组件且不复制，客户端调用 Server RPC 时按路径名定位它。
	// 自动生成的名字两端不一致，服务器会当作客户端试图创建子对象而踢掉连接，
	// 因此必须用固定名字并标记为可寻址。
	UInGameLoadoutComponent* LoadoutComponent = NewObject<UInGameLoadoutComponent>(Controller, UInGameLoadoutComponent::StaticClass()->GetFName());
	LoadoutComponent->SetNetAddressable();

	// 配装须在注册前写入：组件一旦启动就开始计时等待玩家上行，配置晚到会赶不上发放。
	LoadoutComponent->AddLoadoutDefinition(LoadoutDefinition);
	LoadoutComponent->RegisterComponent();

	FAddedComponent& Created     = ActiveData.AddedComponents.Add(Actor);
	Created.Component            = LoadoutComponent;
	Created.bCreatedByThisAction = true;
}

void UGameFeatureAction_AddLoadout::RemoveLoadoutComponent(AActor* Actor, FPerContextData& ActiveData)
{
	FAddedComponent AddedComponent;
	if (!ActiveData.AddedComponents.RemoveAndCopyValue(Actor, AddedComponent))
	{
		return;
	}

	UInGameLoadoutComponent* LoadoutComponent = AddedComponent.Component.Get();
	if (LoadoutComponent == nullptr)
	{
		return;
	}

	if (AddedComponent.bCreatedByThisAction)
	{
		// 组件在 EndPlay 中回收自己发放过的物品，此处只需销毁它。
		LoadoutComponent->DestroyComponent();
	}
	else
	{
		// 组件归别处所有，只收回本 Action 加进去的那份配装。
		LoadoutComponent->RemoveLoadoutDefinition(LoadoutDefinition);
	}
}

#undef LOCTEXT_NAMESPACE
