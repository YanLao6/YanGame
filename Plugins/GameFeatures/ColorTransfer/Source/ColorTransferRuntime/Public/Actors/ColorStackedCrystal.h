#pragma once

#include "GameFramework/Actor.h"
#include "ColorTransferTypes.h"
#include "Interaction/IInteractableTarget.h"
#include "Interaction/InteractionOption.h"

#include "ColorStackedCrystal.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * 栈式晶体：射入的颜料按栈存放，从栈底起逐层点亮电量格，按 RequiredSequence 的顺序与颜色全部点亮即前往下一关。
 *
 * 每层颜料占一格：存量在 [MinAmountPerCell, MaxAmountPerCell] 内点亮，不足则不亮，
 * 超出则把下一格也点亮成同色（无论下一层有多少），使后续各层整体后移。
 * 玩家经交互技能每次从栈顶取出固定量到自己身上，用于纠错；须有组件对交互射线 Block 才能被扫到。
 * 电量格由蓝图组件提供，经组件引用绑定；电量格材质需带 Color 与 EmissiveStrength 参数。
 * 只在服务器上结算，颜料栈复制给各端驱动电量格显示与交互选项。
 */
UCLASS(MinimalAPI)
class AColorStackedCrystal : public AActor, public IColorShotReceiver, public IColorPigmentSource, public IInteractableTarget
{
	GENERATED_BODY()

public:
	UE_API AColorStackedCrystal();

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

	UFUNCTION(BlueprintPure, Category = "Yan|ColorTransfer")
	bool IsCompleted() const { return bCompleted; }

protected:
	//~Begin AActor Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End AActor Interface

	/** 电量格，从栈底到栈顶排列 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer", Meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.StaticMeshComponent"))
	TArray<FComponentReference> CellMeshes;

	/** InteractionAbilityToGrant 为取颜料技能，靠近时由交互技能授予玩家 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	FInteractionOption InteractionOption;

	/** 从栈底到栈顶须依次注入的颜色 */
	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer")
	TArray<EColorPigment> RequiredSequence = {EColorPigment::White, EColorPigment::Red, EColorPigment::Black, EColorPigment::Blue};

	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.0"))
	float MinAmountPerCell = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Yan|ColorTransfer", Meta = (ClampMin = "0.0"))
	float MaxAmountPerCell = 15.0f;

	UPROPERTY(EditInstanceOnly, Category = "Yan|ColorTransfer")
	TSoftObjectPtr<UWorld> NextLevel;

private:
	struct FCellState
	{
		EColorPigment Pigment = EColorPigment::None;
		bool bLit = false;
	};

	// 按颜料栈推算每个电量格的显示，服务器与客户端共用
	TArray<FCellState> EvaluateCells() const;

	bool IsAmountInRange(float Amount) const;

	void AuthCheckCompletion();
	void TravelToNextLevel();

	UFUNCTION()
	void OnRep_Visual();

	// 晶体内的颜料栈，末尾为栈顶
	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	TArray<FColorPigmentLayer> StoredLayers;

	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	bool bCompleted = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> CellMaterials;

	FTimerHandle TravelTimer;
};

#undef UE_API
