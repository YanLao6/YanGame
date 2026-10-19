#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "YanMusicSubsystem.generated.h"

#define UE_API YANGAMEEFFECTS_API

class UAudioComponent;

/**
 * 背景音乐的跨关卡寄存点。
 *
 * 音乐组件必须活得比关卡长，否则每次切图都会从头重新起播。
 * 挂在 GameInstance 上而非组件自身，是因为音乐管理器组件随关卡一同销毁。
 */
UCLASS(MinimalAPI)
class UYanMusicSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 当前的背景音乐组件；首次由音乐管理器创建后寄存于此，后续关卡直接复用。 */
	UPROPERTY(BlueprintReadWrite, Category = "Yan|Music")
	TObjectPtr<UAudioComponent> MusicSystem;
};

#undef UE_API
