// 快捷栏配置覆盖组件声明。

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Equipment/QuickBarComponent.h"
#include "QuickBarConfigComponent.generated.h"

#define UE_API EQUIPMANAGER_API

/**
 * 快捷栏配置覆盖组件。
 *
 * 挂在与快捷栏组件同一个 Actor 上时，用本组件的槽位数量与准入规则替换快捷栏自带的默认配置，
 * 使同一个快捷栏组件类可被不同 Experience 复用而各自给出不同的槽位布局。
 * 不挂本组件即沿用快捷栏组件自身的默认配置。
 *
 * 配置取自 CDO 而非复制属性，故服务器与客户端各自成立，无需等待复制到达。
 */
UCLASS(MinimalAPI, Blueprintable, meta=(BlueprintSpawnableComponent))
class UQuickBarConfigComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 构造快捷栏配置组件。 */
	UE_API UQuickBarConfigComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 返回本组件配置的槽位数量。 */
	int32 GetNumSlots() const { return NumSlots; }

	/** 返回本组件配置的各槽位准入规则。 */
	const TArray<FQuickBarSlotRule>& GetSlotRules() const { return SlotRules; }

	//~Begin UActorComponent Interface
	// 向同一 Actor 上的快捷栏组件推送本配置。
	UE_API virtual void BeginPlay() override;
	//~End UActorComponent Interface

protected:
	/** 覆盖后的快捷栏槽位数量。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=QuickBar)
	int32 NumSlots = 4;

	/** 覆盖后的各槽位准入规则，按索引对应槽位；长度可短于槽位数，未覆盖的槽位不作限制。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=QuickBar)
	TArray<FQuickBarSlotRule> SlotRules;
};

#undef UE_API
