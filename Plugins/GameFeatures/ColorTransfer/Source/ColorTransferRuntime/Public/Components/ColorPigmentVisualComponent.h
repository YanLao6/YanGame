#pragma once

#include "Components/PawnComponent.h"

#include "ColorPigmentVisualComponent.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class UColorPigmentSet;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UModularPawnComponent_CharacterParts;
class USkeletalMeshComponent;

/**
 * 把身上的颜料以流体层的形式画在角色表面。
 *
 * 颜料从右手食指向全身扩散：离指尖最近的是栈顶外来色（即下一发会射出的颜色），
 * 往外依次是更早注入的外来色，最外层是自身颜色；进入紫爆后整体渐变为紫色。
 *
 * 以 Overlay 材质叠在 Pawn 与外观部件的全部骨骼网格上，不改动原有材质，因此不影响队伍配色逻辑。
 * 只在有画面的端运行，专用服务器上不 Tick。
 */
UCLASS(MinimalAPI, ClassGroup = "Yan|ColorTransfer", Meta = (BlueprintSpawnableComponent))
class UColorPigmentVisualComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	UE_API explicit UColorPigmentVisualComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UActorComponent Interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UE_API virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~End UActorComponent Interface

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	TSoftObjectPtr<UMaterialInterface> OverlayMaterial;

	/** 颜料扩散的起点骨骼。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|ColorTransfer")
	FName SpreadOriginBone = TEXT("index_03_r");

private:
	UFUNCTION()
	void OnCharacterPartsChanged(UModularPawnComponent_CharacterParts* CharacterParts);

	// 重新收集要铺颜料层的网格并挂上 Overlay
	void ApplyOverlayToMeshes();
	void ClearOverlayFromMeshes();

	void UpdateMaterialParameters(const UColorPigmentSet& PigmentSet);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OverlayMID;

	TArray<TWeakObjectPtr<USkeletalMeshComponent>> OverlaidMeshes;
};

#undef UE_API
