// Copyright YanLao.

#pragma once

#include "Engine/DeveloperSettings.h"

#include "YanSessionSettings.generated.h"

class UModularUserFacingExperienceDefinition;

/**
 * 会话流程设置。
 */
UCLASS(MinimalAPI, Config = Game, DefaultConfig, Meta = (DisplayName = "Yan 会话设置"))
class UYanSessionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	//~Begin UDeveloperSettings Interface
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	//~End UDeveloperSettings Interface

	/**
	 * 联机开房时先进入的等待大厅。
	 * 玩家在大厅内选队、准备，由房主或全员准备后再切往所选对局；留空则开房直接进入对局。
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Lobby")
	TSoftObjectPtr<UModularUserFacingExperienceDefinition> LobbyExperience;
};
