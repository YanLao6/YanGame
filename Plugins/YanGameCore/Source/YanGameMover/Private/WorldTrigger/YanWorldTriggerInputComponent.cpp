#include "WorldTrigger/YanWorldTriggerInputComponent.h"

#include "GameFramework/Pawn.h"
#include "MoverComponent.h"
#include "YanMoverDataModelTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanWorldTriggerInputComponent)

UYanWorldTriggerInputComponent* UYanWorldTriggerInputComponent::FindOrAdd(APawn* Pawn)
{
	if (!Pawn || !Pawn->FindComponentByClass<UMoverComponent>())
	{
		return nullptr;
	}

	if (UYanWorldTriggerInputComponent* Existing = Pawn->FindComponentByClass<UYanWorldTriggerInputComponent>())
	{
		return Existing;
	}

	UYanWorldTriggerInputComponent* NewComponent = NewObject<UYanWorldTriggerInputComponent>(Pawn);
	NewComponent->RegisterComponent();

	return NewComponent;
}

uint8 UYanWorldTriggerInputComponent::NextRequestId(uint8& Serial)
{
	++Serial;
	if (Serial == 0)
	{
		++Serial;
	}
	return Serial;
}

void UYanWorldTriggerInputComponent::RequestLaunch(FVector AdditiveVelocity)
{
	bPendingLaunch        = true;
	PendingLaunchVelocity = AdditiveVelocity;
	NextRequestId(LaunchRequestSerial);
}

void UYanWorldTriggerInputComponent::RequestTeleport(FVector Location, FRotator Rotation, bool bOverrideVelocity, FVector ExitVelocity)
{
	bPendingTeleport                 = true;
	NextRequestId(TeleportRequestSerial);
	PendingTeleportLocation          = Location;
	PendingTeleportRotation          = Rotation;
	bPendingTeleportOverridesVelocity = bOverrideVelocity;
	PendingTeleportExitVelocity      = ExitVelocity;
}

void UYanWorldTriggerInputComponent::OnRegister()
{
	Super::OnRegister();

	// MoverComponent 只在自己 BeginPlay 时收集一次输入源，按需挂上的组件须自行登记
	CachedMoverComp = GetOwner() ? GetOwner()->FindComponentByClass<UMoverComponent>() : nullptr;
	if (CachedMoverComp)
	{
		CachedMoverComp->InputProducers.AddUnique(this);
	}
}

void UYanWorldTriggerInputComponent::OnUnregister()
{
	if (IsValid(CachedMoverComp))
	{
		CachedMoverComp->InputProducers.Remove(this);
	}
	CachedMoverComp = nullptr;

	Super::OnUnregister();
}

void UYanWorldTriggerInputComponent::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	// 每帧显式写入：脉冲位必须在投递后的次帧确定为 false，输入命令若被跨帧复用不会重复触发
	FYanCharacterInputs& YanInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FYanCharacterInputs>();

	YanInputs.bWantsWorldLaunch = bPendingLaunch;
	if (bPendingLaunch)
	{
		YanInputs.WorldLaunchRequestId = LaunchRequestSerial;
		YanInputs.WorldLaunchVelocity  = PendingLaunchVelocity;
	}

	YanInputs.bWantsWorldTeleport = bPendingTeleport;
	if (bPendingTeleport)
	{
		YanInputs.WorldTeleportRequestId          = TeleportRequestSerial;
		YanInputs.WorldTeleportLocation           = PendingTeleportLocation;
		YanInputs.WorldTeleportRotation           = PendingTeleportRotation;
		YanInputs.bWorldTeleportOverridesVelocity = bPendingTeleportOverridesVelocity;
		YanInputs.WorldTeleportExitVelocity       = PendingTeleportExitVelocity;
	}

	bPendingLaunch   = false;
	bPendingTeleport = false;
}
