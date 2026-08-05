#include "Hero/YanHeroInputComponent.h"

#include "InputActionValue.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "ActorComponent/ModularAbilitySystemComponent.h"
#include "ActorComponent/ModularInputConfigComponent.h"
#include "ModularGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(YanHeroInputComponent)


UYanHeroInputComponent::UYanHeroInputComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UYanHeroInputComponent::InitializePlayerInput(UInputComponent* PlayerInputComponent)
{
	// 执行基类的 MappingContext 注册与 NativeAction 绑定（Move / Look 等）
	Super::InitializePlayerInput(PlayerInputComponent);

	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return;
	}

	// 注册自身为 Mover 基础移动 InputProducer（写入方向/朝向/控制器旋转）。
	// 该注册与 InputConfig 无关：PawnData 未配置输入 Fragment 时移动仍须可用。
	// UInputComponent 晚于 UMoverComponent::BeginPlay 创建，BeginPlay 扫描无法找到本组件，需在此手动注册。
	if (UMoverComponent* MoverComp = Pawn->FindComponentByClass<UMoverComponent>())
	{
		MoverComp->InputProducers.AddUnique(this);
	}
}

void UYanHeroInputComponent::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return;
	}

	FCharacterDefaultInputs& CharInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

	// 消费本帧 AddMovementInput 累积的方向向量（Input_Move 已按控制器 Yaw 旋转为世界空间）
	const FVector MoveInput = const_cast<APawn*>(Pawn)->ConsumeMovementInputVector();
	CharInputs.SetMoveInput(EMoveInputType::DirectionalIntent, MoveInput);

	// 传递控制器旋转，供移动模式使用
	FRotator ControlRotation = FRotator::ZeroRotator;
	if (const AController* Controller = Pawn->GetController())
	{
		ControlRotation = Controller->GetControlRotation();
		CharInputs.ControlRotation = ControlRotation;
	}

	// 按朝向模式写入 OrientationIntent（世界空间单位向量）
	switch (OrientationMode)
	{
	case EYanOrientationMode::FaceCamera:
		{
			// 面朝控制器 Yaw 前向：身体恒朝镜头，后退/横移时不转身（Strafe）
			const FRotator YawRotation(0.0, ControlRotation.Yaw, 0.0);
			CharInputs.OrientationIntent = YawRotation.Vector();
			break;
		}
	case EYanOrientationMode::FaceMovement:
		{
			// 面朝移动方向：零输入时保持上一帧朝向，避免回正抖动
			if (!MoveInput.IsNearlyZero())
			{
				CharInputs.OrientationIntent = MoveInput.GetSafeNormal();
			}
			break;
		}
	}
}

void UYanHeroInputComponent::SetOrientationMode(EYanOrientationMode NewMode)
{
	OrientationMode = NewMode;
}

void UYanHeroInputComponent::ToggleOrientationMode()
{
	OrientationMode = (OrientationMode == EYanOrientationMode::FaceCamera)
		? EYanOrientationMode::FaceMovement
		: EYanOrientationMode::FaceCamera;
}
