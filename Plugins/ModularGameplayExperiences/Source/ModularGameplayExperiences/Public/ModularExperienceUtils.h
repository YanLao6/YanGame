// Copyright Chronicler.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InputCoreTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ModularExperienceUtils.generated.h"

#define UE_API MODULARGAMEPLAYEXPERIENCES_API

class APlayerController;

/**
 * Experience 运行时查询辅助。
 *
 * 收拢需要沿 Experience 内部数据链路（InputConfig → EnhancedInput）才能得出的只读查询，
 * 使上层无需了解该链路的层级与资产结构。
 */
UCLASS(MinimalAPI)
class UModularExperienceUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 查询 InputTag 在该玩家当前实际绑定到的按键。
	 *
	 * 经 Pawn 输入组件上全部已生效的 InputConfig 把 Tag 解析为 UInputAction（Ability 表优先于 Native 表），
	 * 再由 EnhancedInput 取其当前映射按键，并按当前输入设备（键鼠/手柄）优选其一。
	 * Tag 未配置、Pawn 尚未就绪或该 Action 无任何映射时返回无效按键（FKey::IsValid 为 false）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Experience|Input")
	static UE_API FKey GetKeyValueByTag(const APlayerController* PlayerController, FGameplayTag InputTag);
};

#undef UE_API
