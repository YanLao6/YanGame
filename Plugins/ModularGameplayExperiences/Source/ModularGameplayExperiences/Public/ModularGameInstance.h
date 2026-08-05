// Copyright Chronicler.


#pragma once
#include "CommonGameInstance.h"

#include "ModularGameInstance.generated.h"

#define UE_API MODULARGAMEPLAYEXPERIENCES_API

/**
 * Modular GameInstance 基类。
 *
 * 在 `UCommonGameInstance` 基础上补充 Experience/Session 相关的项目级通用能力。
 */
UCLASS(MinimalAPI, Config="Game")
class UModularGameInstance : public UCommonGameInstance
{
	GENERATED_BODY()

public:
	/** 构造 GameInstance。 */
	UE_API explicit UModularGameInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	/** 客户端准备加入会话前可修改 URL 参数。 */
	UE_API void OnPreClientTravelToSession(FString& URL);

public:
	/** 初始化 GameInstance 并注册运行期回调（@ingroup UCommonGameInstance）。 */
	UE_API virtual void Init() override;
};

#undef UE_API
