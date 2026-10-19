// 射击玩法运行时设置声明。

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "YanShooterRuntimeSettings.generated.h"

/** 射击玩法插件的运行时设置。 */
UCLASS(MinimalAPI, config = Game, defaultconfig)
class UYanShooterRuntimeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UYanShooterRuntimeSettings(const FObjectInitializer& Initializer);

	/** 返回瞄准辅助搜寻目标所用的检测通道。 */
	ECollisionChannel GetAimAssistCollisionChannel() const { return AimAssistCollisionChannel; }

private:
	/** 瞄准辅助搜寻可见目标时使用的检测通道。 */
	UPROPERTY(config, EditAnywhere, Category = "Aim Assist")
	TEnumAsByte<ECollisionChannel> AimAssistCollisionChannel = ECollisionChannel::ECC_EngineTraceChannel5;
};
