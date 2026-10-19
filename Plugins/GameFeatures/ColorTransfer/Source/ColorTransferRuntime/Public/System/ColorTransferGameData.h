#pragma once

#include "Engine/DataAsset.h"

#include "ColorTransferGameData.generated.h"

#define UE_API COLORTRANSFERRUNTIME_API

class UGameplayEffect;

/**
 * 颜料玩法的全局数据：颜料 GE 的引用集中于此，C++ 侧经 Get() 取用，不在代码里写死资产。
 *
 * 资产路径由 UColorTransferSettings::GameDataPath 指定；打包依赖 GameFeatureData 中
 * 以 AlwaysCook 登记的 ColorTransferGameData 主资产类型。缺失即 Fatal，与项目其他 GameData 一致。
 */
UCLASS(MinimalAPI, BlueprintType, Const, Meta = (DisplayName = "Color Transfer Game Data"))
class UColorTransferGameData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static UE_API const UColorTransferGameData& Get();

	/** 注入颜料，须带 UColorPigmentInjectExecution；颜色与注入量经颜色 SetByCaller 传入。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Pigment")
	TSoftClassPtr<UGameplayEffect> InjectEffect;

	/** 射击消耗，修改 ShotConsumption，量为 SetByCaller.ColorPigment.Amount。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Pigment")
	TSoftClassPtr<UGameplayEffect> ShotConsumptionEffect;

	/** 外来颜料衰减，修改 ForeignDecay，量为 SetByCaller.ColorPigment.Amount。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Pigment")
	TSoftClassPtr<UGameplayEffect> ForeignDecayEffect;

	/** 重置颜料，修改 PigmentReset（量无意义）；自身颜色取值为正的颜色 SetByCaller。 */
	UPROPERTY(EditDefaultsOnly, Category = "Yan|Pigment")
	TSoftClassPtr<UGameplayEffect> ResetEffect;
};

#undef UE_API
