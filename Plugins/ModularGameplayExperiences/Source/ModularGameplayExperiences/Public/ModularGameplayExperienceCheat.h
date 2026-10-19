#pragma once

#include "GameFramework/CheatManager.h"
#include "ModularGameplayExperienceCheat.generated.h"

/** Experience 调试命令，随 CheatManager 自动注册。 */
UCLASS()
class MODULARGAMEPLAYEXPERIENCES_API UModularGameplayExperienceCheat : public UCheatManagerExtension
{
	GENERATED_BODY()

public:
	/** 在服务器上无缝重开当前地图，保留当前 Experience。 */
	UFUNCTION(Exec, Category="Yan|Experience")
	void PlayNextGame();

	/** 在服务器上切往指定地图，并使用目标地图配置的 Experience。 */
	UFUNCTION(Exec, Category="Yan|Experience")
	void ExperienceLevelTravel(const FString& MapPath);

	/** 输出当前 World 的加载状态及已加载 Experience 的资产标识。 */
	UFUNCTION(Exec, Category="Yan|Experience")
	void DumpExperience();
};
