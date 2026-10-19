#pragma once

#include "Engine/DataAsset.h"

#include "YanAimSensitivityData.generated.h"

#define UE_API YANGAMESETTING_API

enum class EYanGamepadSensitivity : uint8;

/**
 * 手柄灵敏度档位到实际倍率的映射表。
 * 档位存档、倍率查表，使手感曲线可随时调整而不影响已有存档。
 */
UCLASS(MinimalAPI, BlueprintType, Const, meta = (DisplayName = "Yan Aim Sensitivity Data"))
class UYanAimSensitivityData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UE_API UYanAimSensitivityData(const FObjectInitializer& ObjectInitializer);

	/** 查表取倍率；档位缺失时回退为 1.0 */
	UE_API float SensitivityEnumToFloat(const EYanGamepadSensitivity InSensitivity) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<EYanGamepadSensitivity, float> SensitivityMap;
};

#undef UE_API
