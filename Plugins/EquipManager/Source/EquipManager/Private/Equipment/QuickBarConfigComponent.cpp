// 快捷栏配置覆盖组件实现。


#include "Equipment/QuickBarConfigComponent.h"

#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(QuickBarConfigComponent)


UQuickBarConfigComponent::UQuickBarConfigComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 配置只在启动时推送一次，无需每帧更新。
	PrimaryComponentTick.bCanEverTick = false;
}

//~Begin UActorComponent Interface
void UQuickBarConfigComponent::BeginPlay()
{
	Super::BeginPlay();

	// 两个组件都由 GameFeatureAction 挂入，先后次序取决于 ActionSet 的排列，
	// 故此处主动推送一次，与快捷栏组件启动时的主动拉取合起来使结果与次序无关。
	if (AActor* OwningActor = GetOwner())
	{
		if (UQuickBarComponent* QuickBar = OwningActor->FindComponentByClass<UQuickBarComponent>())
		{
			QuickBar->ApplyConfigOverride(NumSlots, SlotRules);
		}
	}
}
//~End UActorComponent Interface
