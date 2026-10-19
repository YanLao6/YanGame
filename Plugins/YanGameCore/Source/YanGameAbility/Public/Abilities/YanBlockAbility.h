#pragma once

#include "CoreMinimal.h"
#include "Abilities/YanGameplayAbility.h"
#include "NativeGameplayTags.h"

#include "YanBlockAbility.generated.h"

#define UE_API YANGAMEABILITY_API

/** 格挡姿态：持有期间正面来袭的击飞被抵消。由击飞 Execution 读取。 */
YANGAMEABILITY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Blocking);

/**
 * 格挡技能：按住进入格挡姿态，松手退出。
 *
 * 技能本身不做任何判定，只负责在持续期间挂上 Status.Blocking；
 * 是否格挡成功由击飞 Execution 在服务器上按来袭方向裁定，
 * 抵消击飞与回复机动能量都发生在那一侧。
 *
 * 判定放在受击方而非格挡方，是因为击飞的方向只有 Execution 才知道；
 * 姿态做成按住而非短窗口，则是为了让联机下的激活延迟不影响成败。
 */
UCLASS(MinimalAPI)
class UYanBlockAbility : public UYanGameplayAbility
{
	GENERATED_BODY()

public:
	UE_API UYanBlockAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~Begin UYanGameplayAbility Interface
	UE_API virtual void K2_InputReleased_Implementation() override;
	//~End UYanGameplayAbility Interface
};

#undef UE_API
