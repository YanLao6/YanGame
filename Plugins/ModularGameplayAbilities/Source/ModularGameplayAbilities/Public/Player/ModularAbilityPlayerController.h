// 版权声明请在「项目设置 → 描述」中填写。

#pragma once

#include "CoreMinimal.h"
#include "CommonPlayerController.h"
#include "ModularPlayerController.h"
#include "ModularAbilityPlayerController.generated.h"

#define UE_API MODULARGAMEPLAYABILITIES_API

class AModularAbilityPlayerState;
class AModularPlayerState;
class UModularAbilitySystemComponent;

/**
 * 提供 PlayerState / ASC 访问，并在 PostProcessInput 中处理 Ability Input 的 PlayerController。
 */
UCLASS(MinimalAPI)
class AModularAbilityPlayerController : public ACommonPlayerController
{
	GENERATED_BODY()

public:
	/** 取得 AModularAbilityPlayerState（可为 nullptr）。 */
	UFUNCTION(BlueprintCallable, Category = "Lyra|PlayerController")
	UE_API AModularAbilityPlayerState* GetModularPlayerState() const;

	/** 取得 PlayerState 上的 UModularAbilitySystemComponent。 */
	UFUNCTION(BlueprintCallable, Category = "Lyra|PlayerController")
	UE_API UModularAbilitySystemComponent* GetModularAbilitySystemComponent() const;

	//~APlayerController 接口
	UE_API virtual void PreProcessInput(const float DeltaTime, const bool bGamePaused) override;
	/** 在父类之后调用 ASC::ProcessAbilityInput。 */
	UE_API virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;
	//~APlayerController 接口结束
};

#undef UE_API
