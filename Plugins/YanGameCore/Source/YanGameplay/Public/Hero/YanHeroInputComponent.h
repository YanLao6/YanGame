#pragma once

#include "CoreMinimal.h"
#include "ActorComponent/ModularInputComponent.h"
#include "MoverSimulationTypes.h"
#include "YanHeroInputComponent.generated.h"

/**
 * 角色朝向模式：决定 ProduceInput 写入 Mover 的 OrientationIntent（世界空间）。
 *
 * ChaosMover 不使用 Actor 的 bUseControllerRotationYaw / bOrientRotationToMovement，
 * 旋转完全由仿真状态按 OrientationIntent 驱动，故朝向风格在此层切换。
 */
UENUM(BlueprintType)
enum class EYanOrientationMode : uint8
{
	/** 面朝控制器（镜头）Yaw 方向：支持后退/横移而身体不转身（Strafe）。 */
	FaceCamera,
	/** 面朝移动方向：转向时身体随之转动，无法原地后退。 */
	FaceMovement
};

/**
 * 英雄角色专用输入组件。
 *
 * 继承 UModularInputComponent 的 Enhanced Input 绑定流程，
 * 并实现 IMoverInputProducerInterface：
 * 每帧由 UMoverComponent 调用 ProduceInput，将移动意图（方向、朝向、控制器旋转）
 * 写入 FCharacterDefaultInputs，驱动 Mover 物理模拟。
 *
 * 技能性输入（跳跃、冲刺、滑铲、钩锁等）路由至 UModularAbilitySystemComponent，
 * 由 GAS ProcessAbilityInput 驱动 UMoverInputAbility 的激活与结束。
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class YANGAMEPLAY_API UYanHeroInputComponent : public UModularInputComponent, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	UYanHeroInputComponent(const FObjectInitializer& ObjectInitializer);

	//~Begin IMoverInputProducerInterface
	/** 将本帧基础移动输入（方向/朝向/控制旋转）写入 Mover 命令包。 */
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;
	//~End IMoverInputProducerInterface

	/** 设置朝向模式（运行时可切换）。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Orientation")
	void SetOrientationMode(EYanOrientationMode NewMode);

	/** 在面朝镜头与面朝移动方向之间切换。 */
	UFUNCTION(BlueprintCallable, Category = "Yan|Orientation")
	void ToggleOrientationMode();

	/** 当前朝向模式，决定 ProduceInput 写入的 OrientationIntent。默认面朝镜头（可后退/横移）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yan|Orientation")
	EYanOrientationMode OrientationMode = EYanOrientationMode::FaceCamera;

protected:
	/** 将技能输入路由至 ASC，并注册自身为 MoverComponent 的基础移动 InputProducer。 */
	virtual void InitializePlayerInput(UInputComponent* PlayerInputComponent) override;
};
