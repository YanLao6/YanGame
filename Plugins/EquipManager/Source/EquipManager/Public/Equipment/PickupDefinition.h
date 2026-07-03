// 拾取物定义声明。

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PickupDefinition.generated.h"

#define UE_API EQUIPMANAGER_API

class UNiagaraSystem;
class UInventoryItemDefinition;
/**
 * 拾取物定义资源。
 *
 * 用于描述场景中可拾取对象对应的物品定义、展示网格，以及拾取和刷新时的表现资源。
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, Const, Meta = (DisplayName = "Lyra Pickup Data", ShortTooltip = "Data asset used to configure a pickup."))
class UPickupDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 成功拾取后要加入背包的物品定义。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EM|Pickup|Equipment")
	TSubclassOf<UInventoryItemDefinition> InventoryItemDefinition;

	/** 拾取物在场景中的静态网格显示资源。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EM|Pickup|Mesh")
	TObjectPtr<UStaticMesh> DisplayMesh;

	/** 被拾取后再次刷新的冷却时间，单位为秒。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EM|Pickup")
	int32 SpawnCoolDownSeconds;

	/** 拾取成功时播放的声音资源。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EM|Pickup")
	TObjectPtr<USoundBase> PickedUpSound;

	/** 拾取物刷新回场景时播放的声音资源。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EM|Pickup")
	TObjectPtr<USoundBase> RespawnedSound;

	/** 拾取成功时播放的 Niagara 特效。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EM|Pickup")
	TObjectPtr<UNiagaraSystem> PickedUpEffect;

	/** 拾取物刷新回场景时播放的 Niagara 特效。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EM|Pickup")
	TObjectPtr<UNiagaraSystem> RespawnedEffect;
};


/**
 * 武器拾取物定义。
 *
 * 在通用拾取物配置基础上，额外提供武器展示网格的偏移与缩放参数。
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, Const, Meta = (DisplayName = " Weapon Pickup Data", ShortTooltip = "Data asset used to configure a weapon pickup."))
class UWeaponPickupDefinition : public UPickupDefinition
{
	GENERATED_BODY()

public:
	/** 武器展示网格相对于生成点的局部偏移。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EM|Pickup|Mesh")
	FVector WeaponMeshOffset;

	/** 武器展示网格的局部缩放倍率。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EM|Pickup|Mesh")
	FVector WeaponMeshScale = FVector(1.0f, 1.0f, 1.0f);
};

#undef UE_API
