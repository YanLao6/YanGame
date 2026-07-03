// Copyright Chronicler.


#pragma once

#include "ModularExperienceDefinition.h"
#include "ModularWorldSettings.generated.h"

/**
 * WorldSettings 扩展类。
 *
 * 用于在地图层面配置默认 GameplayExperience 与附加 Action/ActionSet。
 */
UCLASS()
class MODULARGAMEPLAYEXPERIENCES_API AModularWorldSettings : public AWorldSettings
{
	GENERATED_BODY()

public:
	/** 构造 WorldSettings。 */
	explicit AModularWorldSettings(const FObjectInitializer& ObjectInitializer);

	/** 从任意 WorldContext 对象获取当前关卡的 ModularWorldSettings。 */
	static AModularWorldSettings* GetModularWorldSettings(const UObject* WorldContent);

public:
	/** 返回该关卡默认 GameplayExperience（可被用户侧选择覆盖）。 */
	FPrimaryAssetId GetDefaultGameplayExperience() const;

protected:
	/**
	 * 服务器打开此地图时默认使用的 Experience。
	 * 若存在用户侧体验选择，则可覆盖此默认值。
	 */
	UPROPERTY(EditDefaultsOnly, Category="GameMode")
	TSoftClassPtr<UModularExperienceDefinition> DefaultGameplayExperience;
	
public:
	/**
	* 是否启用GameplayExperience功能
	* 控制当前World是否启用GameplayExperience功能。设置为false时，
	* GameMode不会尝试加载Experience。
	*/
	UPROPERTY(EditDefaultsOnly, Category=GameMode)
	bool bEnableGameplayExperience = true;
	
	/**
	 * 直接配置在WorldSettings上的GameFeatureAction列表
	 * 用于在地图上快速添加功能以便测试。对于正式的游戏内容制作，
	 * 这些Action会在Experience加载时执行。
	 */
	UPROPERTY(EditDefaultsOnly, Instanced, Category=GameMode)
	TArray<TObjectPtr<UGameFeatureAction>> Actions;

	/**
	 * 直接配置在WorldSettings上的ActionSet列表
	 * 用于在地图上快速添加功能集合以便测试。对于正式的游戏内容制作，
	 * 这些ActionSet会在Experience加载时合并到Experience中。
	 */
	UPROPERTY(EditDefaultsOnly, Category=GameMode)
	TArray<TObjectPtr<UModularExperienceActionSet>> ActionSets;
};
