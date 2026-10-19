#pragma once

#include "GameFramework/Actor.h"
#include "ColorTransferTypes.h"
#include "Interaction/IInteractableTarget.h"
#include "Interaction/InteractionOption.h"

#include "ColorPaintCan.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class UColorPigmentSet;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * 颜料罐：颜色射线射入的颜料按栈存放，玩家经交互技能每次从栈顶取出固定量到自己身上。
 *
 * CanMesh 提供碰撞，须对颜色射线通道与交互通道 Block（默认 BlockAllDynamic 即满足）。
 * 罐内颜料由 LiquidMesh 槽位 0 的分层材质表现，材质需带以下参数（i 为 0..MaxLayers-1，0 为栈底）：
 * LayerCount、Layer{i}Color、Layer{i}Top（该层顶面占罐高的比例 0~1，逐层累计；未使用的层等于下层顶面）。
 * 只在服务器上结算，颜料栈复制给各端驱动材质与交互选项。
 */
UCLASS(MinimalAPI)
class AColorPaintCan : public AActor, public IColorShotReceiver, public IColorPigmentSource, public IInteractableTarget
{
	GENERATED_BODY()

public:
	/** 材质色带数，也是颜料栈的最大层数 */
	static constexpr int32 MaxLayers = 6;

	UE_API AColorPaintCan();

	//~Begin IColorShotReceiver Interface
	UE_API virtual void ReceiveColorShot(const FColorShotHit& Shot) override;
	//~End IColorShotReceiver Interface

	//~Begin IColorPigmentSource Interface
	UE_API virtual float AuthTakeTopPigment(UColorPigmentSet* PigmentSet, float MaxAmount) override;
	//~End IColorPigmentSource Interface

	//~Begin IInteractableTarget Interface
	UE_API virtual void GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& OptionBuilder) override;
	//~End IInteractableTarget Interface

	//~Begin AActor Interface
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End AActor Interface

	/** 罐内颜料栈，末尾为栈顶。 */
	UFUNCTION(BlueprintPure, Category = "Yan|ColorTransfer")
	const TArray<FColorPigmentLayer>& GetStoredLayers() const { return StoredLayers; }

protected:
	//~Begin AActor Interface
	UE_API virtual void BeginPlay() override;
	//~End AActor Interface

	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UStaticMeshComponent> CanMesh;

	UPROPERTY(VisibleAnywhere, Category = "Yan|ColorTransfer")
	TObjectPtr<UStaticMeshComponent> LiquidMesh;

	/** 超出容量的射入颜料直接损失，射手照样被扣除 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "1.0"))
	float Capacity = 100.0f;

	/** InteractionAbilityToGrant 为取颜料技能，靠近时由交互技能授予玩家 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	FInteractionOption InteractionOption;

	/** 罐内颜料栈，末尾为栈顶；可在关卡中预设初始内容 */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_StoredLayers, Category = "Yan|ColorTransfer")
	TArray<FColorPigmentLayer> StoredLayers;

private:
	UFUNCTION()
	void OnRep_StoredLayers();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LiquidMaterial;
};

#undef UE_API
